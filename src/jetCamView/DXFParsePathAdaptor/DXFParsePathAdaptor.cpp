#include "DXFParsePathAdaptor.h"
#include <dxf/spline/Bezier.h>
#include <EasyRender/logging/loguru.h>
#include "application.h"
#include "jetCamView/jetCamView.h"

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

/**
 * Default constructor.
 */
DXFParsePathAdaptor::DXFParsePathAdaptor(EasyRender* easy_render_instance, void (*v)(PrimitiveContainer *), void (*m)(PrimitiveContainer*, nlohmann::json)) 
{
    this->current_layer = "default";
    this->filename = "";
    this->easy_render_instance = easy_render_instance;
    this->view_callback = v;
    this->mouse_callback = m;
    this->smoothing = 0.003;
    this->scale = 1.0;
    this->chain_tolorance = 0.001;
}
void DXFParsePathAdaptor::SetScaleFactor(double scale)
{
    this->scale = scale;
}
void DXFParsePathAdaptor::SetSmoothing(double smoothing)
{
    this->smoothing = smoothing;
}
void DXFParsePathAdaptor::SetFilename(std::string f)
{
    this->filename = f;
}
void DXFParsePathAdaptor::SetChainTolorance(double chain_tolorance)
{
    this->chain_tolorance = chain_tolorance;
}
void DXFParsePathAdaptor::GetBoundingBox(const std::vector<std::vector<double_point_t>>& path_stack, double_point_t& bbox_min, double_point_t& bbox_max)
{
    bbox_max.x = INT_MIN;
    bbox_max.y = INT_MIN;
    bbox_min.x = INT_MAX;
    bbox_min.y = INT_MAX;
    for (const auto& path : path_stack)
    {
        for (const auto& point : path)
        {
            bbox_min.x = std::min(point.x, bbox_min.x);
            bbox_max.x = std::max(point.x, bbox_max.x);
            bbox_min.y = std::min(point.y, bbox_min.y);
            bbox_max.y = std::max(point.y, bbox_max.y);
        }
    }
}
bool DXFParsePathAdaptor::CheckIfPointIsInsidePath(std::vector<double_point_t> path, double_point_t point)
{
    size_t polyCorners = path.size();
    size_t j = polyCorners-1;
    bool oddNodes=false;
    for (size_t i = 0; i < polyCorners; i++)
    {
        if (((path[i].y < point.y && path[j].y >= point.y)
        ||   (path[j].y < point.y && path[i].y >= point.y))
        &&  (path[i].x <= point.x || path[j].x <= point.x))
        {
            oddNodes^=(path[i].x + (point.y - path[i].y) / (path[j].y - path[i].y) * (path[j].x - path[i].x) < point.x);
        }
        j=i;
    }
    return oddNodes;
}
bool DXFParsePathAdaptor::CheckIfPathIsInsidePath(std::vector<double_point_t> path1, std::vector<double_point_t> path2)
{
    for (std::vector<double_point_t>::iterator it = path1.begin(); it != path1.end(); ++it)
    {
        if (this->CheckIfPointIsInsidePath(path2, (*it)))
        {
            return true;
        }
    }
    return false;
}
std::vector<std::vector<double_point_t>> DXFParsePathAdaptor::Chainify(std::vector<double_line_t> haystack, double tolorance)
{
    Geometry g;
    std::vector<std::vector<double_point_t>> contours;
    double_point_t point;
    std::vector<double_point_t> chain;
    auto is_point_shared = [](std::vector<double_line_t> h, double_point_t p, double t) 
    { 
        Geometry g;
        for (std::vector<double_line_t>::iterator it = h.begin(); it != h.end(); ++it)
        {
            if (g.distance(p, it->start) < t)
            {
                return true;
            }
            if (g.distance(p, it->end) < t)
            {
                return true;
            }
        }
        return false;
    };
    while(haystack.size() > 0)
    {
        chain.clear();
        double_point_t p1;
        double_point_t p2;
        double_line_t first = haystack.back();
        haystack.pop_back();
        if (is_point_shared(haystack, first.end, tolorance))
        {
            point.x = first.start.x;
            point.y = first.start.y;
            chain.push_back(point);
            point.x = first.end.x;
            point.y = first.end.y;
            chain.push_back(point);
        }
        else
        {
            point.x = first.end.x;
            point.y = first.end.y;
            chain.push_back(point);
            point.x = first.start.x;
            point.y = first.start.y;
            chain.push_back(point);
        }
        bool didSomething;
        do{
            didSomething = false;
            double_point_t current_point = chain.back();
            p1.x = current_point.x;
            p1.y = current_point.y;
            for (int x = 0; x < haystack.size(); x++)
            {
                p2.x = haystack[x].start.x;
                p2.y = haystack[x].start.y;
                if (g.distance(p1, p2) < tolorance)
                {
                    point.x = haystack[x].end.x;
                    point.y = haystack[x].end.y;
                    chain.push_back(point);
                    haystack.erase(haystack.begin()+x);
                    didSomething = true;
                    break;
                }
                p2.x = haystack[x].end.x;
                p2.y = haystack[x].end.y;
                if (g.distance(p1, p2) < tolorance)
                {
                    point.x = haystack[x].start.x;
                    point.y = haystack[x].start.y;
                    chain.push_back(point);
                    haystack.erase(haystack.begin()+x);
                    didSomething = true;
                    break;
                }
            }
        } while(didSomething);
        contours.push_back(chain);
    }
    return contours;
}
void DXFParsePathAdaptor::Finish()
{
    Geometry g = Geometry();
    if (this->current_polyline.points.size() > 0)
    {
        this->polylines.push_back(this->current_polyline); //Push last polyline
        this->current_polyline.points.clear();
    }
    if (this->current_spline.points.size() > 0)
    {
        this->splines.push_back(this->current_spline); //Push last spline
        this->current_spline.points.clear();
    }
    //LOG_F(INFO, "(DXFParsePathAdaptor::Finish) Interpolating %lu splines", this->splines.size());
    for (int x = 0; x < this->splines.size(); x++)
    {
        std::vector<Point> pointList;
        std::vector<Point> out;
        Curve* curve = new Bezier();
	    curve->set_steps(100);
        for (int y = 0; y < this->splines[x].points.size(); y++)
        {
            curve->add_way_point(Vector(this->splines[x].points[y].x, this->splines[x].points[y].y, 0));
        }
        //LOG_F(INFO, "Spline Node Count: %d", curve->node_count());
        if (curve->node_count() > 0)
        {
            for (int i = 0; i < curve->node_count(); i++)
            {
                pointList.push_back(Point(curve->node(i).x, curve->node(i).y));
            }
            try
            {
                g.RamerDouglasPeucker(pointList, 0.003, out);
                for (int i = 1; i < out.size(); i++)
                {
                    this->addLine(DL_LineData((double)out[i-1].first, (double)out[i-1].second, 0, (double)out[i].first, (double)out[i].second, 0));
                }
                if (this->splines[x].isClosed == true)
                {
                    this->addLine(DL_LineData((double)out[0].first, (double)out[0].second, 0, (double)out[curve->node_count()-1].first, (double)out[curve->node_count()-1].second, 0));
                }
            }
            catch(const std::exception& e)
            {
                LOG_F(ERROR, "(DXFParsePathAdaptor::Finish) %s", e.what());
            }
        }
        delete curve;
    }
    //LOG_F(INFO, "(DXFParsePathAdaptor::Finish) Processing %lu polylines", this->polylines.size());
    for (int x = 0; x < this->polylines.size(); x++)
    {
        for (int y = 0; y < this->polylines[x].points.size()-1; y++)
        {
            if (this->polylines[x].points[y].bulge != 0)
            {
                double_point_t bulgeStart;
                bulgeStart.x = this->polylines[x].points[y].point.x;
                bulgeStart.y = this->polylines[x].points[y].point.y;
				double_point_t bulgeEnd;
                bulgeEnd.x = this->polylines[x].points[y + 1].point.x;
                bulgeEnd.y = this->polylines[x].points[y + 1].point.y;
				double_point_t midpoint = g.midpoint(bulgeStart, bulgeEnd);
				double distance = g.distance(bulgeStart, midpoint);
				double sagitta = this->polylines[x].points[y].bulge * distance;

                double_line_t bulgeLine = g.create_polar_line(midpoint, g.measure_polar_angle(bulgeStart, bulgeEnd) + 270, sagitta);
                double_point_t arc_center = g.three_point_circle_center(bulgeStart, bulgeLine.end, bulgeEnd);
                double arc_endAngle, arc_startAngle = 0;
                if (sagitta > 0)
                {
                    arc_endAngle = g.measure_polar_angle(arc_center, bulgeEnd);
                    arc_startAngle = g.measure_polar_angle(arc_center, bulgeStart);
                }
                else
                {
                    arc_endAngle = g.measure_polar_angle(arc_center, bulgeStart);
                    arc_startAngle = g.measure_polar_angle(arc_center, bulgeEnd);
                }
                this->addArc(DL_ArcData((double)arc_center.x, (double)arc_center.y, 0, g.distance(arc_center, bulgeStart), arc_startAngle, arc_endAngle));
            }
            else
            {
                this->addLine(DL_LineData((double)this->polylines[x].points[y].point.x, (double)this->polylines[x].points[y].point.y, 0, (double)this->polylines[x].points[y+1].point.x, (double)this->polylines[x].points[y+1].point.y, 0));
            }
            
        }
        int shared = 0; //Assume we are not shared
        double_point_t our_endpoint = {this->polylines[x].points.back().point.x * this->scale, this->polylines[x].points.back().point.y * this->scale};
        double_point_t our_startpoint = {this->polylines[x].points.front().point.x * this->scale, this->polylines[x].points.front().point.y * this->scale};
        for (std::vector<double_line_t>::iterator it = this->line_stack.begin(); it != this->line_stack.end(); ++it)
        {
            if (g.distance(it->start, our_endpoint) < this->chain_tolorance)
            {
                shared++;
            }
            if (g.distance(it->start, our_startpoint) < this->chain_tolorance)
            {
                shared++;
            }
            if (g.distance(it->end, our_startpoint) < this->chain_tolorance)
            {
                shared++;
            }
            if (g.distance(it->end, our_endpoint) < this->chain_tolorance)
            {
                shared++;
            }
        }
        if (shared == 2)
        {
            if (this->polylines[x].points.back().bulge == 0.0f)
            {
                this->addLine(DL_LineData((double)this->polylines[x].points.front().point.x, (double)this->polylines[x].points.front().point.y, 0, (double)this->polylines[x].points.back().point.x, (double)this->polylines[x].points.back().point.y, 0));
            }
            else
            {
                double_point_t bulgeStart;
                bulgeStart.x = this->polylines[x].points.back().point.x;
                bulgeStart.y = this->polylines[x].points.back().point.y;
				double_point_t bulgeEnd;
                bulgeEnd.x = this->polylines[x].points.front().point.x;
                bulgeEnd.y = this->polylines[x].points.front().point.y;
				double_point_t midpoint = g.midpoint(bulgeStart, bulgeEnd);
				double distance = g.distance(bulgeStart, midpoint);
				double sagitta = this->polylines[x].points.back().bulge * distance;

                double_line_t bulgeLine = g.create_polar_line(midpoint, g.measure_polar_angle(bulgeStart, bulgeEnd) + 270, sagitta);
                double_point_t arc_center = g.three_point_circle_center(bulgeStart, bulgeLine.end, bulgeEnd);
                double arc_endAngle, arc_startAngle = 0;
                if (sagitta > 0)
                {
                    arc_endAngle = g.measure_polar_angle(arc_center, bulgeEnd);
                    arc_startAngle = g.measure_polar_angle(arc_center, bulgeStart);
                }
                else
                {
                    arc_endAngle = g.measure_polar_angle(arc_center, bulgeStart);
                    arc_startAngle = g.measure_polar_angle(arc_center, bulgeEnd);
                }
                this->addArc(DL_ArcData((double)arc_center.x, (double)arc_center.y, 0, g.distance(arc_center, bulgeStart), arc_startAngle, arc_endAngle));
            }
        }
    }
    std::vector<std::vector<double_point_t>> chains = this->Chainify(this->line_stack, this->chain_tolorance);
    std::vector<EasyPrimitive::Part::path_t> paths;
    double_point_t bb_min, bb_max;
    this->GetBoundingBox(chains, bb_min, bb_max);
    for (size_t i = 0; i < chains.size(); i++)
    {
        EasyPrimitive::Part::path_t path;
        path.is_inside_contour = false;
        if (g.distance(chains[i].front(), chains[i].back()) > this->chain_tolorance)
        {
            path.is_closed = false;
        }
        else
        {
            path.is_closed = true;
        }
        this->easy_render_instance->SetColorByName(path.color, globals->jet_cam_view->outside_contour_color);
        for (size_t j = 0; j < chains[i].size(); j++)
        {
            const double px = chains[i][j].x - bb_min.x;
            const double py = chains[i][j].y - bb_min.y;
            path.points.push_back({px, py});
        }
        path.layer = "default";
        path.toolpath_offset = 0.0f;
        path.toolpath_visible = false;
        paths.push_back(path);
    }
    for (size_t x = 0; x < paths.size(); x++)
    {
        for (size_t i = 0; i < paths.size(); i++)
        {
            if (i != x)
            {
                if (this->CheckIfPathIsInsidePath(paths[x].points, paths[i].points))
                {
                    paths[x].is_inside_contour = true;
                    if (paths[x].is_closed == true)
                    {
                        this->easy_render_instance->SetColorByName(paths[x].color, globals->jet_cam_view->inside_contour_color);
                    }
                    else
                    {
                        this->easy_render_instance->SetColorByName(paths[x].color, globals->jet_cam_view->open_contour_color);
                    }
                    break;
                }
            }
        }
    }
    EasyPrimitive::Part *p = this->easy_render_instance->PushPrimitive(new EasyPrimitive::Part(this->filename, paths));
    p->control.smoothing = this->smoothing;
    p->control.scale = this->scale;
    //p->properties->id = this->filename;
    p->properties->mouse_callback = this->mouse_callback;
    p->properties->matrix_callback = this->view_callback;
}
void DXFParsePathAdaptor::ExplodeArcToLines(double cx, double cy, double r, double start_angle, double end_angle, double num_segments)
{
    Geometry g;
    std::vector<Point> pointList;
	std::vector<Point> pointListOut; //List after simplification
    double_point_t start;
    double_point_t sweeper;
    double_point_t end;
    start.x = cx + (r * cosf((start_angle) * 3.1415926f / 180.0f));
	start.y = cy + (r * sinf((start_angle) * 3.1415926f / 180.0f));
    end.x = cx + (r * cosf((end_angle) * 3.1415926f / 180.0f));
	end.y = cy + (r * sinf((end_angle) * 3.1415926 / 180.0f));
    pointList.push_back(Point(start.x, start.y));
    double diff = MAX(start_angle, end_angle) - MIN(start_angle, end_angle);
	if (diff > 180) diff = 360 - diff;
	double angle_increment = diff / num_segments;
	double angle_pointer = start_angle + angle_increment;
    for (int i = 0; i < num_segments; i++)
	{
		sweeper.x = cx + (r * cosf((angle_pointer) * 3.1415926f / 180.0f));
		sweeper.y = cy + (r * sinf((angle_pointer) * 3.1415926f / 180.0f));
		angle_pointer += angle_increment;
        pointList.push_back(Point(sweeper.x, sweeper.y));
	}
    pointList.push_back(Point(end.x, end.y));
    g.RamerDouglasPeucker(pointList, 0.0005, pointListOut);
    for(size_t i=1; i< pointListOut.size(); i++)
	{
        this->addLine(DL_LineData((double)pointListOut[i-1].first, (double)pointListOut[i-1].second, 0, (double)pointListOut[i].first, (double)pointListOut[i].second, 0));
	}
}

