#include "hmi.h"
#include "../motion_control/motion_control.h"
#include "../dialogs/dialogs.h"
#include "../gcode/gcode.h"
#include <limits>

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))


double hmi_backplane_width = 300;
EasyPrimitive::Box *hmi_backpane;
double hmi_dro_backplane_height = 220;
EasyPrimitive::Box *hmi_dro_backpane;
EasyPrimitive::Box *hmi_button_backpane;
EasyPrimitive::Path *arc_okay_highlight_path;
dro_group_data_t dro;
std::vector<hmi_button_group_t> button_groups;
bool left_control_key_is_pressed = false;

template <typename T> std::string to_string_strip_zeros(const T a_value) //Fails to link even though motion_control.h is included here...
{
    std::ostringstream oss;
    oss << std::setprecision(5) << std::noshowpoint << a_value;
    std::string str = oss.str();
    return oss.str();
}

void hmi_get_bounding_box(double_point_t *bbox_min, double_point_t *bbox_max)
{
    std::vector<PrimitiveContainer*> *stack = globals->renderer->GetPrimitiveStack();
    auto min = std::numeric_limits<int>::min();
    auto max = std::numeric_limits<int>::max();
    bbox_max->x = min;
    bbox_max->y = min;
    bbox_min->x = max;
    bbox_min->y = max;
    for (int x = 0; x < stack->size(); x++)
    {
        if (stack->at(x)->type == "path")
        {
            for (int i = 0; i < stack->at(x)->path->points.size(); i++)
            {
                if ((double)stack->at(x)->path->points.at(i).x + globals->nc_control_view->machine_parameters.work_offset[0] < bbox_min->x) bbox_min->x = stack->at(x)->path->points.at(i).x + globals->nc_control_view->machine_parameters.work_offset[0];
                if ((double)stack->at(x)->path->points.at(i).x + globals->nc_control_view->machine_parameters.work_offset[0] > bbox_max->x) bbox_max->x = stack->at(x)->path->points.at(i).x + globals->nc_control_view->machine_parameters.work_offset[0];
                if ((double)stack->at(x)->path->points.at(i).y + globals->nc_control_view->machine_parameters.work_offset[1] < bbox_min->y) bbox_min->y = stack->at(x)->path->points.at(i).y + globals->nc_control_view->machine_parameters.work_offset[1];
                if ((double)stack->at(x)->path->points.at(i).y + globals->nc_control_view->machine_parameters.work_offset[1] > bbox_max->y) bbox_max->y = stack->at(x)->path->points.at(i).y + globals->nc_control_view->machine_parameters.work_offset[1];
            }
        }
    }
}

