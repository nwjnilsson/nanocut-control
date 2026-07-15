#include "SvgParsePathAdaptor.h"
#include "NcCamView/PathImportCommon.h"
#include <algorithm>
#include <cmath>
#include <loguru.hpp>

// nanosvg is a single-header library; instantiate its implementation here (this
// is the only translation unit that defines NANOSVG_IMPLEMENTATION).
#define NANOSVG_IMPLEMENTATION
#include <nanosvg/nanosvg.h>

namespace {

// Single import layer name (nanosvg flattens SVG groups, so there are no real
// layer names to preserve).
constexpr const char* kSvgLayer = "svg";

// Perpendicular distance from p to the segment a->b, degenerating to the plain
// distance from a when the segment has ~zero length (so a bezier whose two
// endpoints coincide -- e.g. a closing loop -- doesn't subdivide forever).
double perpDistance(const Point2d& p, const Point2d& a, const Point2d& b)
{
  const double dx = b.x - a.x;
  const double dy = b.y - a.y;
  const double len2 = dx * dx + dy * dy;
  if (len2 < 1e-12) {
    const double ex = p.x - a.x;
    const double ey = p.y - a.y;
    return std::sqrt(ex * ex + ey * ey);
  }
  const double t = ((p.x - a.x) * dx + (p.y - a.y) * dy) / len2;
  const double projx = a.x + t * dx;
  const double projy = a.y + t * dy;
  const double ex = p.x - projx;
  const double ey = p.y - projy;
  return std::sqrt(ex * ex + ey * ey);
}

// Adaptive flattening of one cubic bezier into line segments. The start point
// is assumed already present in `out`; only intermediate points and the end
// point are appended. Flat when both control points lie within `tol` (mm) of
// the chord. Depth-limited as a backstop against degenerate (looping) segments.
void flattenCubic(std::vector<Point2d>& out,
                  const Point2d&        p0,
                  const Point2d&        p1,
                  const Point2d&        p2,
                  const Point2d&        p3,
                  double                tol,
                  int                   level)
{
  if (level >= 16 || (perpDistance(p1, p0, p3) <= tol &&
                      perpDistance(p2, p0, p3) <= tol)) {
    out.push_back(p3);
    return;
  }

  // de Casteljau subdivision at t = 0.5
  const Point2d p01{ (p0.x + p1.x) * 0.5, (p0.y + p1.y) * 0.5 };
  const Point2d p12{ (p1.x + p2.x) * 0.5, (p1.y + p2.y) * 0.5 };
  const Point2d p23{ (p2.x + p3.x) * 0.5, (p2.y + p3.y) * 0.5 };
  const Point2d p012{ (p01.x + p12.x) * 0.5, (p01.y + p12.y) * 0.5 };
  const Point2d p123{ (p12.x + p23.x) * 0.5, (p12.y + p23.y) * 0.5 };
  const Point2d mid{ (p012.x + p123.x) * 0.5, (p012.y + p123.y) * 0.5 };

  flattenCubic(out, p0, p01, p012, mid, tol, level + 1);
  flattenCubic(out, mid, p123, p23, p3, tol, level + 1);
}

} // namespace

SvgParsePathAdaptor::SvgParsePathAdaptor(
  NcRender*                       nc_render_instance,
  std::function<void(Primitive*)> view_callback,
  std::function<void(Primitive*, const Primitive::MouseEventData&)>
             mouse_callback,
  NcCamView* cam_view)
  : m_nc_render_instance(nc_render_instance)
  , m_view_callback(view_callback)
  , m_mouse_callback(mouse_callback)
  , m_cam_view(cam_view)
{
}

void SvgParsePathAdaptor::setFilename(std::string f) { m_filename = f; }
void SvgParsePathAdaptor::setImportScale(double scale) { m_import_scale = scale; }
void SvgParsePathAdaptor::setImportQuality(int quality)
{
  m_import_quality = quality;
}
void SvgParsePathAdaptor::setSmoothing(float smoothing)
{
  m_simplification = smoothing;
}
void SvgParsePathAdaptor::setChainTolerance(double chain_tolerance)
{
  m_chain_tolerance = chain_tolerance;
}
void SvgParsePathAdaptor::setProgressTracking(std::atomic<float>* progress)
{
  m_progress_ptr = progress;
}

