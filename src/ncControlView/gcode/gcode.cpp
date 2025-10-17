#include "gcode.h"
#include <EasyRender/logging/loguru.h>
#include <EasyRender/geometry/geometry.h>
#include "../dialogs/dialogs.h"
#include "../hmi/hmi.h"
#include "ncControlView/ncControlView.h"

gcode_global_t gcode;
gcode_path_t current_path;
std::vector<gcode_path_t> paths;

std::vector<std::string> gcode_split(std::string str, char delimiter)
{ 
    std::vector<std::string> internal; 
    std::stringstream ss(str); // Turn the string into a stream. 
    std::string tok; 
    while(std::getline(ss, tok, delimiter))
    { 
        internal.push_back(tok); 
    } 
    return internal; 
}
std::string gcode_get_filename()
{
    return gcode.filename;
}
unsigned long count_lines(std::string file)
{
    std::string line;
    unsigned long count = 0;
    std::ifstream in(file);
    while(std::getline(in, line))
    {
        count++;
    }
    return count;
}

bool gcode_open_file(std::string file)
{
    gcode.file.open(file);
    if (gcode.file.is_open())
    {
        globals->renderer->DeletePrimitivesById("gcode");
        globals->renderer->DeletePrimitivesById("gcode_arrows");
        globals->renderer->DeletePrimitivesById("gcode_highlights");
        gcode.line_count = count_lines(file);
        gcode.lines_consumed = 0;
        gcode.filename = file;
        LOG_F(INFO, "Opened file: %s, size: %lu lines", file.c_str(), gcode.line_count);
        dialogs_show_progress_window(true);
        dialogs_set_progress_value(0.0f);
        return true;
    }
    else
    {
        LOG_F(ERROR, "Could not open file: %s", file.c_str());
        return false;
    }
}
nlohmann::json parse_line(std::string line)
{
    nlohmann::json ret;
    std::transform(line.begin(), line.end(),line.begin(), ::toupper);
    std::string active_word = "";
    std::string x_value = "";
    std::string y_value = "";
    for (int x = 0; x < line.size(); x++)
    {
        if (line[x] == 'X')
        {
            active_word = "X";
        }
        else if (line[x] == 'Y')
        {
            active_word = "Y";
        }
        else if (line[x] == 'F')
        {
            active_word = "F";
        }
        else if (line[x] == 'G')
        {
            active_word = "G";
        }
        else if (line[x] == ' ')
        {
            //Eat white spaces....
        }
        else
        {
            if (active_word == "X")
            {
                x_value += line[x];
            }
            if (active_word == "Y")
            {
                y_value += line[x];
            }
        }
    }
    // Rendering in positive quadrant, so invert gcode values because machine
    // position is in negative quadrant.
    ret["x"] = -atof(x_value.c_str());
    ret["y"] = -atof(y_value.c_str());
    return ret;
}
void gcode_push_current_path_to_viewer(int rapid_line)
{
    if (current_path.points.size() > 0)
    {
        Geometry geo;
        try
        {
            if (current_path.points.size() > 1)
            {
                std::vector<double_point_t> simplified = geo.simplify(current_path.points, 0.010);
                std::vector<double_point_t> path;
                int point_count = 0;
                for (int i = 0; i < simplified.size(); i++)
                {
                    if ((i == 0 && i+1 < simplified.size()) || (point_count == 0 && i+1 < simplified.size()))
                    {
                        std::vector<double_point_t> arrow_path;
                        double_point_t midpoint = geo.midpoint(simplified[i+1], simplified[i]);
                        arrow_path.push_back({midpoint.x, midpoint.y});
                        double angle = geo.measure_polar_angle(simplified[i + 1], simplified[i]);
                        double_point_t p1 = geo.create_polar_line(midpoint, angle +30, 0.020).end;
                        double_point_t p2 = geo.create_polar_line(midpoint, angle -30, 0.020).end;
                        arrow_path.push_back({p1.x, p1.y});
                        arrow_path.push_back({p2.x, p2.y});
                        EasyPrimitive::Path *direction_indicator = globals->renderer->PushPrimitive(new EasyPrimitive::Path(arrow_path));
                        globals->renderer->SetColorByName(direction_indicator->properties->color, "blue");
                        direction_indicator->properties->id = "gcode_arrows";
                        direction_indicator->properties->matrix_callback = globals->nc_control_view->view_matrix;
                        direction_indicator->properties->visible = false;
                    }
                    path.push_back({simplified[i].x, simplified[i].y});
                    point_count++;
                    if (point_count > 2)
                    {
                        point_count = 0;
                    }
                }
                EasyPrimitive::Path *g = globals->renderer->PushPrimitive(new EasyPrimitive::Path(path));
                g->is_closed = false;
                g->properties->id = "gcode";
                g->properties->data = {{"rapid_line", rapid_line}};
                globals->renderer->SetColorByName(g->properties->color, "white");
                g->properties->matrix_callback = globals->nc_control_view->view_matrix;
                g->properties->mouse_callback = &hmi_mouse_callback;
                g->properties->visible = false;
            }
        }
        catch(const std::exception& e)
        {
            LOG_F(ERROR, "Caught Exception: %s", e.what());
        }
    }
}
bool gcode_parse_timer()
{
    std::string line;
    for (int x = 0; x < 1000; x++)
    {
        if (std::getline(gcode.file, line))
        {
            gcode.lines_consumed++;
            dialogs_set_progress_value((float)gcode.lines_consumed / (float)gcode.line_count);
            if (line.find("G0") != std::string::npos)
            {
                double_point_t last_path_endpoint = {INT_MIN, INT_MIN};
                if (current_path.points.size() > 0)
                {
                    last_path_endpoint = current_path.points[current_path.points.size()-1];
                }
                gcode_push_current_path_to_viewer(gcode.last_rapid_line);
                if (current_path.points.size() > 0) paths.push_back(current_path);
                current_path.points.clear();
                nlohmann::json g = parse_line(line);
                try
                {
                    current_path.points.push_back({ (double)g["x"], (double)g["y"]});
                    if (last_path_endpoint.x != INT_MIN && last_path_endpoint.y != INT_MIN)
                    {
                        EasyPrimitive::Line *l = globals->renderer->PushPrimitive(new EasyPrimitive::Line(last_path_endpoint, {(double)g["x"], (double)g["y"]}));
                        l->properties->id = "gcode";
                        l->style = "dashed";
                        globals->renderer->SetColorByName(l->properties->color, "grey");
                        l->properties->matrix_callback = globals->nc_control_view->view_matrix;
                        l->properties->visible = false;
                    }
                }
                catch(...)
                {
                    LOG_F(ERROR, "Gcode parsing error at line %lu in file %s", gcode.lines_consumed, gcode.filename.c_str());
                }
                gcode.last_rapid_line = gcode.lines_consumed -1;
            }
            else if (line.find("G1") != std::string::npos)
            {
                nlohmann::json g = parse_line(line);
                try
                {
                    current_path.points.push_back({ (double)g["x"], (double)g["y"]});
                }
                catch(...)
                {
                    LOG_F(ERROR, "Gcode parsing error at line %lu in file %s", gcode.lines_consumed, gcode.filename.c_str());
                }
            }

        }
        else
        {
            LOG_F(INFO, "Reached end of file!");
            gcode_push_current_path_to_viewer(gcode.last_rapid_line);
            if (current_path.points.size() > 0) paths.push_back(current_path);
            current_path.points.clear();
            gcode.file.close();
            dialogs_set_progress_value(1.0f);
            dialogs_show_progress_window(false);
            std::vector<PrimitiveContainer *> *stack = globals->renderer->GetPrimitiveStack();
            for (size_t x = 0; x < stack->size(); x++)
            {
                if (stack->at(x)->properties->id == "gcode" || stack->at(x)->properties->id == "gcode_arrows" || stack->at(x)->properties->id == "gcode_highlights")
                {
                    stack->at(x)->properties->visible = true;
                }
            }
            return false;
        }
    }
    //LOG_F(INFO, "Progress: %.4f", (float)(((float)gcode.lines_consumed / (float)gcode.line_count) * 100.0f));
    return true;
}
