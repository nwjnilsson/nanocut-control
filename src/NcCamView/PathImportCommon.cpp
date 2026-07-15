#include "PathImportCommon.h"
#include "NcApp/NcApp.h"
#include "NcCamView/NcCamView.h"
#include <loguru.hpp>

namespace {

// True if ANY point of path1 lies inside path2 (partial containment). This is
// the same predicate the DXF importer historically used for even-odd nesting.
bool pathIsInsidePath(const std::vector<Point2d>& path1,
                      const std::vector<Point2d>& path2)
{
  for (const auto& pt : path1) {
    if (geo::pointIsInsidePolygon(path2, pt)) {
      return true;
    }
  }
  return false;
}

} // namespace

namespace path_import {

Part* buildAndPushPart(
  NcRender*                       render,
  NcCamView*                      cam_view,
  const std::string&              name,
  float                           smoothing,
  std::function<void(Primitive*)> view_callback,
  std::function<void(Primitive*, const Primitive::MouseEventData&)>
                                           mouse_callback,
  const std::vector<std::vector<Point2d>>& all_chains,
  const std::vector<std::string>&          chain_layers,
  double                                   chain_tolerance)
{
  // Global bounding box across every contour, so the part can be re-based to
  // the origin (all coordinates become >= 0).
  Point2d bb_min, bb_max;
  bb_max.x = -std::numeric_limits<double>::infinity();
  bb_max.y = -std::numeric_limits<double>::infinity();
  bb_min.x = std::numeric_limits<double>::infinity();
  bb_min.y = std::numeric_limits<double>::infinity();
  for (const auto& chain : all_chains) {
    for (const auto& point : chain) {
      bb_min.x = std::min(point.x, bb_min.x);
      bb_max.x = std::max(point.x, bb_max.x);
      bb_min.y = std::min(point.y, bb_min.y);
      bb_max.y = std::max(point.y, bb_max.y);
    }
  }

  // Create layer-organized structure
  std::unordered_map<std::string, Part::Layer> part_layers;

  // Create paths from all chains, organized by layer
  for (size_t i = 0; i < all_chains.size(); i++) {
    if (all_chains[i].empty())
      continue;

    Part::path_t path;
    path.is_inside_contour = false;

    path.is_closed = geo::distance(all_chains[i].front(),
                                   all_chains[i].back()) <= chain_tolerance;

    path.color = &cam_view->m_app->getColor(cam_view->m_outside_contour_color);

    // Adjust coordinates relative to bounding box
    for (size_t j = 0; j < all_chains[i].size(); j++) {
      const double px = all_chains[i][j].x - bb_min.x;
      const double py = all_chains[i][j].y - bb_min.y;
      path.points.push_back({ px, py });
    }

    // Add path to appropriate layer
    const std::string& layer_name = chain_layers[i];
    part_layers[layer_name].paths.push_back(path);
  }

  // Determine inside/outside contours (check across ALL layers)
  // Create temporary flat list for containment checking
  struct PathRef {
    std::string  layer_name;
    size_t       path_index;
    bool         is_closed;
    geo::Extents bbox;
  };
  std::vector<PathRef> path_refs;

  for (auto& [layer_name, layer] : part_layers) {
    for (size_t i = 0; i < layer.paths.size(); i++) {
      PathRef ref;
      ref.layer_name = layer_name;
      ref.path_index = i;
      ref.is_closed = layer.paths[i].is_closed;
      ref.bbox = geo::calculateBoundingBox(layer.paths[i].points);

      path_refs.push_back(ref);
    }
  }

  for (size_t x = 0; x < path_refs.size(); x++) {
    auto& path_x =
      part_layers[path_refs[x].layer_name].paths[path_refs[x].path_index];

    size_t containing_path_count = 0;

    for (size_t i = 0; i < path_refs.size(); i++) {
      if (i == x)
        continue;

      // Skip non-closed paths - they can't contain other paths
      if (!path_refs[i].is_closed)
        continue;

      // Bounding box pre-check: if path_x's bbox isn't inside path_i's bbox,
      // skip expensive check
      if (!geo::extentsContain(path_refs[i].bbox, path_refs[x].bbox)) {
        continue;
      }

      auto& path_i =
        part_layers[path_refs[i].layer_name].paths[path_refs[i].path_index];

      if (pathIsInsidePath(path_x.points, path_i.points)) {
        containing_path_count++;
      }
    }

    // Even-odd rule: inside if contained by an odd number of closed contours
    path_x.is_inside_contour = (containing_path_count % 2) == 1;
    if (path_x.is_inside_contour) {
      if (path_x.is_closed) {
        path_x.color =
          &cam_view->m_app->getColor(cam_view->m_inside_contour_color);
      }
      else {
        path_x.color =
          &cam_view->m_app->getColor(cam_view->m_open_contour_color);
      }
    }
    else {
      path_x.color =
        &cam_view->m_app->getColor(cam_view->m_outside_contour_color);
    }
  }

  size_t total_paths = 0;
  for (const auto& [layer_name, layer] : part_layers) {
    total_paths += layer.paths.size();
  }

  LOG_F(INFO,
        "(path_import::buildAndPushPart) Created %lu paths from %lu layers",
        total_paths,
        part_layers.size());

  // Create the final primitive with layer-organized structure
  Part* p = render->pushPrimitive<Part>(name, std::move(part_layers));
  p->m_control.smoothing = smoothing;
  p->m_control.scale = 1.0;
  p->mouse_callback = mouse_callback;
  p->matrix_callback = view_callback;
  return p;
}

} // namespace path_import