double SvgParsePathAdaptor::sampleToleranceMm() const
{
  const int r = std::max(1, m_import_quality);
  return 1.0 / static_cast<double>(r * r); // 1.0 mm (coarse) .. 0.01 mm (fine)
}

bool SvgParsePathAdaptor::parse(const std::string& path)
{
  m_contours.clear();

  // Parse in mm so an SVG authored in physical units imports ~1:1; px/viewBox
  // documents come through in px and rely on the Scale slider, exactly like the
  // DXF importer.
  NSVGimage* image = nsvgParseFromFile(path.c_str(), "mm", 96.0f);
  if (!image) {
    LOG_F(ERROR, "(SvgParsePathAdaptor::parse) Failed to parse %s", path.c_str());
    return false;
  }

  const double tol = sampleToleranceMm();
  const double scale = m_import_scale;

  // SVG uses a Y-down coordinate system with the origin at the top-left;
  // NanoCut is Y-up. Flip Y (and apply scale) as each control point is mapped
  // into mm space, then flatten there so the tolerance stays in mm.
  auto toMm = [scale](float x, float y) -> Point2d {
    return { static_cast<double>(x) * scale, -static_cast<double>(y) * scale };
  };

  // Count shapes for progress reporting.
  size_t shape_count = 0;
  for (NSVGshape* s = image->shapes; s; s = s->next)
    shape_count++;
  if (m_progress_ptr)
    m_progress_ptr->store(0.f);

  size_t shape_index = 0;
  for (NSVGshape* shape = image->shapes; shape; shape = shape->next) {
    ++shape_index;
    if (m_progress_ptr && shape_count > 0) {
      m_progress_ptr->store(static_cast<float>(shape_index) /
                            static_cast<float>(shape_count));
    }

    // Skip hidden shapes (display:none / visibility:hidden).
    if (!(shape->flags & NSVG_FLAGS_VISIBLE))
      continue;

    for (NSVGpath* p = shape->paths; p; p = p->next) {
      if (p->npts < 1)
        continue;

      std::vector<Point2d> contour;
      contour.push_back(toMm(p->pts[0], p->pts[1]));

      // pts is [x0,y0, (c1x,c1y,c2x,c2y,x1,y1) ...]; consecutive cubic beziers
      // share endpoints, so step 3 points (6 floats) per segment.
      for (int i = 0; i + 3 < p->npts; i += 3) {
        const float* c = &p->pts[i * 2];
        const Point2d p0 = toMm(c[0], c[1]);
        const Point2d p1 = toMm(c[2], c[3]);
        const Point2d p2 = toMm(c[4], c[5]);
        const Point2d p3 = toMm(c[6], c[7]);
        flattenCubic(contour, p0, p1, p2, p3, tol, 0);
      }

      // nanosvg's `closed` flag means "treat as closed" but the point list is
      // not necessarily closed; make it explicit so the shared builder detects
      // closure and containment works.
      if (p->closed && contour.size() > 1) {
        const Point2d& a = contour.front();
        const Point2d& b = contour.back();
        const double   ddx = a.x - b.x;
        const double   ddy = a.y - b.y;
        if (std::sqrt(ddx * ddx + ddy * ddy) > m_chain_tolerance)
          contour.push_back(a);
      }

      if (contour.size() > 1)
        m_contours.push_back(std::move(contour));
    }
  }

  nsvgDelete(image);

  LOG_F(INFO,
        "(SvgParsePathAdaptor::parse) Parsed %s: %lu contours",
        path.c_str(),
        m_contours.size());
  return true;
}

void SvgParsePathAdaptor::finish()
{
  if (m_contours.empty()) {
    LOG_F(WARNING, "(SvgParsePathAdaptor::finish) No geometry to import");
  }

  // All SVG geometry lands on a single layer (groups are flattened by nanosvg).
  std::vector<std::string> chain_layers(m_contours.size(), kSvgLayer);

  path_import::buildAndPushPart(m_nc_render_instance,
                                m_cam_view,
                                m_filename,
                                m_simplification,
                                m_view_callback,
                                m_mouse_callback,
                                m_contours,
                                chain_layers,
                                m_chain_tolerance);
}