void hmi_handle_button(std::string id)
{
    nlohmann::json dro_data = motion_controller_get_dro();
    try
     {
        
        if (dro_data["IN_MOTION"] == false)
        {
            if (id == "Wpos")
            {
                LOG_F(INFO, "Clicked Wpos");
                motion_controller_push_stack("G0 X0 Y0");
                motion_controller_push_stack("M30");
                motion_controller_run_stack();
            }
            else if (id == "Park")
            {
                LOG_F(INFO, "Clicked Park");
                motion_controller_push_stack("G53 G0 Z0");
                motion_controller_push_stack("G53 G0 X0 Y0");
                motion_controller_push_stack("M30");
                motion_controller_run_stack();
            }
            else if (id == "Zero X")
            {
                LOG_F(INFO, "Clicked Zero X");
                try
                {
                    globals->nc_control_view->machine_parameters.work_offset[0] = (float)motion_controller_get_dro()["MCS"]["x"];
                    motion_controller_push_stack("G10 L2 P0 X" + to_string_strip_zeros(globals->nc_control_view->machine_parameters.work_offset[0]));
                    motion_controller_push_stack("M30");
                    motion_controller_run_stack();
                    motion_controller_save_machine_parameters();
                }
                catch (...)
                {
                    LOG_F(ERROR, "Could not set x work offset!");
                }
            }
            else if (id == "Zero Y")
            {
                LOG_F(INFO, "Clicked Zero Y");
                try
                {
                    globals->nc_control_view->machine_parameters.work_offset[1] = (float)motion_controller_get_dro()["MCS"]["y"];
                    motion_controller_push_stack("G10 L2 P0 Y" + to_string_strip_zeros(globals->nc_control_view->machine_parameters.work_offset[1]));
                    motion_controller_push_stack("M30");
                    motion_controller_run_stack();
                    motion_controller_save_machine_parameters();
                }
                catch (...)
                {
                    LOG_F(ERROR, "Could not set y work offset!");
                }
            }
            else if (id == "Zero Z")
            {
                LOG_F(INFO, "Clicked Zero Z");
                try
                {
                    globals->nc_control_view->machine_parameters.work_offset[2] = (float)motion_controller_get_dro()["MCS"]["z"];
                    motion_controller_push_stack("G10 L2 P0 Z" + std::to_string(globals->nc_control_view->machine_parameters.work_offset[2]));
                    motion_controller_push_stack("M30");
                    motion_controller_run_stack();
                    motion_controller_save_machine_parameters();
                }
                catch (...)
                {
                    LOG_F(ERROR, "Could not set y work offset!");
                }
            }
            else if (id == "Spindle On")
            {
                LOG_F(INFO, "Spindle On");
                try
                {
                    motion_controller_push_stack("M3 S1000");
                    motion_controller_push_stack("M30");
                    motion_controller_run_stack();
                    motion_controller_save_machine_parameters();
                }
                catch (...)
                {
                    LOG_F(ERROR, "Could not set y work offset!");
                }
            }
            else if (id == "Spindle Off")
            {
                LOG_F(INFO, "Spindle Off");
                try
                {
                    motion_controller_push_stack("M5");
                    motion_controller_push_stack("M30");
                    motion_controller_run_stack();
                    motion_controller_save_machine_parameters();
                }
                catch (...)
                {
                    LOG_F(ERROR, "Could not set y work offset!");
                }
            }
            else if (id == "Retract")
            {
                LOG_F(INFO, "Clicked Retract");
                motion_controller_push_stack("G53 G0 Z0");
                motion_controller_push_stack("M30");
                motion_controller_run_stack();
            }
            else if (id == "Touch")
            {
                LOG_F(INFO, "Clicked Touch");
                motion_controller_push_stack("touch_torch");
                motion_controller_push_stack("M30");
                motion_controller_run_stack();
            }
            else if (id == "Run")
            {
                LOG_F(INFO, "Clicked Run");
                double_point_t bbox_min, bbox_max;
                hmi_get_bounding_box(&bbox_min, &bbox_max);
                if (bbox_min.x > 0.0f + globals->nc_control_view->machine_parameters.cutting_extents[0] &&
                    bbox_min.y > 0.0f + globals->nc_control_view->machine_parameters.cutting_extents[1] &&
                    bbox_max.x < globals->nc_control_view->machine_parameters.machine_extents[0] + globals->nc_control_view->machine_parameters.cutting_extents[2] &&
                    bbox_max.y < globals->nc_control_view->machine_parameters.machine_extents[1] + globals->nc_control_view->machine_parameters.cutting_extents[3])
                {
                    try
                    {
                        if (gcode_get_filename() != "")
                        {
                            std::ifstream gcode_file(gcode_get_filename());
                            if (gcode_file.is_open())
                            {
                                std::string line;
                                while (std::getline(gcode_file, line))
                                {
                                    motion_controller_push_stack(line);
                                }
                                gcode_file.close();
                                motion_controller_run_stack();
                            }
                            else
                            {
                                dialogs_set_info_value("Could not open file!");
                            }
                        }
                    }
                    catch(...)
                    {
                        dialogs_set_info_value("Caught exception while trying to read file!");
                    }
                }
                else
                {
                    dialogs_set_info_value("Program is outside of machines cuttable extents!");
                }
            }
            else if (id == "Test Run")
            {
                LOG_F(INFO, "Clicked Test Run");
                double_point_t bbox_min, bbox_max;
                hmi_get_bounding_box(&bbox_min, &bbox_max);
                if (bbox_min.x > 0.0f + globals->nc_control_view->machine_parameters.cutting_extents[0] &&
                    bbox_min.y > 0.0f + globals->nc_control_view->machine_parameters.cutting_extents[1] &&
                    bbox_max.x < globals->nc_control_view->machine_parameters.machine_extents[0] + globals->nc_control_view->machine_parameters.cutting_extents[2] &&
                    bbox_max.y < globals->nc_control_view->machine_parameters.machine_extents[1] + globals->nc_control_view->machine_parameters.cutting_extents[3])
                {
                    try
                    {
                        if (gcode_get_filename() != "")
                        {
                            std::ifstream gcode_file(gcode_get_filename());
                            if (gcode_file.is_open())
                            {
                                std::string line;
                                while (std::getline(gcode_file, line))
                                {
                                    if (line.find("fire_torch") != std::string::npos)
                                    {
                                        removeSubstrs(line, "fire_torch");
                                        motion_controller_push_stack("touch_torch" + line);
                                    }
                                    else
                                    {
                                        motion_controller_push_stack(line);
                                    }
                                }
                                gcode_file.close();
                                motion_controller_run_stack();
                            }
                            else
                            {
                                dialogs_set_info_value("Could not open file!");
                            }
                        }
                    }
                    catch(...)
                    {
                        dialogs_set_info_value("Caught exception while trying to read file!");
                    }
                }
                else
                {
                    dialogs_set_info_value("Program is outside of machines cuttable extents!");
                }
            }
        }
        if (id == "Abort")
        {
            LOG_F(INFO, "Clicked Abort");
            motion_controller_cmd("abort");
        }
        if (id == "Clean")
        {
            LOG_F(INFO, "Clicked Clean");
            globals->renderer->DeletePrimitivesById("gcode_highlights");
        }
        else if (id == "Fit")
        {
            LOG_F(INFO, "Clicked Fit");
            double_point_t bbox_min, bbox_max;
            hmi_get_bounding_box(&bbox_min, &bbox_max);
            if (bbox_max.x == -1000000 && bbox_max.y == -1000000 && bbox_min.x == 1000000 && bbox_min.y == 1000000)
            {
                LOG_F(INFO, "No paths, fitting to machine extents!");
                globals->zoom = 1;
                globals->pan.x = ((globals->nc_control_view->machine_parameters.machine_extents[0]) / 2) - (hmi_backplane_width / 2);
                globals->pan.y = (((globals->nc_control_view->machine_parameters.machine_extents[1]) / 2) + 10) * -1; //10 is for the menu bar
                if ((MAX((double)globals->renderer->GetWindowSize().y, (double)globals->nc_control_view->machine_parameters.machine_extents[1]) - MIN((double)globals->renderer->GetWindowSize().y, (double)globals->nc_control_view->machine_parameters.machine_extents[1])) < 
                    MAX((double)globals->renderer->GetWindowSize().x - hmi_backplane_width, (double)globals->nc_control_view->machine_parameters.machine_extents[0]) - MIN((double)globals->renderer->GetWindowSize().x, (double)globals->nc_control_view->machine_parameters.machine_extents[0]))
                {
                    double machine_y = std::abs(globals->nc_control_view->machine_parameters.machine_extents[1]);
                    globals->zoom = ((double)globals->renderer->GetWindowSize().y - 100) / machine_y;
                    globals->pan.x += ((double)globals->nc_control_view->machine_parameters.machine_extents[0] / 2) * (1 - globals->zoom);
                    globals->pan.y += ((double)globals->nc_control_view->machine_parameters.machine_extents[1] / 2) * (1 - globals->zoom);
                }
                else //Fit X
                {
                    double machine_x = std::abs(globals->nc_control_view->machine_parameters.machine_extents[0]);
                    globals->zoom = ((double)globals->renderer->GetWindowSize().x - (hmi_backplane_width / 2) - 200) / machine_x;
                    globals->pan.x += ((double)globals->nc_control_view->machine_parameters.machine_extents[0] / 2) * (1 - globals->zoom) - (hmi_backplane_width / 2) + 50;
                    globals->pan.y += ((double)globals->nc_control_view->machine_parameters.machine_extents[1] / 2) * (1 - globals->zoom);
                }
            }
            else
            {
                LOG_F(INFO, "Calculated bounding box: => bbox_min = (%.4f, %.4f) and bbox_max = (%.4f, %.4f)", bbox_min.x, bbox_min.y, bbox_max.x, bbox_max.y);
                double y_diff = std::abs(bbox_max.y) - std::abs(bbox_min.y);
                double x_diff = std::abs(bbox_max.x) - std::abs(bbox_min.x);
                globals->zoom = 1;
                globals->pan.x = (x_diff / 2) - (hmi_backplane_width / 2);
                globals->pan.y = (((y_diff / 2) + 10) * -1); //10 is for the menu bar
                if ((MAX((double)globals->renderer->GetWindowSize().y, (bbox_max.x - bbox_min.x)) - MIN((double)globals->renderer->GetWindowSize().y, (bbox_max.y - bbox_min.y))) < 
                    MAX((double)globals->renderer->GetWindowSize().x - hmi_backplane_width, (bbox_max.x - bbox_min.x)) - MIN((double)globals->renderer->GetWindowSize().x, (bbox_max.y - bbox_min.y)))
                {
                    LOG_F(INFO, "Fitting to Y");
                    globals->zoom = ((double)globals->renderer->GetWindowSize().y - 300) / std::abs(y_diff);
                    globals->pan.x += x_diff * (1 - globals->zoom) + (hmi_backplane_width / 2);
                    globals->pan.y += y_diff * (1 - globals->zoom);
                    globals->pan.x -= globals->nc_control_view->machine_parameters.work_offset[0] * globals->zoom;
                    globals->pan.y -= globals->nc_control_view->machine_parameters.work_offset[1] * globals->zoom;
                }
                else //Fit X
                {
                    LOG_F(INFO, "Fitting to X");
                    globals->zoom = ((double)globals->renderer->GetWindowSize().x - (hmi_backplane_width / 2) - 200) / std::abs(x_diff);
                    globals->pan.x += x_diff * (1 - globals->zoom) + (hmi_backplane_width / 2);
                    globals->pan.y += y_diff * (1 - globals->zoom);
                    globals->pan.x -= globals->nc_control_view->machine_parameters.work_offset[0] * globals->zoom;
                    globals->pan.y -= globals->nc_control_view->machine_parameters.work_offset[1] * globals->zoom;
                }
            }
        }
        else if (id == "ATHC")
        {
            LOG_F(INFO, "Clicked ATHC");
            dialogs_show_thc_window(true);
        }
    }
    catch(...)
    {
        LOG_F(ERROR, "JSON Parsing ERROR!");
    }
}

