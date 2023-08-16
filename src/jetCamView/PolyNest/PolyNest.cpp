#include "PolyNest.h"

PolyNest::PolyPoint PolyNest::PolyPart::RotatePoint(PolyPoint p, double a)
{
    /*
        Assumes parts extents are already centered!
    */
    PolyPoint return_point;
    double radians = (3.1415926f / 180.0f) * a;
	double cos = cosf(radians);
	double sin = sinf(radians);
	return_point.x = (cos * (p.x)) + (sin * (p.y));
	return_point.y = (cos * (p.y)) - (sin * (p.x));
    return return_point;
}
void PolyNest::PolyPart::GetBoundingBox(PolyPoint *bbox_min, PolyPoint *bbox_max)
{
    bbox_max->x = -1000000;
    bbox_max->y = -1000000;
    bbox_min->x = 1000000;
    bbox_min->y = 1000000;
    for (std::vector<PolyGon>::iterator it = this->built_polygons.begin(); it != this->built_polygons.end(); ++it)
    {
        for (size_t x = 0; x < it->verticies.size(); x++)
        {
            if ((double)it->verticies[x].X < bbox_min->x) bbox_min->x = (double)it->verticies[x].X;
            if ((double)it->verticies[x].X > bbox_max->x) bbox_max->x = (double)it->verticies[x].X;
            if ((double)it->verticies[x].Y < bbox_min->y) bbox_min->y = (double)it->verticies[x].Y;
            if ((double)it->verticies[x].Y > bbox_max->y) bbox_max->y = (double)it->verticies[x].Y;
        }
    }
}
void PolyNest::PolyPart::MoveOutsidePolyGonToBack()
{
    PolyGon outside;
    for (size_t x = 0; x < this->polygons.size(); x++)
    {
        if (this->polygons[x].is_inside == false)
        {
            outside = this->polygons[x];
            this->polygons.erase(this->polygons.begin() + x);
            break;
        }
    }
    this->polygons.push_back(outside);
}
void PolyNest::PolyPart::Build()
{
    this->built_polygons.clear();
    for (size_t x = 0; x < this->polygons.size(); x++)
    {
        PolyGon p;
        p.is_closed = this->polygons[x].is_closed;
        for (size_t i = 0; i < this->polygons[x].verticies.size(); i++)
        {
            PolyPoint point = this->RotatePoint(PolyPoint(this->polygons[x].verticies[i].X, this->polygons[x].verticies[i].Y), *this->angle);
            point.x += *this->offset_x;
            point.y += *this->offset_y;
            p.verticies << ClipperLib::FPoint(point.x, point.y);
        }
        built_polygons.push_back(p);
    }
    this->GetBoundingBox(&this->bbox_min, &this->bbox_max);
}
double PolyNest::PolyNest::MeasureDistanceBetweenPoints(PolyPoint a, PolyPoint b)
{
    double x = a.x - b.x;
    double y = a.y - b.y;
    return sqrtf(x*x + y*y);
}
bool PolyNest::PolyNest::CheckIfPointIsInsidePath(std::vector<PolyPoint> path, PolyPoint point)
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
bool PolyNest::PolyNest::CheckIfPathIsInsidePath(std::vector<PolyPoint> path1, std::vector<PolyPoint> path2)
{
    for (std::vector<PolyPoint>::iterator it = path1.begin(); it != path1.end(); ++it)
    {
        if (this->CheckIfPointIsInsidePath(path2, (*it)))
        {
            return true;
        }
    }
    return false;
}
bool PolyNest::PolyNest::CheckIfPolyPartTouchesAnyPlacedPolyPart(PolyPart p)
{
    for (size_t x = 0; x < this->placed_parts.size(); x++)
    {
        ClipperLib::Clipper c;
        c.AddPath(p.built_polygons.front().verticies, ClipperLib::ptSubject, true);
        c.AddPath(this->placed_parts[x].built_polygons.back().verticies, ClipperLib::ptClip, true);
        ClipperLib::Paths solution;
        c.Execute(ClipperLib::ctIntersection, solution, ClipperLib::pftNonZero, ClipperLib::pftNonZero);
        if (solution.size() > 0)
        {
            return true;
        }
    }
    return false;
}
bool PolyNest::PolyNest::CheckIfPolyPartIsInsideExtents(PolyPart p, PolyPoint min_point, PolyPoint max_point, PolyPart part)
{
    if (p.bbox_min.x > min_point.x && p.bbox_max.x < max_point.x && p.bbox_min.y > min_point.y && p.bbox_max.y < max_point.y)
    {
        return true;
    }
    else
    {
        return false;
    }
}
double PolyNest::PolyNest::PerpendicularDistance(const PolyPoint &pt, const PolyPoint &lineStart, const PolyPoint &lineEnd)
{
	double dx = lineEnd.x - lineStart.x;
	double dy = lineEnd.y - lineStart.y;
	double mag = pow(pow(dx,2.0)+pow(dy,2.0),0.5);
	if(mag > 0.0)
	{
		dx /= mag; dy /= mag;
	}

	double pvx = pt.x - lineStart.x;
	double pvy = pt.y - lineStart.y;
	double pvdot = dx * pvx + dy * pvy;
	double dsx = pvdot * dx;
	double dsy = pvdot * dy;
	double ax = pvx - dsx;
	double ay = pvy - dsy;
	return pow(pow(ax,2.0)+pow(ay,2.0),0.5);
}
void PolyNest::PolyNest::Simplify(const std::vector<PolyPoint> &pointList, std::vector<PolyPoint> &out, double epsilon)
{
	if(pointList.size() < 2)
    {
        throw "Not enough points to simplify";
    }
	double dmax = 0.0;
	size_t index = 0;
	size_t end = pointList.size()-1;
	for(size_t i = 1; i < end; i++)
	{
		double d = this->PerpendicularDistance(pointList[i], pointList[0], pointList[end]);
		if (d > dmax)
		{
			index = i;
			dmax = d;
		}
	}
	if(dmax > epsilon)
	{
		// Recursive call
		std::vector<PolyPoint> recResults1;
		std::vector<PolyPoint> recResults2;
		std::vector<PolyPoint> firstLine(pointList.begin(), pointList.begin()+index+1);
		std::vector<PolyPoint> lastLine(pointList.begin()+index, pointList.end());
		this->Simplify(firstLine, recResults1, epsilon);
		this->Simplify(lastLine, recResults2, epsilon);
		// Build the result list
		out.assign(recResults1.begin(), recResults1.end()-1);
		out.insert(out.end(), recResults2.begin(), recResults2.end());
		if(out.size() < 2)
        {
            throw "Problem assembling output";
        }
	} 
	else 
	{
		//Just return start and end points
		out.clear();
		out.push_back(pointList[0]);
		out.push_back(pointList[end]);
	}
}
PolyNest::PolyPart PolyNest::PolyNest::BuildPart(std::vector<std::vector<PolyPoint>> p, double *offset_x, double *offset_y, double *angle, bool *visable)
{
    PolyPart part;
    for (size_t x = 0; x < p.size(); x++)
    {
        if (p[x].size() > 2)
        {
            PolyGon polygon;
            polygon.is_inside = false;
            for (size_t i = 0; i < p.size(); i++)
            {
                if (i != x)
                {
                    if (this->CheckIfPathIsInsidePath(p[x], p[i]))
                    {
                        polygon.is_inside = true;
                        break;
                    }
                }
            }
            std::vector<PolyPoint> simplified;
            try
            {
                Simplify(p[x], simplified, 0.020);
                ClipperLib::Path subj;
                for (size_t i = 0; i < simplified.size(); i++)
                {
                    subj << ClipperLib::FPoint(simplified[i].x, simplified[i].y);
                }
                ClipperLib::Paths solution;
                ClipperLib::ClipperOffset co;
                co.AddPath(subj, ClipperLib::jtRound, ClipperLib::etClosedPolygon);
                if (polygon.is_inside == true)
                {
                    co.Execute(solution, -0.1875);
                }
                else
                {
                    co.Execute(solution, +0.1875);
                }
                
                if (solution.size() > 0)
                {
                    polygon.verticies = solution.front();
                }
                else
                {
                    throw "Zero solutions on offset!";
                }
                if (this->MeasureDistanceBetweenPoints(PolyPoint(polygon.verticies.front().X, polygon.verticies.front().Y), PolyPoint(polygon.verticies.back().X, polygon.verticies.back().Y)) < this->is_closed_tolorance)
                {
                    polygon.is_closed = true;
                }
                else
                {
                    polygon.is_closed = false;
                }
                part.polygons.push_back(polygon);
            }
            catch(...)
            {
                LOG_F(ERROR, "(PolyNest::PolyNest::BuildPart) Caught Exception!");
            }
        }
    }
    part.offset_x = offset_x;
    part.offset_y = offset_y;
    part.angle = angle;
    part.visable = visable;
    part.MoveOutsidePolyGonToBack();
    part.Build();
    return part;
}
void PolyNest::PolyNest::PushUnplacedPolyPart(std::vector<std::vector<PolyPoint>> p, double *offset_x, double *offset_y, double *angle, bool *visable)
{
    try
    {
        PolyPart part = this->BuildPart(p, offset_x, offset_y, angle, visable);
        this->unplaced_parts.push_back(part);
    }
    catch(const std::exception& e)
    {
        LOG_F(ERROR, "(PolyNest::PolyNest::PushUnplacedPolyPart) Caught Exception: %s", e.what());
    }
}
void PolyNest::PolyNest::PushPlacedPolyPart(std::vector<std::vector<PolyPoint>> p, double *offset_x, double *offset_y, double *angle, bool *visable)
{
    try
    {
        PolyPart part = this->BuildPart(p, offset_x, offset_y, angle, visable);
        this->placed_parts.push_back(part);
    }
    catch(const std::exception& e)
    {
        LOG_F(ERROR, "(PolyNest::PolyNest::PushUnplacedPolyPart) Caught Exception: %s", e.what());
    }
}
void PolyNest::PolyNest::BeginPlaceUnplacedPolyParts()
{
    std::sort(this->unplaced_parts.begin(), this->unplaced_parts.end(), [](const PolyPart &lhs, const PolyPart &rhs) {
        return (lhs.bbox_max.x - lhs.bbox_min.x) * (lhs.bbox_max.y - lhs.bbox_min.y) > (rhs.bbox_max.x - rhs.bbox_min.x) * (rhs.bbox_max.y - rhs.bbox_min.y);
    });
    bool found_valid_placement = false;
    if (this->unplaced_parts.size() > 0 && *this->unplaced_parts.front().offset_x == 0 && *this->unplaced_parts.front().offset_y == 0)
    {
        *this->unplaced_parts.front().offset_x = this->min_extents.x;
        *this->unplaced_parts.front().offset_y = this->min_extents.y;
        *this->unplaced_parts.front().offset_x += (this->unplaced_parts.front().bbox_max.x - this->unplaced_parts.front().bbox_min.x) / 2;
        *this->unplaced_parts.front().offset_y += (this->unplaced_parts.front().bbox_max.y - this->unplaced_parts.front().bbox_min.y) / 2;
    }
    this->found_valid_placement = false;
}
void PolyNest::PolyNest::SetExtents(PolyPoint min, PolyPoint max)
{
    this->min_extents = min;
    this->max_extents = max;
}
bool PolyNest::PolyNest::PlaceUnplacedPolyPartsTick(void *p)
{
    PolyNest *self = reinterpret_cast<PolyNest *>(p);
    if (self != NULL)
    {
        for (int z = 0; z < 500; z++)
        {
            if (self->unplaced_parts.size() > 0)
            {
                if (self->found_valid_placement == true)
                {
                    LOG_F(INFO, "Found valid placement at offset(%.4f, %.4f)", *self->unplaced_parts.front().offset_x, *self->unplaced_parts.front().offset_y);
                    *self->unplaced_parts.front().visable = true;
                    self->found_valid_placement = false;
                    self->placed_parts.push_back(self->unplaced_parts.front());
                    self->unplaced_parts.erase(self->unplaced_parts.begin());
                    if (self->unplaced_parts.size() > 0)
                    {
                        *self->unplaced_parts.front().offset_x = self->min_extents.x;
                        *self->unplaced_parts.front().offset_y = self->min_extents.y;
                        *self->unplaced_parts.front().offset_x += (self->unplaced_parts.front().bbox_max.x - self->unplaced_parts.front().bbox_min.x) / 2;
                        *self->unplaced_parts.front().offset_y += (self->unplaced_parts.front().bbox_max.y - self->unplaced_parts.front().bbox_min.y) / 2;
                    }
                }
                else
                {
                    for (size_t x = 0; x < (360.0 / self->rotation_increments); x++)
                    {
                        if (self->CheckIfPolyPartTouchesAnyPlacedPolyPart(self->unplaced_parts.front()) == false && self->CheckIfPolyPartIsInsideExtents(self->unplaced_parts.front(), self->min_extents, self->max_extents, self->unplaced_parts.front()) == true)
                        {
                            self->found_valid_placement = true;
                        }
                        else
                        {
                            *self->unplaced_parts.front().angle += self->rotation_increments;
                            if (*self->unplaced_parts.front().angle >= 360) *self->unplaced_parts.front().angle -= 360.0;
                            self->unplaced_parts.front().Build();
                        }
                    }
                    if (self->CheckIfPolyPartTouchesAnyPlacedPolyPart(self->unplaced_parts.front()) == false && self->CheckIfPolyPartIsInsideExtents(self->unplaced_parts.front(), self->min_extents, self->max_extents, self->unplaced_parts.front()) == true)
                    {
                        self->found_valid_placement = true;
                    }
                    else
                    {
                        *self->unplaced_parts.front().offset_x += self->offset_increments.x;
                        if (*self->unplaced_parts.front().offset_x > (self->max_extents.x - ((self->unplaced_parts.front().bbox_max.x - self->unplaced_parts.front().bbox_min.x) / 2)))
                        {
                            *self->unplaced_parts.front().offset_x = self->min_extents.x;
                            *self->unplaced_parts.front().offset_x += (self->unplaced_parts.front().bbox_max.x - self->unplaced_parts.front().bbox_min.x) / 2;
                            *self->unplaced_parts.front().offset_y += self->offset_increments.y;
                        }
                        if (*self->unplaced_parts.front().offset_y > (self->max_extents.y - ((self->unplaced_parts.front().bbox_max.y - self->unplaced_parts.front().bbox_min.y) / 2)))
                        {
                            LOG_F(INFO, "Not enough material to place part!");
                            *self->unplaced_parts.front().visable = true;
                            return false;
                        }
                        self->unplaced_parts.front().Build();
                    }
                }
            }
            else
            {
                return false;
            }
        }
    }
    return true;
}