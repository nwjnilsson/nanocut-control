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

#include <EasyRender/EasyRender.h>
#include <dxf/dxflib/dl_creationadapter.h>

/**
 * This class takes care of the entities read from the file.
 * Usually such a class would probably store the entities.
 * this one just prints some information about them to stdout.
 *
 * @author Andrew Mustun
 */

struct polyline_vertex_t {
  double_point_t point;
  double         bulge;
};

struct polyline_t {
  std::vector<polyline_vertex_t> points;
  bool                           is_closed;
};

// Enhanced spline structure supporting both control points and fit points
struct spline_t {
  std::vector<double_point_t> control_points; // NURBS control points
  std::vector<double_point_t> fit_points;     // Fit points for interpolation
  std::vector<double>         knots;          // Knot vector for NURBS
  std::vector<double>         weights;        // Weights for rational NURBS
  int                         degree;         // Spline degree
  bool                        is_closed;      // Is the spline closed?
  bool                        has_fit_points; // Uses fit points?
  bool                        has_knots;      // Has explicit knot vector?

  spline_t()
    : degree(3), is_closed(false), has_fit_points(false), has_knots(false)
  {
  }
};

class DXFParsePathAdaptor : public DL_CreationAdapter {
public:
  DXFParsePathAdaptor(EasyRender* easy_render_pointer,
                      void (*v)(PrimitiveContainer*),
                      void (*m)(PrimitiveContainer*, const nlohmann::json&));

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

  EasyRender* easy_render_instance;
  void (*view_callback)(PrimitiveContainer*);
  void (*mouse_callback)(PrimitiveContainer*, const nlohmann::json&);

  void printAttributes();
  void SetFilename(std::string f);
  void SetImportScale(double scale);
  void SetImportQuality(int quality);
  void SetSmoothing(float smoothing);
  void SetChainTolerance(double chain_tolerance);
  void GetApproxBoundingBox(double_point_t& bbox_min,
                            double_point_t& bbox_max,
                            size_t&         vertex_count);
  bool CheckIfPointIsInsidePath(std::vector<double_point_t> path,
                                double_point_t              point);
  bool CheckIfPathIsInsidePath(std::vector<double_point_t> path1,
                               std::vector<double_point_t> path2);
  std::vector<std::vector<double_point_t>>
       Chainify(std::vector<double_line_t> line_stack, double tolerance);
  void ScaleAllPoints(double scale);
  void Finish();
  void ExplodeArcToLines(double cx,
                         double cy,
                         double r,
                         double start_angle,
                         double end_angle,
                         double num_segments);

private:
  void
  GetBoundingBox(const std::vector<std::vector<double_point_t>>& path_stack,
                 double_point_t&                                 bbox_min,
                 double_point_t&                                 bbox_max);

public:
  std::string current_layer;
  std::string filename;
  float       smoothing;
  double      import_scale;
  int         import_quality;
  double      chain_tolerance;
  Units       units;

  std::vector<double_line_t> line_stack;

  std::vector<polyline_t> polylines;
  polyline_t              current_polyline;

  std::vector<spline_t> splines;
  spline_t              current_spline;
};

#endif