void hmi_jumpin(PrimitiveContainer* p)
{
    double_point_t bbox_min, bbox_max;
    hmi_get_bounding_box(&bbox_min, &bbox_max);
    if (bbox_min.x > 0.0f + globals->nc_control_view->machine_parameters.cutting_extents[0] &&
        bbox_min.y > 0.0f + globals->nc_control_view->machine_parameters.cutting_extents[1] &&
        bbox_max.x < globals->nc_control_view->machine_parameters.machine_extents[0] + globals->nc_control_view->machine_parameters.cutting_extents[2] &&
        bbox_max.y < globals->nc_control_view->machine_parameters.machine_extents[1] + globals->nc_control_view->machine_parameters.cutting_extents[3])
    {
        try
        {
            if (gcode_get_filename() != "")
            {
                std::ifstream gcode_file(gcode_get_filename());
                if (gcode_file.is_open())
                {
                    std::string line;
                    unsigned long count = 0;
                    while (std::getline(gcode_file, line))
                    {
                        if (count > (unsigned long)p->properties->data["rapid_line"] - 1) motion_controller_push_stack(line);
                        count++;
                    }
                    gcode_file.close();
                    motion_controller_run_stack();
                }
                else
                {
                    dialogs_set_info_value("Could not open file!");
                }
            }
        }
        catch(...)
        {
            dialogs_set_info_value("Caught exception while trying to read file!");
        }
    }
    else
    {
        dialogs_set_info_value("Program is outside of machines cuttable extents!");
    }
}

void hmi_reverse(PrimitiveContainer* p)
{
    std::vector<std::string> gcode_lines_before_reverse;
    std::vector<std::string> gcode_lines_to_reverse;
    std::vector<std::string> gcode_lines_after_reverse;
    bool found_path_end = false;
    try
    {
        if (gcode_get_filename() != "")
        {
            std::ifstream gcode_file(gcode_get_filename());
            if (gcode_file.is_open())
            {
                std::string line;
                unsigned long count = 0;
                while (std::getline(gcode_file, line))
                {
                    if (count <= (unsigned long)p->properties->data["rapid_line"] + 1)
                    {
                        gcode_lines_before_reverse.push_back(line);
                    }
                    else if (count > (unsigned long)p->properties->data["rapid_line"] + 1)
                    {
                        if (line.find("torch_off") != std::string::npos)
                        {
                            found_path_end = true;
                        }
                        if (found_path_end == true)
                        {
                            gcode_lines_after_reverse.push_back(line);
                        }
                        else
                        {
                            gcode_lines_to_reverse.push_back(line);
                        }
                    }
                    count++;
                }
                gcode_file.close();
            }
            else
            {
                dialogs_set_info_value("Could not open file!");
            }
        }
    }
    catch(...)
    {
        dialogs_set_info_value("Caught exception while trying to read file!");
    }
    try
    {
        std::ofstream out(gcode_get_filename());
        for (unsigned long x = 0; x < gcode_lines_before_reverse.size(); x++)
        {
            out << gcode_lines_before_reverse.at(x); 
            out << std::endl;
        }
        while(gcode_lines_to_reverse.size() > 0)
        {
            out << gcode_lines_to_reverse.back();
            out << std::endl;
            gcode_lines_to_reverse.pop_back();
        }
        for (unsigned long x = 0; x < gcode_lines_after_reverse.size(); x++)
        {
            out << gcode_lines_after_reverse.at(x); 
            out << std::endl;
        }
        out.close();
        if (gcode_open_file(gcode_get_filename()))
        {
            globals->renderer->PushTimer(0, &gcode_parse_timer);
        }
    }
    catch(...)
    {
        dialogs_set_info_value("Could not write gcode file to disk!");
    }
    
}
void hmi_mouse_callback(PrimitiveContainer* c, nlohmann::json e)
{
    //LOG_F(INFO, "%s", e.dump().c_str());
    if (c->type == "path" && c->properties->id == "gcode")
    {
        if (e["event"] == "mouse_in" && left_control_key_is_pressed == true)
        {
            //LOG_F(INFO, "Start Line => %lu", (unsigned long)o->data["rapid_line"]);
            globals->renderer->SetColorByName(c->properties->color, "light-green");
        }
        if (e["event"] == "mouse_out")
        {
            //LOG_F(INFO, "Mouse Out - %s\n", e.dump().c_str());
            globals->renderer->SetColorByName(c->properties->color, "white");
        }
        if (e["event"] == "right_click_up" && left_control_key_is_pressed == true)
        {
            globals->renderer->SetColorByName(c->properties->color, "light-green");
            dialogs_ask_yes_no("Are you sure you want to reverse this paths direction?", &hmi_reverse, NULL, c);
        }
        if (e["event"] == "right_click_down" && left_control_key_is_pressed == true)
        {
            globals->renderer->SetColorByName(c->properties->color, "green");
        }
        if (e["event"] == "left_click_down" && left_control_key_is_pressed == true)
        {
            globals->renderer->SetColorByName(c->properties->color, "green");
        }
        if (e["event"] == "left_click_up" && left_control_key_is_pressed == true)
        {
            globals->renderer->SetColorByName(c->properties->color, "light-green");
            dialogs_ask_yes_no("Are you sure you want to start the program at this path?", &hmi_jumpin, NULL, c);
        }
    }
    if (c->type == "box" && c->properties->id != "cuttable_plane" && c->properties->id != "machine_plane")
    {
        if (e["event"] == "mouse_in")
        {
            globals->renderer->SetColorByName(c->properties->color, "light-green");
        }
        if (e["event"] == "mouse_out")
        {
            globals->renderer->SetColorByName(c->properties->color, "black");
        }
        if (e["event"] == "left_click_down")
        {
            globals->renderer->SetColorByName(c->properties->color, "green");
        }
        if (e["event"] == "left_click_up")
        {
            globals->renderer->SetColorByName(c->properties->color, "light-green");
            hmi_handle_button(c->properties->id);
        }
    }
    else if (left_control_key_is_pressed == false)
    {
        if (e["event"] == "left_click_up")
        {
            nlohmann::json dro_data = motion_controller_get_dro();
            try
            {
                if (dro_data["IN_MOTION"] == false)
                {
                    globals->nc_control_view->way_point_position = globals->mouse_pos_in_matrix_coordinates;
                }
            }
            catch(...)
            {
                LOG_F(ERROR, "Error parsing DRO Data!");
            }
        }
    }
}

