#include "dialogs.h"
#include <EasyRender/gui/ImGuiFileDialog.h>
#include "../motion_control/motion_control.h"
#include "ncControlView/ncControlView.h"
#include "../gcode/gcode.h"
#include <algorithm>

EasyRender::EasyRenderGui *thc_window_handle;
EasyRender::EasyRenderGui *preferences_window_handle;
EasyRender::EasyRenderGui *machine_parameters_window_handle;
EasyRender::EasyRenderGui *progress_window_handle;
float progress = 0.0f;
EasyRender::EasyRenderGui *info_window_handle;
EasyRender::EasyRenderGui *controller_offline_window_handle;
EasyRender::EasyRenderGui *controller_alarm_window_handle;
EasyRender::EasyRenderGui *controller_homing_window_handle;
std::string controller_alarm_text;
std::string info;
EasyRender::EasyRenderGui *ask_window_handle;
std::string ask_text;
void (*ask_window_yes_callback)(PrimitiveContainer *);
void (*ask_window_no_callback)(PrimitiveContainer *);
PrimitiveContainer *ask_window_args;

void dialogs_file_open()
{
    if (ImGuiFileDialog::Instance()->Display("ChooseFileDlgKey", ImGuiWindowFlags_NoCollapse, ImVec2(600, 500)))
    {
        if (ImGuiFileDialog::Instance()->IsOk())
        {
            std::string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
            std::string filePath = ImGuiFileDialog::Instance()->GetCurrentPath();
            LOG_F(INFO, "File Path: %s, File Path Name: %s", filePath.c_str(), filePathName.c_str());
            std::ofstream out(globals->renderer->GetConfigDirectory() + "last_gcode_open_path.conf");
            out << filePath;
            out << "/";
            out.close();
            if (gcode_open_file(filePathName))
            {
                globals->renderer->PushTimer(0, &gcode_parse_timer);
            }
        }
        ImGuiFileDialog::Instance()->Close();
    }
}