void DXFParsePathAdaptor::addLayer(const DL_LayerData& data)
{
    current_layer = data.name;
}

void DXFParsePathAdaptor::addPoint(const DL_PointData& data)
{
    LOG_F(WARNING, "(DXFParsePathAdaptor::addPoint) No point handle!");
}

void DXFParsePathAdaptor::addLine(const DL_LineData& data)
{
    double_line_t l;
    l.start.x = data.x1;
    l.start.y = data.y1;
    l.end.x = data.x2;
    l.end.y = data.y2;
    this->line_stack.push_back(l);
}
void DXFParsePathAdaptor::addXLine(const DL_XLineData& data)
{
    LOG_F(WARNING, "(DXFParsePathAdaptor::addXline) No Xline handle!");
}

void DXFParsePathAdaptor::addArc(const DL_ArcData& data)
{
    this->ExplodeArcToLines(data.cx, data.cy, data.radius, data.angle1, data.angle2, 100);
}

void DXFParsePathAdaptor::addCircle(const DL_CircleData& data)
{
    this->ExplodeArcToLines(data.cx, data.cy, data.radius, 0, 180, 100);
    this->ExplodeArcToLines(data.cx, data.cy, data.radius, 180, 360, 100);
}
void DXFParsePathAdaptor::addEllipse(const DL_EllipseData& data)
{
    LOG_F(WARNING, "(DXFParsePathAdaptor::addEllipse) No Ellipse handle!");
}