void hmi_view_matrix(PrimitiveContainer *p)
{
    if (p->type == "line")
    {
        p->properties->scale = globals->zoom;
        p->properties->offset[0] = globals->pan.x + (globals->nc_control_view->machine_parameters.work_offset[0] * globals->zoom);
        p->properties->offset[1] = globals->pan.y + (globals->nc_control_view->machine_parameters.work_offset[1] * globals->zoom);
    }
    if (p->type == "arc" || p->type == "circle")
    {
        if (p->properties->id == "torch_pointer")
        {
            p->properties->offset[0] = globals->pan.x;
            p->properties->offset[1] = globals->pan.y;
            p->properties->scale = globals->zoom;
            p->circle->radius = 5.0f / globals->zoom;
        }
        else
        {
            p->properties->offset[0] = globals->pan.x;
            p->properties->offset[1] = globals->pan.y;
            p->properties->scale = globals->zoom;
        }
    }
    if (p->type == "box")
    {
        p->properties->scale = globals->zoom;
        p->properties->offset[0] = globals->pan.x;
        p->properties->offset[1] = globals->pan.y;
    }
    if (p->type == "path")
    {
        if (p->properties->id == "gcode" || p->properties->id == "gcode_highlights")
        {
            p->properties->scale = globals->zoom;
            p->properties->offset[0] = globals->pan.x + (globals->nc_control_view->machine_parameters.work_offset[0] * globals->zoom);
            p->properties->offset[1] = globals->pan.y + (globals->nc_control_view->machine_parameters.work_offset[1] * globals->zoom);
        }
        if (p->properties->id == "gcode_arrows")
        {
            p->properties->scale = globals->zoom;
            p->properties->offset[0] = globals->pan.x + (globals->nc_control_view->machine_parameters.work_offset[0] * globals->zoom);
            p->properties->offset[1] = globals->pan.y + (globals->nc_control_view->machine_parameters.work_offset[1] * globals->zoom);
        }
    }
}
std::string to_fixed_string(double n, int d)
{
    std::stringstream stream;
    stream << std::fixed << std::setprecision(d) << n;
    return stream.str();
}

bool hmi_update_timer()
{
    nlohmann::json dro_data = motion_controller_get_dro();
    if (dro_data.contains("STATUS"))
    {
        // { "STATUS": "Idle", "MCS": { "x": 0.000,"y": 0.000,"z": 0.000 }, "WCS": { "x": -4.594,"y": -3.260,"z": 0.000 }, "FEED": 0, "ADC": 0, "IN_MOTION": false, "ARC_OK": true }
        dro.x.work_readout->textval = to_fixed_string(dro_data["WCS"]["x"], 4);
        dro.y.work_readout->textval = to_fixed_string(dro_data["WCS"]["y"], 4);
        dro.z.work_readout->textval = to_fixed_string(dro_data["WCS"]["z"], 4);
        dro.x.absolute_readout->textval = to_fixed_string(dro_data["MCS"]["x"], 4);
        dro.y.absolute_readout->textval = to_fixed_string(dro_data["MCS"]["y"], 4);
        dro.z.absolute_readout->textval = to_fixed_string(dro_data["MCS"]["z"], 4);
        dro.feed->textval = "FEED: " + to_fixed_string(dro_data["FEED"], 1);
        dro.arc_readout->textval = "ARC: " + to_fixed_string(dro_data["ADC"], 0);
        dro.arc_set->textval = "SET: " + to_fixed_string(globals->nc_control_view->machine_parameters.thc_set_value, 0);
        nlohmann::json runtime = motion_controller_get_run_time();
        if (runtime != NULL) dro.run_time->textval = "RUN: " + to_string_strip_zeros((int)runtime["hours"]) + ":" + to_string_strip_zeros((int)runtime["minutes"]) + ":" + to_string_strip_zeros((int)runtime["seconds"]);
        globals->nc_control_view->torch_pointer->center = {(double)dro_data["MCS"]["x"], (double)dro_data["MCS"]["y"]};
        if (motion_controller_is_torch_on())
        {
            hmi_dro_backpane->properties->color[0] = 100;
            hmi_dro_backpane->properties->color[1] = 32;
            hmi_dro_backpane->properties->color[2] = 48;
        }
        else
        {
            hmi_dro_backpane->properties->color[0] = 29;
            hmi_dro_backpane->properties->color[1] = 32;
            hmi_dro_backpane->properties->color[2] = 48;
        }
        if ((bool)dro_data["ARC_OK"] == false)
        {
            globals->renderer->SetColorByName(dro.arc_readout->properties->color, "red");
            //Keep adding points to current highlight path
            if (arc_okay_highlight_path == NULL)
            {
                std::vector<double_point_t> path;
                path.push_back({(double)dro_data["WCS"]["x"], (double)dro_data["WCS"]["y"]});
                arc_okay_highlight_path = globals->renderer->PushPrimitive(new EasyPrimitive::Path(path));
                arc_okay_highlight_path->properties->id = "gcode_highlights";
                arc_okay_highlight_path->is_closed = false;
                arc_okay_highlight_path->width = 3;
                globals->renderer->SetColorByName(arc_okay_highlight_path->properties->color, "green");
                arc_okay_highlight_path->properties->matrix_callback = globals->nc_control_view->view_matrix;
            }
            else
            {
                arc_okay_highlight_path->points.push_back({(double)dro_data["WCS"]["x"], (double)dro_data["WCS"]["y"]});
            }
        }
        else
        {
            arc_okay_highlight_path = NULL;
            dro.arc_readout->properties->color[0] = 247;
            dro.arc_readout->properties->color[1] = 104;
            dro.arc_readout->properties->color[2] = 15;
        }
    }
    return true;
}

