/**
 * @file DXFParse_Class.h
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
  bool                           isClosed;
};
struct spline_t {
  std::vector<double_point_t> points;
  bool                        isClosed;
};

class DXFParsePathAdaptor : public DL_CreationAdapter {
public:
  DXFParsePathAdaptor(EasyRender* easy_render_pointer,
                      void (*v)(PrimitiveContainer*),
                      void (*m)(PrimitiveContainer*, nlohmann::json));

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

  EasyRender* easy_render_instance;
  void (*view_callback)(PrimitiveContainer*);
  void (*mouse_callback)(PrimitiveContainer*, nlohmann::json);

  void printAttributes();
  void SetFilename(std::string f);
  void SetScaleFactor(double scale);
  void SetSmoothing(double smoothing);
  void SetChainTolorance(double chain_tolorance);
  void
  GetBoundingBox(const std::vector<std::vector<double_point_t>>& path_stack,
                 double_point_t&                                 bbox_min,
                 double_point_t&                                 bbox_max);
  bool CheckIfPointIsInsidePath(std::vector<double_point_t> path,
                                double_point_t              point);
  bool CheckIfPathIsInsidePath(std::vector<double_point_t> path1,
                               std::vector<double_point_t> path2);
  std::vector<std::vector<double_point_t>>
       Chainify(std::vector<double_line_t> line_stack, double tolorance);
  void Finish();
  void ExplodeArcToLines(double cx,
                         double cy,
                         double r,
                         double start_angle,
                         double end_angle,
                         double num_segments);

  std::string                current_layer;
  std::string                filename;
  std::vector<double_line_t> line_stack;
  double                     smoothing;
  double                     scale;
  double                     chain_tolorance;

  std::vector<polyline_t> polylines;
  polyline_t              current_polyline;

  std::vector<spline_t> splines;
  spline_t              current_spline;
};

#endif
