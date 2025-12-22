#ifndef PART_
#define PART_

#include "../../geometry/geometry.h"
#include "../Primitive.h"
#include <application.h>
#include <string>
#include <vector>

class Part : public Primitive {
public:
  struct path_t {
    std::vector<Point2d> points;
    std::vector<Point2d> built_points;
    std::string          layer;
    double               toolpath_offset;
    bool                 toolpath_visible;
    bool                 is_closed;
    bool                 is_inside_contour;
    float                color[4];
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
              lead_out_length == a.lead_out_length &&
              scale == a.scale && smoothing == a.smoothing && angle == a.angle &&
              mouse_mode == a.mouse_mode);
    }
  };

  std::vector<path_t>               m_paths;
  std::vector<std::vector<Point2d>> m_tool_paths;
  float                             m_width = 1.0f;
  std::string                       m_style = "solid"; // solid, dashed
  std::string                       m_part_name;
  part_control_data_t               m_control;
  part_control_data_t               m_last_control;
  bool                              m_is_part_selected = false;
  Point2d                           m_bb_min{ 0.0, 0.0 };
  Point2d                           m_bb_max{ 0.0, 0.0 };
  size_t                            m_number_of_verticies = 0;

  Part(std::string name, std::vector<path_t> p)
    : m_paths(std::move(p)), m_part_name(std::move(name))
  {
    m_control.lead_in_length = 0;
    m_control.lead_out_length = 0;
    m_control.offset = { 0, 0 };
    m_control.scale = 1.0f;
    m_control.smoothing = SCALE(0.25f);
    m_control.angle = 0.0f;
    m_control.mouse_mode = 0; // 0 is paths, 1 is parts
    m_last_control.lead_in_length = 0;
    m_last_control.lead_out_length = 0;
    m_last_control.offset = { -1, -1 };
    m_last_control.scale = 1.0f;
    m_last_control.smoothing = SCALE(0.25f);
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
  std::vector<std::vector<Point2d>> getOrderedToolpaths();
  double                            perpendicularDistance(const Point2d& pt,
                                                          const Point2d& lineStart,
                                                          const Point2d& lineEnd);
  void simplify(const std::vector<Point2d>& pointList,
                std::vector<Point2d>&       out,
                double                      epsilon);
  Point2d*
  getClosestPoint(size_t* index, Point2d point, std::vector<Point2d>* points);
  bool createToolpathLeads(std::vector<Point2d>* tpath,
                           double                length,
                           Point2d               position,
                           int                   direction);
};

#endif // PATH_