void hmi_resize_callback(nlohmann::json e)
{
    //LOG_F(INFO, "(hmi_resize_callback) %s", e.dump().c_str());
    hmi_backpane->bottom_left.x = (globals->renderer->GetWindowSize().x / 2) - hmi_backplane_width;
    hmi_backpane->bottom_left.y = -(globals->renderer->GetWindowSize().y / 2);
    hmi_backpane->width = hmi_backplane_width;
    hmi_backpane->height = globals->renderer->GetWindowSize().y - 15;

    hmi_dro_backpane->bottom_left.x = hmi_backpane->bottom_left.x + 5;
    hmi_dro_backpane->bottom_left.y = (globals->renderer->GetWindowSize().y / 2) - hmi_dro_backplane_height;
    hmi_dro_backpane->width = hmi_backplane_width - 10;
    hmi_dro_backpane->height = hmi_dro_backplane_height - 30;

    float dro_group_x = (float)hmi_dro_backpane->bottom_left.x;
    float dro_group_y = (float)hmi_dro_backpane->bottom_left.y;

    dro_group_y += 10;
    dro.feed->position = {(float)dro_group_x + 5, (float)dro_group_y - dro.feed->height + 5};
    dro.arc_readout->position = {(float)dro_group_x + 80, (float)dro_group_y - dro.arc_readout->height + 5};
    dro.arc_set->position = {(float)dro_group_x + 150, (float)dro_group_y - dro.arc_set->height + 5};
    dro.run_time->position = {(float)dro_group_x + 210, (float)dro_group_y - dro.run_time->height + 5};
    dro_group_y += 55;
    dro.z.label->position = {(float)dro_group_x + 5, (float)dro_group_y - dro.z.label->height};
    dro.z.work_readout->position = {(float)dro_group_x + 5 + 50, (float)dro_group_y - dro.z.label->height};
    dro.z.absolute_readout->position = {(float)dro_group_x + 5 + 220, (float)dro_group_y - dro.z.label->height};
    dro.z.divider->bottom_left = {(float)dro_group_x + 5, (float)dro_group_y - 45};
    dro.z.divider->width = hmi_backplane_width - 20;
    dro.z.divider->height = 5;
    dro_group_y += 55;
    dro.y.label->position = {(float)dro_group_x + 5, (float)dro_group_y - dro.y.label->height};
    dro.y.work_readout->position = {(float)dro_group_x + 5 + 50, (float)dro_group_y - dro.y.label->height};
    dro.y.absolute_readout->position = {(float)dro_group_x + 5 + 220, (float)dro_group_y - dro.y.label->height};
    dro.y.divider->bottom_left = {(float)dro_group_x + 5, (float)dro_group_y - 45};
    dro.y.divider->width = hmi_backplane_width - 20;
    dro.y.divider->height = 5;
    dro_group_y += 55;
    dro.x.label->position = {(float)dro_group_x + 5, (float)dro_group_y - dro.x.label->height};
    dro.x.work_readout->position = {(float)dro_group_x + 5 + 50, (float)dro_group_y - dro.x.label->height};
    dro.x.absolute_readout->position = {(float)dro_group_x + 5 + 220, (float)dro_group_y - dro.x.label->height};
    dro.x.divider->bottom_left = {(float)dro_group_x + 5, (float)dro_group_y - 45};
    dro.x.divider->width = hmi_backplane_width - 20;
    dro.x.divider->height = 5;
    
    hmi_button_backpane->bottom_left.x = hmi_backpane->bottom_left.x + 5;
    hmi_button_backpane->bottom_left.y = hmi_backpane->bottom_left.y + 20;
    hmi_button_backpane->width = hmi_backplane_width - 10;
    hmi_button_backpane->height = globals->renderer->GetWindowSize().y - (hmi_dro_backplane_height + 30);
    
    double button_group_x = hmi_button_backpane->bottom_left.x;
    double button_group_y = hmi_button_backpane->bottom_left.y + hmi_button_backpane->height;
    double button_height = (hmi_button_backpane->height - 10) / (double)button_groups.size();
    double button_width =  hmi_button_backpane->width / 2;
    
    double center_x;
    double center_y; 
    for (int x = 0; x < button_groups.size(); x++)
    {
        button_groups[x].button_one.object->bottom_left.x = button_group_x + 5;
        button_groups[x].button_one.object->bottom_left.y = (button_group_y - 2.5) - button_height;
        button_groups[x].button_one.object->width = button_width - 10;
        button_groups[x].button_one.object->height = button_height - 10;
        center_x = button_groups[x].button_one.object->bottom_left.x + (button_width / 2);
        center_y = button_groups[x].button_one.object->bottom_left.y + (button_height / 2);
        button_groups[x].button_one.label->position = {(float)center_x - (button_groups[x].button_one.label->width / 2.0f) - 5, (float)center_y - (button_groups[x].button_one.label->height) + 5};

        button_group_x += button_width;
        button_groups[x].button_two.object->bottom_left.x = button_group_x + 5;
        button_groups[x].button_two.object->bottom_left.y = (button_group_y - 2.5) - button_height;
        button_groups[x].button_two.object->width = button_width - 10;
        button_groups[x].button_two.object->height = button_height - 10;
        center_x = button_groups[x].button_two.object->bottom_left.x + (button_width / 2);
        center_y = button_groups[x].button_two.object->bottom_left.y + (button_height / 2);

        button_groups[x].button_two.label->position = {(float)center_x - (button_groups[x].button_two.label->width / 2.0f) - 5, (float)center_y - (button_groups[x].button_two.label->height) + 5};
        button_group_x -= button_width;

        button_group_y -= button_height;
    }
}

void hmi_push_button_group(std::string b1, std::string b2)
{
    hmi_button_group_t group;
    group.button_one.name = b1;
    group.button_one.object = globals->renderer->PushPrimitive(new EasyPrimitive::Box({-1000000, -1000000}, 1, 1, 5));
    group.button_one.object->properties->mouse_callback = &hmi_mouse_callback;
    globals->renderer->SetColorByName(group.button_one.object->properties->color, "black");
    group.button_one.object->properties->zindex = 200;
    group.button_one.object->properties->id = b1;
    group.button_one.label = globals->renderer->PushPrimitive(new EasyPrimitive::Text({-1000000, -1000000}, group.button_one.name, 20));
    group.button_one.label->properties->zindex = 210;
    globals->renderer->SetColorByName(group.button_one.label->properties->color, "white");

    group.button_two.name = b2;
    group.button_two.object = globals->renderer->PushPrimitive(new EasyPrimitive::Box({-1000000, -1000000}, 1, 1, 5));
    group.button_two.object->properties->mouse_callback = &hmi_mouse_callback;
    globals->renderer->SetColorByName(group.button_two.object->properties->color, "black");
    group.button_two.object->properties->zindex = 200;
    group.button_two.object->properties->id = b2;
    group.button_two.label = globals->renderer->PushPrimitive(new EasyPrimitive::Text({-1000000, -1000000}, group.button_two.name, 20));
    group.button_two.label->properties->zindex = 210;
    globals->renderer->SetColorByName(group.button_two.label->properties->color, "white");

    button_groups.push_back(group);
}

