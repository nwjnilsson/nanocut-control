#ifndef PART_
#define PART_

#include "../../geometry/geometry.h"
#include "../Primitive.h"
#include <NanoCut.h>
#include <string>
#include <unordered_map>
#include <vector>

class Part : public Primitive {
public:
  struct path_t {
    std::vector<Point2d> points;
    std::vector<Point2d> simplified_points; // Cached simplification result
    std::vector<Point2d> built_points;
    geo::Extents         bbox;              // Bounding box of built_points
    bool                 is_closed;
    bool                 is_inside_contour;
    // Manual cut-direction override. By default toolpaths run in the
    // plasma-quality direction (external profiles clockwise, holes
    // counter-clockwise); this flag flips that default for a single path.
    // Applied to the offset contour AFTER Clipper (Clipper canonicalizes input
    // winding, so reversing the source points has no effect). Toggled from the
    // CAM context menu.
    bool                 reversed = false;
    const Color4f*       color = &Primitive::s_default_color;
  };

  // A built toolpath with explicit lead-in / lead-out metadata so the
  // gcode emitter knows where the contour proper starts and ends inside
  // `points`. contour_first = lead_in_count; contour_last =
  // points.size() - lead_out_count - 1.
  struct Toolpath {
    std::vector<Point2d> points;
    size_t               lead_in_count = 0;
    size_t               lead_out_count = 0;
    bool                 is_closed_contour = false;
    bool                 is_inside_contour = false;
    bool                 lead_in_is_arc = false;
    // Full kerf width in built-point space (== 2 * |layer.toolpath_offset|),
    // captured at build time so render() can draw the toolpath as a swath of
    // the actual cut width. 0 falls back to a thin centerline.
    double               kerf_width = 0.0;
  };

  struct Layer {
    std::vector<path_t> paths;
    double              toolpath_offset = 0.0;
    bool                toolpath_visible = false;
    bool                visible = true;
  };
  struct part_control_data_t {
    Point2d offset;
    double  lead_in_length;
    double  lead_out_length;
    double  scale;
    float   smoothing;
    double  angle;
    int     mouse_mode;

    bool operator==(const part_control_data_t& a) const
    {
      return (offset.x == a.offset.x && offset.y == a.offset.y &&
              lead_in_length == a.lead_in_length &&
              lead_out_length == a.lead_out_length && scale == a.scale &&
              smoothing == a.smoothing && angle == a.angle &&
              mouse_mode == a.mouse_mode);
    }
  };

  std::unordered_map<std::string, Layer> m_layers;
  std::vector<Toolpath>                  m_tool_paths;
  // One V-shaped arrow (3 points) per toolpath, indicating cut direction in
  // the preview. Rebuilt alongside m_tool_paths.
  std::vector<std::vector<Point2d>>      m_tool_path_arrows;
  // Toolpath preview colors, injected by NcCamView from the theme cache (Part
  // has no access to the ThemeManager itself). Cut = contour motion, Lead =
  // lead-ins/lead-outs, Arrow = direction arrows. Default to static white so a
  // Part rendered before wiring is still visible (never null).
  const Color4f*                         m_toolpath_cut_color =
    &Primitive::s_default_color;
  const Color4f*                         m_toolpath_lead_color =
    &Primitive::s_default_color;
  const Color4f*                         m_toolpath_arrow_color =
    &Primitive::s_default_color;
  // When true, toolpaths render as a filled swath of the actual kerf width
  // (scales with zoom); when false they fall back to thin centerlines. Set
  // every frame by NcCamView. Arrows always stay thin overlays.
  bool                                   m_show_kerf_width = true;
  // Reused scratch for the per-frame triangle-strip ribbon vertices so the
  // kerf-swath draw doesn't reallocate each frame.
  std::vector<Point2d>                   m_ribbon_buf;
  float                                  m_width = 1.0f;
  std::string                            m_style = "solid"; // solid, dashed
  std::string                            m_part_name;
  part_control_data_t                    m_control;
  part_control_data_t                    m_last_control;
  bool                                   m_is_part_selected = false;
  Point2d                                m_bb_min{ 0.0, 0.0 };
  Point2d                                m_bb_max{ 0.0, 0.0 };
  size_t                                 m_number_of_verticies = 0;

  Part(std::string_view name, std::unordered_map<std::string, Layer>&& layers)
    : m_layers(std::move(layers)), m_part_name(name)
  {
    m_control.lead_in_length = 0;
    m_control.lead_out_length = 0;
    m_control.offset = { 0, 0 };
    m_control.scale = 1.0f;
    m_control.smoothing = 0.25f;
    m_control.angle = 0.0f;
    m_control.mouse_mode = 0; // 0 is paths, 1 is parts
    m_last_control.lead_in_length = 0;
    m_last_control.lead_out_length = 0;
    m_last_control.offset = { -1, -1 };
    m_last_control.scale = 1.0f;
    m_last_control.smoothing = 0.25f;
    m_last_control.angle = 0.0f;
  }

  // Implement Primitive interface
  std::string    getTypeName() override;
  void           processMouse(float mpos_x, float mpos_y) override;
  void           render() override;
  nlohmann::json serialize() override;

  // Part-specific methods
  std::vector<std::vector<Point2d>> offsetPath(std::vector<Point2d> path,
                                               double               offset);
  void getBoundingBox(Point2d* bbox_min, Point2d* bbox_max);
  bool checkIfPointIsInsidePath(std::vector<Point2d> path, Point2d point);
  bool checkIfPathIsInsidePath(std::vector<Point2d> path1,
                               std::vector<Point2d> path2);
  std::vector<Toolpath>             getOrderedToolpaths();
  double                            perpendicularDistance(const Point2d& pt,
                                                          const Point2d& lineStart,
                                                          const Point2d& lineEnd);
  void simplify(const std::vector<Point2d>& pointList,
                std::vector<Point2d>&       out,
                double                      epsilon);
  Point2d*
  getClosestPoint(size_t* index, Point2d point, std::vector<Point2d>* points);
  // Builds a toolpath (lead-in + contour, optionally lead-out) into
  // `out`. Returns true on success. The lead-IN is sized by `lead_in_len`
  // for both inside (direction=-1) and outside (direction=+1) contours.
  // A real lead-OUT (sized by `lead_out_len`) is appended for OUTSIDE
  // contours only -- inside contours rely on overburn in the emitter and
  // never get a lead-out. Both the lead-in and the lead-out try an arc
  // first and fall back to a straight lead when geometry doesn't support
  // arc tangency.
  bool createToolpathLeads(Toolpath*                   out,
                           const std::vector<Point2d>& contour,
                           double                      lead_in_len,
                           double                      lead_out_len,
                           int                         direction
                           );
};

#endif // PATH_