void DXFParsePathAdaptor::addPolyline(const DL_PolylineData& data)
{
    if ((data.flags & (1<<0)))
    {
        current_polyline.isClosed = true;
    }
    else
    {
        current_polyline.isClosed = false;
    }
    if (current_polyline.points.size() > 0)
    {
        polylines.push_back(current_polyline); //Push last polyline to
        current_polyline.points.clear();
    }
}

void DXFParsePathAdaptor::addVertex(const DL_VertexData& data)
{
    polyline_vertex_t vertex;
    vertex.point.x = data.x;
    vertex.point.y = data.y;
    vertex.bulge = data.bulge;
    current_polyline.points.push_back(vertex);
}
void DXFParsePathAdaptor::addSpline(const DL_SplineData& data)
{
    if ((data.flags & (1<<0)))
    {
        current_spline.isClosed = true;
    }
    else
    {
        current_spline.isClosed = false;
    }
    if (current_spline.points.size() > 0)
    {
        splines.push_back(current_spline); //Push last spline
        current_spline.points.clear();
    }
}
void DXFParsePathAdaptor::addControlPoint(const DL_ControlPointData& data)
{
    double_point_t p;
    p.x = data.x;
    p.y = data.y;
    current_spline.points.push_back(p);
}
void DXFParsePathAdaptor::addFitPoint(const DL_FitPointData& data)
{
    //LOG_F(WARNING, "(DXFParsePathAdaptor::addFitPoint) No FitPoint handle!");
}
void DXFParsePathAdaptor::addKnot(const DL_KnotData& data)
{
    //LOG_F(WARNING, "(DXFParsePathAdaptor::addKnot) No addKnot handle!");
}
void DXFParsePathAdaptor::addRay(const DL_RayData& data)
{
    //LOG_F(WARNING, "(DXFParsePathAdaptor::addRay) No addRay handle!");
}