void hmi_tab_key_up_callback(nlohmann::json e)
{
    if (globals->nc_control_view->way_point_position.x != -1000 && globals->nc_control_view->way_point_position.y != -1000)
    {
        LOG_F(INFO, "Going to waypoint position: X%.4f Y%.4f", globals->nc_control_view->way_point_position.x, globals->nc_control_view->way_point_position.y);
        motion_controller_push_stack("G53 G0 X" + to_string_strip_zeros(globals->nc_control_view->way_point_position.x) + " Y" + to_string_strip_zeros(globals->nc_control_view->way_point_position.y));
        motion_controller_push_stack("M30");
        motion_controller_run_stack();
        globals->nc_control_view->way_point_position.x = -1000;
        globals->nc_control_view->way_point_position.y = -1000;
    }
}
void hmi_escape_key_callback(nlohmann::json e)
{
    hmi_handle_button("Abort");
    globals->nc_control_view->way_point_position = {-1000, -1000};
}
void hmi_up_key_callback(nlohmann::json e)
{
    try
    {
        nlohmann::json dro_data = motion_controller_get_dro();
        if (dro_data["STATUS"] == "Idle")
        {
            if ((int)e["action"] == 1 || (int)e["action"] == 2)
            {
                if (left_control_key_is_pressed == true)
                {
                    std::string dist = std::to_string(globals->nc_control_view->machine_parameters.precise_jog_units);
                    LOG_F(INFO, "Jogging Y positive %s", dist.c_str()); 
                    motion_controller_push_stack("G91 Y" + dist);
                    motion_controller_push_stack("M30");
                    motion_controller_run_stack();
                }
                else
                {
                    //key down
                    LOG_F(INFO, "Jogging Y positive Continuous!");
                    float y_extent = globals->nc_control_view->machine_parameters.machine_extents[1];
                    if(y_extent < 0)
                        motion_controller_push_stack("G53 G0 Y0");
                    else
                        motion_controller_push_stack("G53 G0 Y" + std::to_string(y_extent));
                    motion_controller_push_stack("M30");
                    motion_controller_run_stack();
                }
            }
        }
        if ((int)e["action"] == 0 && left_control_key_is_pressed == false)
        {
            //key up
            LOG_F(INFO, "Cancelling Y positive jog!");
            hmi_handle_button("Abort");
        }
    }
    catch(...)
    {
        //DRO data not available
    }
}
void hmi_down_key_callback(nlohmann::json e)
{
    try
    {
        nlohmann::json dro_data = motion_controller_get_dro();
        if (dro_data["STATUS"] == "Idle")
        {
            if ((int)e["action"] == 1 || (int)e["action"] == 2)
            {
                if (left_control_key_is_pressed == true)
                {
                    std::string dist = std::to_string(globals->nc_control_view->machine_parameters.precise_jog_units);
                    LOG_F(INFO, "Jogging Y negative %s!", dist.c_str());
                    motion_controller_push_stack("G91 Y-" + dist);
                    motion_controller_push_stack("M30");
                    motion_controller_run_stack();
                }
                else
                {
                    //key down
                    LOG_F(INFO, "Jogging Y negative Continuous!");
                    float y_extent = globals->nc_control_view->machine_parameters.machine_extents[1];
                    if(y_extent < 0)
                        motion_controller_push_stack("G53 G0 Y" + std::to_string(y_extent));
                    else
                        motion_controller_push_stack("G53 G0 Y0");
                    motion_controller_push_stack("M30");
                    motion_controller_run_stack();
                }
            }
        }
        if ((int)e["action"] == 0 && left_control_key_is_pressed == false)
        {
            //key up
            LOG_F(INFO, "Cancelling Y negative jog!");
            hmi_handle_button("Abort");
        }
    }
    catch(...)
    {
        //DRO data not available
    }
}
void hmi_right_key_callback(nlohmann::json e)
{
    try
    {
        nlohmann::json dro_data = motion_controller_get_dro();
        if (dro_data["STATUS"] == "Idle")
        {
            if ((int)e["action"] == 1 || (int)e["action"] == 2)
            {
                if (left_control_key_is_pressed == true)
                {
                    std::string dist = std::to_string(globals->nc_control_view->machine_parameters.precise_jog_units);
                    LOG_F(INFO, "Jogging X positive %s!", dist.c_str());
                    motion_controller_push_stack("G91 X" + dist);
                    motion_controller_push_stack("M30");
                    motion_controller_run_stack();
                }
                else
                {
                    //key down
                    LOG_F(INFO, "Jogging X Positive Continuous!");
                    float x_extent = globals->nc_control_view->machine_parameters.machine_extents[0];
                    if(x_extent < 0)
                        motion_controller_push_stack("G53 G0 X0");
                    else
                        motion_controller_push_stack("G53 G0 X" + std::to_string(x_extent));
                    
                    motion_controller_push_stack("M30");
                    motion_controller_run_stack();
                }
            }
        }
        if ((int)e["action"] == 0 && left_control_key_is_pressed == false)
        {
            //key up
            LOG_F(INFO, "Cancelling X Positive jog!");
            hmi_handle_button("Abort");
        }
    }
    catch(...)
    {
        //DRO data not available
    }
}
void hmi_left_key_callback(nlohmann::json e)
{
    try
    {
        nlohmann::json dro_data = motion_controller_get_dro();
        if (dro_data["STATUS"] == "Idle")
        {
            if ((int)e["action"] == 1 || (int)e["action"] == 2)
            {
                if (left_control_key_is_pressed == true)
                {
                    std::string dist = std::to_string(globals->nc_control_view->machine_parameters.precise_jog_units);
                    LOG_F(INFO, "Jogging X negative %s!", dist.c_str());
                    motion_controller_push_stack("G91 X-" + dist);
                    motion_controller_push_stack("M30");
                    motion_controller_run_stack();
                }
                else
                {
                    //key down
                    LOG_F(INFO, "Jogging X Negative Continuous!");
                    float x_extent = globals->nc_control_view->machine_parameters.machine_extents[0];
                    if(x_extent < 0)
                        motion_controller_push_stack("G53 G0 X" + std::to_string(x_extent));
                    else
                        motion_controller_push_stack("G53 G0 X0");
                    motion_controller_push_stack("M30");
                    motion_controller_run_stack();
                }
            }
        }
        if ((int)e["action"] == 0 && left_control_key_is_pressed == false)
        {
            //key up
            LOG_F(INFO, "Cancelling X Negative jog!");
            hmi_handle_button("Abort");
        }
    }
    catch(...)
    {
        //DRO data not available
    }
}
void hmi_page_up_key_callback(nlohmann::json e)
{
    if ((int)e["action"] == 1)
    {
        motion_controller_send_rt('>');
    }
    else if ((int)e["action"] == 0)
    {
        motion_controller_send_rt('^');
    }
    /*try
    {
        nlohmann::json dro_data = motion_controller_get_dro();
        if (dro_data["STATUS"] == "Idle")
        {
            if ((int)e["action"] == 1 || (int)e["action"] == 2)
            {
                if (left_control_key_is_pressed == true)
                {
                    LOG_F(INFO, "Jogging Z positive 0.010!");
                    motion_controller_push_stack("G91 Z0.010");
                    motion_controller_push_stack("M30");
                    motion_controller_run_stack();
                }
                else
                {
                    //key down
                    LOG_F(INFO, "Jogging Z Positive Continuous!");
                    motion_controller_push_stack("G53 G0 Z0");
                    motion_controller_push_stack("M30");
                    motion_controller_run_stack();
                }
            }
        }
        if ((int)e["action"] == 0 && left_control_key_is_pressed == false)
        {
            //key up
            LOG_F(INFO, "Cancelling Z Positive jog!");
            hmi_handle_button("Abort");
        }
    }
    catch(...)
    {
        //DRO data not available
    }*/
}
void hmi_page_down_key_callback(nlohmann::json e)
{
    if ((int)e["action"] == 1)
    {
        motion_controller_send_rt('<');
    }
    else if ((int)e["action"] == 0)
    {
        motion_controller_send_rt('^');
    }
    /*try
    {
        nlohmann::json dro_data = motion_controller_get_dro();
        if (dro_data["STATUS"] == "Idle")
        {
            if ((int)e["action"] == 1 || (int)e["action"] == 2)
            {
                if (left_control_key_is_pressed == true)
                {
                    LOG_F(INFO, "Jogging Z negative 0.010!");
                    motion_controller_push_stack("G91 Z-0.010");
                    motion_controller_push_stack("M30");
                    motion_controller_run_stack();
                }
                else
                {
                    //key down
                    LOG_F(INFO, "Jogging Z Negative Continuous!");
                    motion_controller_push_stack("G53 G0 Z" + std::to_string(globals->nc_control_view->machine_parameters.machine_extents[2]));
                    motion_controller_push_stack("M30");
                    motion_controller_run_stack();
                }
            }
        }
        if ((int)e["action"] == 0 && left_control_key_is_pressed == false)
        {
            //key up
            LOG_F(INFO, "Cancelling Z Negative jog!");
            hmi_handle_button("Abort");
        }
    }
    catch(...)
    {
        //DRO data not available
    }*/
}
void hmi_control_key_callback(nlohmann::json e)
{
    if ((int)e["action"] == 0)
    {
        left_control_key_is_pressed = false;
    }
    if ((int)e["action"] == 1)
    {
        left_control_key_is_pressed = true;
    }
}
void hmi_mouse_motion_callback(nlohmann::json e)
{
    double_point_t point = {(double)e["pos"]["x"], (double)e["pos"]["y"]};
    if (globals->move_view)
    {
        globals->pan.x += (point.x - globals->mouse_pos_in_screen_coordinates.x);
        globals->pan.y += (point.y - globals->mouse_pos_in_screen_coordinates.y);
    }
    globals->mouse_pos_in_screen_coordinates = point;
    globals->mouse_pos_in_matrix_coordinates = {
        ((double)e["pos"]["x"] / globals->zoom) - (globals->pan.x / globals->zoom),
        ((double)e["pos"]["y"] / globals->zoom) - (globals->pan.y / globals->zoom)
    };
}
void hmi_init()
{
    globals->nc_control_view->way_point_position = {-1000, -1000};
    
    globals->nc_control_view->machine_plane = globals->renderer->PushPrimitive(new EasyPrimitive::Box({0, 0}, globals->nc_control_view->machine_parameters.machine_extents[0], globals->nc_control_view->machine_parameters.machine_extents[1], 0));
    globals->nc_control_view->machine_plane->properties->id = "machine_plane";
    globals->nc_control_view->machine_plane->properties->zindex = -20;
    globals->nc_control_view->machine_plane->properties->color[0] = globals->nc_control_view->preferences.machine_plane_color[0] * 255.0f;
    globals->nc_control_view->machine_plane->properties->color[1] = globals->nc_control_view->preferences.machine_plane_color[1] * 255.0f;
    globals->nc_control_view->machine_plane->properties->color[2] = globals->nc_control_view->preferences.machine_plane_color[2] * 255.0f;
    globals->nc_control_view->machine_plane->properties->matrix_callback = globals->nc_control_view->view_matrix;
    globals->nc_control_view->machine_plane->properties->mouse_callback = &hmi_mouse_callback;


    globals->nc_control_view->cuttable_plane = globals->renderer->PushPrimitive(new EasyPrimitive::Box({globals->nc_control_view->machine_parameters.cutting_extents[0], globals->nc_control_view->machine_parameters.cutting_extents[1]}, (globals->nc_control_view->machine_parameters.machine_extents[0] + globals->nc_control_view->machine_parameters.cutting_extents[2]) - globals->nc_control_view->machine_parameters.cutting_extents[0], (globals->nc_control_view->machine_parameters.machine_extents[1] + globals->nc_control_view->machine_parameters.cutting_extents[3]) - globals->nc_control_view->machine_parameters.cutting_extents[1], 0));
    globals->nc_control_view->cuttable_plane->properties->id = "cuttable_plane";
    globals->nc_control_view->cuttable_plane->properties->zindex = -10;
    globals->nc_control_view->cuttable_plane->properties->color[0] = globals->nc_control_view->preferences.cuttable_plane_color[0] * 255.0f;
    globals->nc_control_view->cuttable_plane->properties->color[1] = globals->nc_control_view->preferences.cuttable_plane_color[1] * 255.0f;
    globals->nc_control_view->cuttable_plane->properties->color[2] = globals->nc_control_view->preferences.cuttable_plane_color[2] * 255.0f;
    globals->nc_control_view->cuttable_plane->properties->matrix_callback = globals->nc_control_view->view_matrix;
    
    hmi_backpane = globals->renderer->PushPrimitive(new EasyPrimitive::Box({-100000, -100000}, 1, 1, 5));
    hmi_backpane->properties->color[0] = 25;
    hmi_backpane->properties->color[1] = 44;
    hmi_backpane->properties->color[2] = 71;
    hmi_backpane->properties->zindex = 100;

  
    hmi_dro_backpane = globals->renderer->PushPrimitive(new EasyPrimitive::Box({-100000, -100000}, 1, 1, 5));
    hmi_dro_backpane->properties->color[0] = 29;
    hmi_dro_backpane->properties->color[1] = 32;
    hmi_dro_backpane->properties->color[2] = 48;
    hmi_dro_backpane->properties->zindex = 110;

    hmi_button_backpane = globals->renderer->PushPrimitive(new EasyPrimitive::Box({-100000, -100000}, 1, 1, 5));
    hmi_button_backpane->properties->color[0] = 29;
    hmi_button_backpane->properties->color[1] = 32;
    hmi_button_backpane->properties->color[2] = 48;
    hmi_button_backpane->properties->zindex = 115;

    if (globals->nc_control_view->machine_parameters.machine_type == 0) //Plasma
    {
        hmi_push_button_group("Zero X", "Zero Y");
        hmi_push_button_group("Touch", "Retract");
        hmi_push_button_group("Fit", "Clean");
        hmi_push_button_group("Wpos", "Park");
        hmi_push_button_group("Test Run", "ATHC");
        hmi_push_button_group("Run", "Abort");
    }
    if (globals->nc_control_view->machine_parameters.machine_type == 1) //Router
    {
        hmi_push_button_group("Zero X", "Zero Y");
        hmi_push_button_group("Zero Z", "Retract");
        hmi_push_button_group("Fit", "Clean");
        hmi_push_button_group("Wpos", "Park");
        hmi_push_button_group("Spindle On", "Spindle Off");
        hmi_push_button_group("Run", "Abort");
    }
    
    dro.x.label = globals->renderer->PushPrimitive(new EasyPrimitive::Text({-100000, -100000}, "X", 50));
    dro.x.label->properties->zindex = 210;
    globals->renderer->SetColorByName(dro.x.label->properties->color, "white");
    dro.x.work_readout = globals->renderer->PushPrimitive(new EasyPrimitive::Text({-100000, -100000}, "0.0000", 40));
    dro.x.work_readout->properties->zindex = 210;
    dro.x.work_readout->properties->color[0] = 10;
    dro.x.work_readout->properties->color[1] = 150;
    dro.x.work_readout->properties->color[2] = 10;
    dro.x.absolute_readout = globals->renderer->PushPrimitive(new EasyPrimitive::Text({-100000, -100000}, "0.0000", 15));
    dro.x.absolute_readout->properties->zindex = 210;
    dro.x.absolute_readout->properties->color[0] = 247;
    dro.x.absolute_readout->properties->color[1] = 104;
    dro.x.absolute_readout->properties->color[2] = 15;
    dro.x.divider = globals->renderer->PushPrimitive(new EasyPrimitive::Box({-100000, -10000}, 1, 1, 3));
    dro.x.divider->properties->zindex = 150;
    globals->renderer->SetColorByName(dro.x.divider->properties->color, "black");


    dro.y.label = globals->renderer->PushPrimitive(new EasyPrimitive::Text({-100000, -100000}, "Y", 50));
    dro.y.label->properties->zindex = 210;
    globals->renderer->SetColorByName(dro.y.label->properties->color, "white");
    dro.y.work_readout = globals->renderer->PushPrimitive(new EasyPrimitive::Text({-100000, -100000}, "0.0000", 40));
    dro.y.work_readout->properties->zindex = 210;
    dro.y.work_readout->properties->color[0] = 10;
    dro.y.work_readout->properties->color[1] = 150;
    dro.y.work_readout->properties->color[2] = 10;
    dro.y.absolute_readout = globals->renderer->PushPrimitive(new EasyPrimitive::Text({-100000, -100000}, "0.0000", 15));
    dro.y.absolute_readout->properties->zindex = 210;
    dro.y.absolute_readout->properties->color[0] = 247;
    dro.y.absolute_readout->properties->color[1] = 104;
    dro.y.absolute_readout->properties->color[2] = 15;
    dro.y.divider = globals->renderer->PushPrimitive(new EasyPrimitive::Box({-100000, -10000}, 1, 1, 3));
    dro.y.divider->properties->zindex = 150;
    globals->renderer->SetColorByName(dro.y.divider->properties->color, "black");

    dro.z.label = globals->renderer->PushPrimitive(new EasyPrimitive::Text({-100000, -100000}, "Z", 50));
    dro.z.label->properties->zindex = 210;
    globals->renderer->SetColorByName(dro.z.label->properties->color, "white");
    dro.z.work_readout = globals->renderer->PushPrimitive(new EasyPrimitive::Text({-100000, -100000}, "0.0000", 40));
    dro.z.work_readout->properties->zindex = 210;
    dro.z.work_readout->properties->color[0] = 10;
    dro.z.work_readout->properties->color[1] = 150;
    dro.z.work_readout->properties->color[2] = 10;
    dro.z.absolute_readout = globals->renderer->PushPrimitive(new EasyPrimitive::Text({-100000, -100000}, "0.0000", 15));
    dro.z.absolute_readout->properties->zindex = 210;
    dro.z.absolute_readout->properties->color[0] = 247;
    dro.z.absolute_readout->properties->color[1] = 104;
    dro.z.absolute_readout->properties->color[2] = 15;
    dro.z.divider = globals->renderer->PushPrimitive(new EasyPrimitive::Box({-100000, -10000}, 1, 1, 3));
    dro.z.divider->properties->zindex = 150;
    globals->renderer->SetColorByName(dro.z.divider->properties->color, "black");

    dro.feed = globals->renderer->PushPrimitive(new EasyPrimitive::Text({-100000, -100000}, "FEED: 0", 12));
    dro.feed->properties->zindex = 210;
    dro.feed->properties->color[0] = 247;
    dro.feed->properties->color[1] = 104;
    dro.feed->properties->color[2] = 15;

    dro.arc_readout = globals->renderer->PushPrimitive(new EasyPrimitive::Text({-100000, -100000}, "ARC: 0.0", 12));
    dro.arc_readout->properties->zindex = 210;
    dro.arc_readout->properties->color[0] = 247;
    dro.arc_readout->properties->color[1] = 104;
    dro.arc_readout->properties->color[2] = 15;


    dro.arc_set = globals->renderer->PushPrimitive(new EasyPrimitive::Text({-100000, -100000}, "SET: 0", 12));
    dro.arc_set->properties->zindex = 210;
    dro.arc_set->properties->color[0] = 247;
    dro.arc_set->properties->color[1] = 104;
    dro.arc_set->properties->color[2] = 15;

    if (globals->nc_control_view->machine_parameters.machine_type == 1) //Router
    {
        dro.arc_readout->properties->visible = false;
        dro.arc_set->properties->visible = false;
    }

    dro.run_time = globals->renderer->PushPrimitive(new EasyPrimitive::Text({-100000, -100000}, "RUN: 0:0:0", 12));
    dro.run_time->properties->zindex = 210;
    dro.run_time->properties->color[0] = 247;
    dro.run_time->properties->color[1] = 104;
    dro.run_time->properties->color[2] = 15;

    globals->nc_control_view->torch_pointer = globals->renderer->PushPrimitive(new EasyPrimitive::Circle({-100000, -100000}, 5));
    globals->nc_control_view->torch_pointer->properties->zindex = 500;
    globals->nc_control_view->torch_pointer->properties->id = "torch_pointer";
    globals->renderer->SetColorByName(globals->nc_control_view->torch_pointer->properties->color, "green");
    globals->nc_control_view->torch_pointer->properties->matrix_callback = globals->nc_control_view->view_matrix;
   
    globals->renderer->PushEvent("Tab", "keyup", hmi_tab_key_up_callback);
    globals->renderer->PushEvent("none", "window_resize", hmi_resize_callback);
    globals->renderer->PushEvent("Escape", "keyup", hmi_escape_key_callback);

    globals->renderer->PushEvent("Up", "keydown", hmi_up_key_callback);
    globals->renderer->PushEvent("Up", "keyup", hmi_up_key_callback);
    globals->renderer->PushEvent("Up", "repeat", hmi_up_key_callback);

    globals->renderer->PushEvent("Down", "keydown", hmi_down_key_callback);
    globals->renderer->PushEvent("Down", "keyup", hmi_down_key_callback);

    globals->renderer->PushEvent("Left", "keydown", hmi_left_key_callback);
    globals->renderer->PushEvent("Left", "keyup", hmi_left_key_callback);

    globals->renderer->PushEvent("Right", "keydown", hmi_right_key_callback);
    globals->renderer->PushEvent("Right", "keyup", hmi_right_key_callback);

    globals->renderer->PushEvent("PgUp", "keydown", hmi_page_up_key_callback);
    globals->renderer->PushEvent("PgUp", "keyup", hmi_page_up_key_callback);

    globals->renderer->PushEvent("PgDown", "keydown", hmi_page_down_key_callback);
    globals->renderer->PushEvent("PgDown", "keyup", hmi_page_down_key_callback);

    globals->renderer->PushEvent("LeftControl", "keyup", hmi_control_key_callback);
    globals->renderer->PushEvent("LeftControl", "keydown", hmi_control_key_callback);

    globals->renderer->PushEvent("none", "mouse_move", hmi_mouse_motion_callback);

    globals->renderer->PushTimer(100, hmi_update_timer);
    
    hmi_handle_button("Fit");
}
