/**
 * @file DXFParsePathAdaptor.h
 */

/*****************************************************************************
**  $Id: DXFParse_Class.h 8865 2008-02-04 18:54:02Z andrew $
**
**  This is part of the dxflib library
**  Copyright (C) 2001 Andrew Mustun
**
**  This program is free software; you can redistribute it and/or modify
**  it under the terms of the GNU Library General Public License as
**  published by the Free Software Foundation.
**
**  This program is distributed in the hope that it will be useful,
**  but WITHOUT ANY WARRANTY; without even the implied warranty of
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**  GNU Library General Public License for more details.
**
**  You should have received a copy of the GNU Library General Public License
**  along with this program; if not, write to the Free Software
**  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
******************************************************************************/

#ifndef DXFParsePathAdaptor_
#define DXFParsePathAdaptor_

#include <NcRender/NcRender.h>
#include <dxf/dxflib/dl_creationadapter.h>

// Forward declarations
class NcCamView;

/**
 * This class takes care of the entities read from the file.
 * Usually such a class would probably store the entities.
 * this one just prints some information about them to stdout.
 *
 * @author Andrew Mustun
 */

struct polyline_vertex_t {
  Point2d point;
  double  bulge;
};

struct polyline_t {
  std::vector<polyline_vertex_t> points;
  bool                           is_closed;
};

// Enhanced spline structure supporting both control points and fit points
struct spline_t {
  std::vector<Point2d> control_points; // NURBS control points
  std::vector<Point2d> fit_points;     // Fit points for interpolation
  std::vector<double>  knots;          // Knot vector for NURBS
  std::vector<double>  weights;        // Weights for rational NURBS
  int                  degree;         // Spline degree
  bool                 is_closed;      // Is the spline closed?
  bool                 has_fit_points; // Uses fit points?
  bool                 has_knots;      // Has explicit knot vector?

  spline_t()
    : degree(3), is_closed(false), has_fit_points(false), has_knots(false)
  {
  }
};

class DXFParsePathAdaptor : public DL_CreationAdapter {
public:
  DXFParsePathAdaptor(
    NcRender*                                              nc_render_instance,
    std::function<void(Primitive*)>                        view_callback,
    std::function<void(Primitive*, const nlohmann::json&)> mouse_callback,
    NcCamView*                                            cam_view);

  enum class Units : int {
    None = 0,
    Inch = 1,
    Millimeter = 4,
    Centimeter = 5,
    Meter = 6,
  };

  virtual void addLayer(const DL_LayerData& data);
  virtual void addPoint(const DL_PointData& data);
  virtual void addLine(const DL_LineData& data);
  virtual void addXLine(const DL_XLineData& data);
  virtual void addArc(const DL_ArcData& data);
  virtual void addCircle(const DL_CircleData& data);
  virtual void addRay(const DL_RayData& data);
  virtual void addEllipse(const DL_EllipseData& data);

  virtual void addPolyline(const DL_PolylineData& data);
  virtual void addVertex(const DL_VertexData& data);

  virtual void addSpline(const DL_SplineData& data);
  virtual void addControlPoint(const DL_ControlPointData& data);
  virtual void addFitPoint(const DL_FitPointData& data);
  virtual void addKnot(const DL_KnotData& data);
  virtual void setVariableInt(const std::string&, int, int);

  NcRender*                                              m_nc_render_instance;
  std::function<void(Primitive*)>                        m_view_callback;
  std::function<void(Primitive*, const nlohmann::json&)> m_mouse_callback;
  NcCamView*                                            m_cam_view;

  void printAttributes();
  void setFilename(std::string f);
  void setImportScale(double scale);
  void setImportQuality(int quality);
  void setSmoothing(float smoothing);
  void setChainTolerance(double chain_tolerance);
  void getApproxBoundingBox(Point2d& bbox_min,
                            Point2d& bbox_max,
                            size_t&  vertex_count);
  bool checkIfPointIsInsidePath(std::vector<Point2d> path, Point2d point);
  bool checkIfPathIsInsidePath(std::vector<Point2d> path1,
                               std::vector<Point2d> path2);
  std::vector<std::vector<Point2d>>
       chainify(std::vector<double_line_t> line_stack, double tolerance);
  void scaleAllPoints(double scale);
  void finish();
  void explodeArcToLines(double cx,
                         double cy,
                         double r,
                         double start_angle,
                         double end_angle,
                         double num_segments);

private:
  void getBoundingBox(const std::vector<std::vector<Point2d>>& path_stack,
                      Point2d&                                 bbox_min,
                      Point2d&                                 bbox_max);

public:
  std::string m_current_layer;
  std::string m_filename;
  float       m_smoothing;
  double      m_import_scale;
  int         m_import_quality;
  double      m_chain_tolerance;
  Units       m_units;

  std::vector<double_line_t> m_line_stack;

  std::vector<polyline_t> m_polylines;
  polyline_t              m_current_polyline;

  std::vector<spline_t> m_splines;
  spline_t              m_current_spline;
};

#endif