void dialogs_show_preferences(bool s) 
{
    preferences_window_handle->visible = s;
}
void dialogs_preferences()
{
    ImGui::Begin("Preferences", &preferences_window_handle->visible, ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::ColorEdit3("Background Color", globals->nc_control_view->preferences.background_color);
    ImGui::ColorEdit3("Machine Plane Color", globals->nc_control_view->preferences.machine_plane_color);
    ImGui::ColorEdit3("Cuttable Plane Color", globals->nc_control_view->preferences.cuttable_plane_color);
    ImGui::InputInt2("Default Window Size", globals->nc_control_view->preferences.window_size);
    ImGui::SameLine();
    if (ImGui::Button("<= Current Size"))
    {
        globals->nc_control_view->preferences.window_size[0] = (int)globals->renderer->GetWindowSize().x;
        globals->nc_control_view->preferences.window_size[1] = (int)globals->renderer->GetWindowSize().y;
    }
    ImGui::Spacing();
    if (ImGui::Button("OK"))
    {
        globals->renderer->SetClearColor(globals->nc_control_view->preferences.background_color[0] * 255.0f, globals->nc_control_view->preferences.background_color[1] * 255.0f, globals->nc_control_view->preferences.background_color[2] * 255.0f);

        globals->nc_control_view->machine_plane->properties->color[0] = globals->nc_control_view->preferences.machine_plane_color[0] * 255.0f;
        globals->nc_control_view->machine_plane->properties->color[1] = globals->nc_control_view->preferences.machine_plane_color[1] * 255.0f;
        globals->nc_control_view->machine_plane->properties->color[2] = globals->nc_control_view->preferences.machine_plane_color[2] * 255.0f;

        globals->nc_control_view->cuttable_plane->properties->color[0] = globals->nc_control_view->preferences.cuttable_plane_color[0] * 255.0f;
        globals->nc_control_view->cuttable_plane->properties->color[1] = globals->nc_control_view->preferences.cuttable_plane_color[1] * 255.0f;
        globals->nc_control_view->cuttable_plane->properties->color[2] = globals->nc_control_view->preferences.cuttable_plane_color[2] * 255.0f;

        //Write preferences to file
        nlohmann::json preferences;
        preferences["background_color"]["r"] = globals->nc_control_view->preferences.background_color[0];
        preferences["background_color"]["g"] = globals->nc_control_view->preferences.background_color[1];
        preferences["background_color"]["b"] = globals->nc_control_view->preferences.background_color[2];

        preferences["machine_plane_color"]["r"] = globals->nc_control_view->preferences.machine_plane_color[0];
        preferences["machine_plane_color"]["g"] = globals->nc_control_view->preferences.machine_plane_color[1];
        preferences["machine_plane_color"]["b"] = globals->nc_control_view->preferences.machine_plane_color[2];

        preferences["cuttable_plane_color"]["r"] = globals->nc_control_view->preferences.cuttable_plane_color[0];
        preferences["cuttable_plane_color"]["g"] = globals->nc_control_view->preferences.cuttable_plane_color[1];
        preferences["cuttable_plane_color"]["b"] = globals->nc_control_view->preferences.cuttable_plane_color[2];

        preferences["window_width"] = globals->nc_control_view->preferences.window_size[0];
        preferences["window_height"] = globals->nc_control_view->preferences.window_size[1];

        std::ofstream out(globals->renderer->GetConfigDirectory() + "preferences.json");
        out << preferences.dump();
        out.close();
        dialogs_show_preferences(false);
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel"))
    {
        dialogs_show_preferences(false);
    }
    ImGui::End();
}

void dialogs_show_machine_parameters(bool s)
{
    machine_parameters_window_handle->visible = s;
}
void dialogs_machine_parameters()
{
    static ncControlView::machine_parameters_data_t temp_parameters = globals->nc_control_view->machine_parameters;
    ImGui::Begin("Machine Parameters", &machine_parameters_window_handle->visible, ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::Separator();
    ImGui::Text("Machine extents is the max distance each axis can travel freely. X0 is the X negative stop, Y0 is Y negative stop, and Z0 is Z positive stop!");
    ImGui::InputFloat3("Machine Extents (X, Y, Z). Z must be negative for THC to work.", temp_parameters.machine_extents);
    
    ImGui::Separator();
    ImGui::Text("Cutting extents are used to prevent accidentally cutting onto machine frames or generally any area outside of where cutting should happen.\nX1,Y1 is bottom left hand corner and X2, Y2 is top right hand corner, values are incremented off of machine extents, i.e X2 and Y2 should be negative");
    ImGui::InputFloat4("Cutting Extents (X1, Y1, X2, Y2)", temp_parameters.cutting_extents);
    ImGui::Separator();
    ImGui::Text("Scale is in steps per your desired units. E.G. To use machine in\nInches, set scales to steps per inch.");
    ImGui::InputFloat3("Axis Scale (X, Y, Z)", temp_parameters.axis_scale);
    ImGui::Text("Manual precision jogging (ctrl + [MOVE]) can be used for small movements. The distance can be adjusted below.");
    ImGui::InputFloat("Precision jogging distance (units)", &temp_parameters.precise_jog_units);
    ImGui::Separator();
    ImGui::Checkbox("Invert X", &temp_parameters.axis_invert[0]);
    ImGui::SameLine();
    ImGui::Checkbox("Invert Y1", &temp_parameters.axis_invert[1]);
    ImGui::SameLine();
    ImGui::Checkbox("Invert Y2", &temp_parameters.axis_invert[2]);
    ImGui::SameLine();
    ImGui::Checkbox("Invert Z", &temp_parameters.axis_invert[3]);
    ImGui::SameLine();
    ImGui::Checkbox("Invert stepper enable bit", &temp_parameters.invert_step_enable);
    ImGui::Separator();

    ImGui::Text("Homing and Soft Limits. Homing must be enabled to use soft limits!");
    ImGui::Checkbox("Enable Soft Limits", &temp_parameters.soft_limits_enabled);
    ImGui::Checkbox("Enable Homing", &temp_parameters.homing_enabled);
    ImGui::Checkbox("Invert homing direction X", &temp_parameters.homing_dir_invert[0]);
    ImGui::SameLine();
    ImGui::Checkbox("Invert homing direction Y1", &temp_parameters.homing_dir_invert[1]);
    ImGui::SameLine();
    ImGui::Checkbox("Invert homing direction Y2", &temp_parameters.homing_dir_invert[2]);
    ImGui::SameLine();
    ImGui::Checkbox("Invert homing direction Z", &temp_parameters.homing_dir_invert[3]);
    ImGui::Checkbox("Invert limit pins", &temp_parameters.invert_limit_pins);
    ImGui::InputFloat("Homing Feedrate", &temp_parameters.homing_feed);
    ImGui::InputFloat("Homing Seekrate", &temp_parameters.homing_seek);
    ImGui::InputFloat("Homing Debounce", &temp_parameters.homing_debounce);
    ImGui::InputFloat("Homing Pull-Off", &temp_parameters.homing_pull_off);


    ImGui::Separator();
    ImGui::Text("Each axis maximum allowable velocity in units per minute. E.g. inch/min or mm/min");
    ImGui::InputFloat3("Max Velocity (X, Y, Z)", temp_parameters.max_vel);
    ImGui::Separator();
    ImGui::Text("Each axis maximum allowable acceleration in units per second squared");
    ImGui::InputFloat3("Max Acceleration (X, Y, Z)", temp_parameters.max_accel);
    ImGui::InputFloat("Junction Deviation", &temp_parameters.junction_deviation);
    ImGui::Separator();
    ImGui::Text("The distance the floating head moves off of it's gravity stop to where it closes the probe switch. Ohmic sensing should have 0.0000 value");
    ImGui::InputFloat("Floating Head Backlash", &temp_parameters.floating_head_backlash);
    ImGui::Separator();
    ImGui::Text("Velocity in units per minute when probing the torch");
    ImGui::InputFloat("Z Probe Feed", &temp_parameters.z_probe_feedrate);
    ImGui::Separator();
    ImGui::Text("The amount of time after motion starts after a probing cycle to consider the arc stabilized. This will affect THC accuracy!");
    ImGui::InputFloat("Arc Stabilization Time (ms)", &temp_parameters.arc_stabilization_time);
    ImGui::Separator();
    ImGui::Text("The calibrated arc voltage divider of your system. This affects the DRO and THC settings. 1 means raw arc voltage, 50 means a 1:50 divider is used.");
    ImGui::InputFloat("Arc Voltage Divider", &temp_parameters.arc_voltage_divider);

    ImGui::Spacing();
    if (ImGui::Button("Save and write to controller"))
    {
        bool skip_save = false;
        if (temp_parameters.arc_voltage_divider < 1.f or temp_parameters.arc_voltage_divider > 500.f)
        {
            dialogs_set_info_value("The arc voltage divider is insane. Pick something reasonable.");
            dialogs_show_info_window(true);
            skip_save = true;
        }
        else if (globals->nc_control_view->machine_parameters.machine_extents[2] > 0.f)
        {
            dialogs_set_info_value("The NanoCut THC expects a negative Z extent. Please edit your input and try again.");
            dialogs_show_info_window(true);
            skip_save = true;
        }

        if (not skip_save)
        {
            globals->nc_control_view->machine_parameters = temp_parameters;
            motion_controller_save_machine_parameters();
            motion_controller_write_parameters_to_controller();
            dialogs_show_machine_parameters(false);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel"))
    {
        dialogs_show_machine_parameters(false);
    }
    ImGui::End();
}

void dialogs_show_progress_window(bool s)
{
    progress_window_handle->visible = s;
}
void dialogs_set_progress_value(float p)
{
    progress = p;
}
void dialogs_progress_window()
{
    ImGui::Begin("Progress", &progress_window_handle->visible, ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::ProgressBar(progress, ImVec2(0.0f, 0.0f));
    ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
    ImGui::Text("Progress");
    ImGui::End();
}

void dialogs_show_info_window(bool s)
{
    info_window_handle->visible = s;
}
void dialogs_set_info_value(std::string i)
{
    info = i;
    LOG_F(WARNING, "Info Window => %s", info.c_str());
    dialogs_show_info_window(true);
}
void dialogs_info_window()
{
    ImGui::Begin("Info", &info_window_handle->visible, ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::Text("%s", info.c_str());
    if (ImGui::Button("Close"))
    {
        dialogs_show_info_window(false);
    }
    ImGui::End();
}

void dialogs_show_controller_offline_window(bool s)
{
    controller_offline_window_handle->visible = s;
}
void dialogs_set_controller_offline_window_value(std::string i)
{
    info = i;
    LOG_F(WARNING, "Controller Offline Window => %s", info.c_str());
    dialogs_show_controller_offline_window(true);
}
void dialogs_controller_offline_window()
{
    ImGui::Begin("Controller Offline", &controller_offline_window_handle->visible, ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::Text("The motion controller is offline! Verify that USB connection is secure!");
    ImGui::End();
}

void dialogs_show_controller_alarm_window(bool s)
{
    controller_alarm_window_handle->visible = s;
}
void dialogs_set_controller_alarm_value(std::string i)
{
    controller_alarm_text = i;
}
void dialogs_controller_alarm_window()
{
    ImGui::Begin("Controller Alarm", &controller_alarm_window_handle->visible, ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::Text("%s", controller_alarm_text.c_str());
    if (ImGui::Button("Clear Alarm"))
    {
        LOG_F(WARNING, "User cleared alarm!");
        motion_controller_trigger_reset();
        dialogs_show_controller_alarm_window(false);
    }
    ImGui::End();
}
bool dialogs_controller_alarm_window_visible()
{
    return controller_alarm_window_handle->visible;
}

void dialogs_show_controller_homing_window(bool s)
{
    controller_homing_window_handle->visible = s;
}
void dialogs_controller_homing_window()
{
    ImGui::Begin("Home Machine", &controller_homing_window_handle->visible, ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::Text("Machine needs to be homed. Click on Home button when ready!");
    if (ImGui::Button("Home"))
    {
        LOG_F(WARNING, "User initiated homing cycle!");
        motion_controller_set_needs_homed(false);
        motion_controller_send("$H");
        dialogs_show_controller_homing_window(false);
    }
    ImGui::End();
}

void dialogs_ask_yes_no(std::string a, void (*y)(PrimitiveContainer *), void (*n)(PrimitiveContainer *), PrimitiveContainer *args)
{
    ask_text = a;
    ask_window_yes_callback = y;
    ask_window_no_callback = n;
    ask_window_args = args;
    LOG_F(INFO, "Ask Window => %s", ask_text.c_str());
    ask_window_handle->visible = true;
}
void dialogs_ask_window()
{
    ImGui::Begin("Question?", &ask_window_handle->visible, ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::Text("%s", ask_text.c_str());
    if (ImGui::Button("Yes"))
    {
        if (ask_window_yes_callback != NULL) ask_window_yes_callback(ask_window_args);
        ask_window_handle->visible = false;
    }
    ImGui::SameLine();
    if (ImGui::Button("No"))
    {
        if (ask_window_no_callback != NULL) ask_window_no_callback(ask_window_args);
        ask_window_handle->visible = false;
    }
    ImGui::End();
}

void dialogs_show_thc_window(bool s)
{
    thc_window_handle->visible = s;
}
void dialogs_thc_window()
{
    ImGui::Begin("Torch Height Control", &thc_window_handle->visible, ImGuiWindowFlags_AlwaysAutoResize);
    //ImGui::Text("Smart THC mode automatically sets the \"set voltage\" that is measured shortly\nafter the torch pierces and moves negative to cut height.\n By doing this, the THC should maintain approximately\nthe same cut height as is set in the cutting parameters.");
    //ImGui::BulletText("Smart THC Should not be used on thin materials that warp when the torch touches off!");
    //ImGui::Separator();
    //ImGui::Checkbox("Turn on Auto THC Setting Mode", &globals->nc_control_view->machine_parameters.smart_thc_on);
    ImGui::Separator();
    //ImGui::Text("When Smart THC is off (Not Checked) the THC set voltage set below will be used");
    ImGui::Text("0 = THC OFF, Max value is 1024. Press Tab to manually enter a value");
    ImGui::Separator();
    ImGui::SliderInt("Set Voltage", &globals->nc_control_view->machine_parameters.thc_set_value, 0, 1024);
    if (ImGui::Button("Close"))
    {
        dialogs_show_thc_window(false);
    }
    ImGui::End();
}

void dialogs_init()
{
    globals->renderer->PushGui(true, dialogs_file_open);
    preferences_window_handle = globals->renderer->PushGui(false, dialogs_preferences);
    machine_parameters_window_handle = globals->renderer->PushGui(false, dialogs_machine_parameters);
    progress_window_handle = globals->renderer->PushGui(false, dialogs_progress_window);
    info_window_handle = globals->renderer->PushGui(false, dialogs_info_window);
    controller_offline_window_handle = globals->renderer->PushGui(false, dialogs_controller_offline_window);
    controller_alarm_window_handle = globals->renderer->PushGui(false, dialogs_controller_alarm_window);
    controller_homing_window_handle = globals->renderer->PushGui(false, dialogs_controller_homing_window);
    ask_window_handle = globals->renderer->PushGui(false, dialogs_ask_window);
    thc_window_handle = globals->renderer->PushGui(false, dialogs_thc_window);
}
