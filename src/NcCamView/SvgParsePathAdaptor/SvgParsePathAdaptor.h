/**
 * @file SvgParsePathAdaptor.h
 *
 * SVG importer. Parses an SVG file with nanosvg (which reduces every SVG
 * element -- path, rect, circle, ellipse, line, polyline, polygon, arcs and
 * transforms -- to flattened cubic-bezier polylines), tessellates those beziers
 * to line segments, and hands the resulting contours to the shared
 * path_import::buildAndPushPart() to produce a Part identical in shape to what
 * the DXF importer produces.
 *
 * Notes / limitations (documented for users):
 *  - nanosvg ignores <text>; text must be converted to paths in the design tool.
 *  - nanosvg flattens <g> groups, so SVG layers are not preserved; all geometry
 *    lands on a single import layer.
 */

#ifndef SvgParsePathAdaptor_
#define SvgParsePathAdaptor_

#include <NcRender/NcRender.h>
#include <atomic>
#include <functional>
#include <string>
#include <vector>

// Forward declarations
class NcCamView;

class SvgParsePathAdaptor {
public:
  SvgParsePathAdaptor(
    NcRender*                       nc_render_instance,
    std::function<void(Primitive*)> view_callback,
    std::function<void(Primitive*, const Primitive::MouseEventData&)>
               mouse_callback,
    NcCamView* cam_view);

  void setFilename(std::string f);
  void setImportScale(double scale);
  void setImportQuality(int quality);
  void setSmoothing(float smoothing);
  void setChainTolerance(double chain_tolerance);
  void setProgressTracking(std::atomic<float>* progress);

  // Parse the SVG file and tessellate its geometry into contours. Returns false
  // if the file cannot be parsed. Safe to call off the main thread.
  bool parse(const std::string& path);

  // Build the Part primitive from the parsed contours and push it into the
  // renderer. Mirrors DXFParsePathAdaptor::finish().
  void finish();

  std::string m_filename;

private:
  // "Import resolution" slider (1..10) as a chord tolerance in mm.
  double sampleToleranceMm() const;

  NcRender*                       m_nc_render_instance;
  std::function<void(Primitive*)> m_view_callback;
  std::function<void(Primitive*, const Primitive::MouseEventData&)>
             m_mouse_callback;
  NcCamView* m_cam_view;

  float               m_simplification = 0.02f;
  double              m_import_scale = 1.0;
  int                 m_import_quality = 5;
  double              m_chain_tolerance = 0.25;
  std::atomic<float>* m_progress_ptr = nullptr;

  // One ordered contour per parsed SVG subpath (already in mm, Y-up).
  std::vector<std::vector<Point2d>> m_contours;
};

#endif
