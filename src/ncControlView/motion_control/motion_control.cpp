#include <sstream>
#include <iomanip>
#include "motion_control.h"
#include "ncControlView/ncControlView.h"
#include "../dialogs/dialogs.h"
#include "../util.h"

#define DEFAULT_PIERCE_HEIGHT SCALE(2.8f)
#define DEFAULT_CUT_HEIGHT SCALE(1.75f)
#define DEFAULT_PIERCE_DELAY_S 1.2f

easy_serial motion_controller("arduino|USB", byte_handler, line_handler);

void removeSubstrs(std::string& s, std::string p) { 
  std::string::size_type n = p.length();
  for (std::string::size_type i = s.find(p);
      i != std::string::npos;
      i = s.find(p))
      s.erase(i, n);
}
std::vector<std::string> split(std::string str, char delimiter)
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
nlohmann::json dro_data;
nlohmann::json callback_args;
bool controller_ready;
void (*okay_callback)() = NULL;
void (*probe_callback)() = NULL;
void (*motion_sync_callback)() = NULL;
void (*arc_okay_callback)() = NULL;
int arc_retry_count = 0;
std::vector<std::string> gcode_stack;
bool torch_on;
unsigned long torch_on_timer;
unsigned long arc_okay_timer;
unsigned long program_run_time;
unsigned long last_checksum_error = -1;
bool abort_pending;
bool handling_crash;
bool needs_homed;

/*
    Callbacks that get called via okay_callback
*/
void program_finished()
{
    LOG_F(INFO, "M30 Program finished!");
    motion_controller_log_runtime();
    program_run_time = 0;
    arc_retry_count = 0;
    okay_callback = NULL;
    probe_callback = NULL;
    motion_sync_callback = NULL;
    arc_okay_callback = NULL;
    motion_controller_clear_stack();
}
bool arc_okay_expire_timer()
{
    if (abort_pending == true) return false; //Pending abort must cancel this timer otherwise aborts would be ignored in arc retry loop!
    /*
        if this function is called, torch fired but controller never seen an arc okay signal after 3 seconds...
    */
    if (arc_okay_callback != NULL) //Only run this code if the arc_okay_callback was not processed, meaning the timr event fired and the arc never transfered after n seconds
    {
        okay_callback = &run_pop;
        probe_callback = NULL;
        motion_sync_callback = NULL;
        arc_okay_callback = NULL;
        torch_on = false;
        //Remember that inserting at the top of list means its the next code to run, meaning
        //This list that we are inserting is ran from bottom to top or LIFO mode
        gcode_stack.insert(gcode_stack.begin(), "fire_torch " + to_string_strip_zeros((double)callback_args["pierce_height"]) + " " + to_string_strip_zeros((double)callback_args["pierce_delay"]) + " " + to_string_strip_zeros((double)callback_args["cut_height"]));
        gcode_stack.insert(gcode_stack.begin(), "G53 G0 Z0"); //Retract
        gcode_stack.insert(gcode_stack.begin(), "G90"); //Absolute mode
        gcode_stack.insert(gcode_stack.begin(), "M5"); //Torch Off
        
        arc_retry_count++;
        LOG_F(WARNING, "No arc OK received! Running retry callback => arc_okay_expire_timer() - retry count: %d", arc_retry_count);
        run_pop();
    }
    return false; //Don't repeat
}
void raise_to_cut_height_and_run_program()
{
    okay_callback = &run_pop;
    probe_callback = NULL;
    arc_okay_callback = NULL;
    arc_retry_count = 0;
    //Remember that inserting at the top of list means its the next code to run, meaning
    //This list that we are inserting is ran from bottom to top or LIFO mode
    if (globals->nc_control_view->machine_parameters.smart_thc_on == false)
    {
        gcode_stack.insert(gcode_stack.begin(), "$T=" + to_string_strip_zeros(voltage_to_adc_sample(globals->nc_control_view->machine_parameters.thc_set_value)));
    }
    gcode_stack.insert(gcode_stack.begin(), "G90");
    gcode_stack.insert(gcode_stack.begin(), "G91G0Z" + to_string_strip_zeros((double)callback_args["cut_height"] - (double)callback_args["pierce_height"]));
    gcode_stack.insert(gcode_stack.begin(), "G4P" + to_string_strip_zeros((double)callback_args["pierce_delay"]));
    LOG_F(INFO, "Running callback => raise_to_cut_height_and_run_program()");
    run_pop();
}
void raise_to_pierce_height_and_fire_torch()
{
    okay_callback = &run_pop;
    probe_callback = NULL;
    //Remember that inserting at the top of list means its the next code to run, meaning
    //This list that we are inserting is ran from bottom to top or LIFO mode
    gcode_stack.insert(gcode_stack.begin(), "WAIT_FOR_ARC_OKAY");
    gcode_stack.insert(gcode_stack.begin(), "M3S1000");
    gcode_stack.insert(gcode_stack.begin(), "G91G0Z" + to_string_strip_zeros((double)callback_args["pierce_height"]));
    gcode_stack.insert(gcode_stack.begin(), "G91G0Z" + to_string_strip_zeros(globals->nc_control_view->machine_parameters.floating_head_backlash));
    LOG_F(INFO, "Running callback => raise_to_pierce_height_and_fire_torch()");
    torch_on = true;
    torch_on_timer = EasyRender::Millis();
    run_pop();
}
void touch_torch_and_pierce()
{
    okay_callback = &run_pop;
    probe_callback = NULL;
    //Remember that inserting at the top of list means its the next code to run, meaning
    //This list that we are inserting is ran from bottom to top or LIFO mode
    gcode_stack.insert(gcode_stack.begin(), "G90");
    gcode_stack.insert(gcode_stack.begin(), "G91G0Z0.5");
    gcode_stack.insert(gcode_stack.begin(), "G91G0Z" + to_string_strip_zeros(globals->nc_control_view->machine_parameters.floating_head_backlash));
    LOG_F(INFO, "Touching off torch and dry running!");
    run_pop();
}
void torch_off_and_abort()
{
    okay_callback = &run_pop;
    probe_callback = NULL;
    motion_sync_callback = NULL;
    arc_okay_callback = NULL;
    torch_on = false;
    //Remember that inserting at the top of list means its the next code to run, meaning
    //This list that we are inserting is ran from bottom to top or LIFO mode
    gcode_stack.insert(gcode_stack.begin(), "M30");
    gcode_stack.insert(gcode_stack.begin(), "G53 G0 Z0");
    gcode_stack.insert(gcode_stack.begin(), "M5");
    LOG_F(INFO, "Shutting torch off, retracting, and aborting!");
    run_pop();
}
void torch_off_and_retract()
{
    okay_callback = &run_pop;
    probe_callback = NULL;
    motion_sync_callback = NULL;
    arc_okay_callback = NULL;
    torch_on = false;
    //Remember that inserting at the top of list means its the next code to run, meaning
    //This list that we are inserting is ran from bottom to top or LIFO mode
    if (globals->nc_control_view->machine_parameters.smart_thc_on == false)
    {
        gcode_stack.insert(gcode_stack.begin(), "$T=0");
    }
    gcode_stack.insert(gcode_stack.begin(), "G53 G0 Z0");
    gcode_stack.insert(gcode_stack.begin(), "M5");
    LOG_F(INFO, "Shutting torch off and retracting!");
    run_pop();
}
void run_pop()
{
    if (gcode_stack.size() > 0)
    {
        std::string line = gcode_stack[0];
        gcode_stack.erase(gcode_stack.begin());
        if (line.find("fire_torch") != std::string::npos)
        {
            LOG_F(INFO, "[fire_torch] Sending probing cycle! - Waiting for probe to finish!");
            std::vector<std::string> args = split(line, ' ');
            if (args.size() == 4)
            {
                callback_args["pierce_height"] = (double)atof(args[1].c_str());
                callback_args["pierce_delay"] = (double)atof(args[2].c_str());
                callback_args["cut_height"] = (double)atof(args[3].c_str());
            }
            else
            {
                LOG_F(ERROR, "[fire_torch] Invalid arguments - Using defaults!");
                callback_args["pierce_height"] = DEFAULT_PIERCE_HEIGHT;
                callback_args["pierce_delay"] = DEFAULT_PIERCE_DELAY_S;
                callback_args["cut_height"] = DEFAULT_CUT_HEIGHT;
            }
            if (arc_retry_count > 3)
            {
                LOG_F(INFO, "[run_pop] Arc retry max count reached. Retracting and aborting program!");
                okay_callback = NULL;
                motion_sync_callback = &torch_off_and_abort;
                dialogs_set_info_value("Arc Strike Retry max count expired!\nLikely causes are:\n1. Bad or worn out consumables.\n2. Faulty work clamp connection or wire\n3. Inadequate pressure and/or moisture in the air system\n4. Dirty material and/or non-conductive surface\n5. Faulty Cutting unit, eg. Plasma Power Unit is worn out or broken");
            }
            else
            {
                okay_callback = NULL;
                probe_callback = &raise_to_pierce_height_and_fire_torch;
                if (globals->nc_control_view->machine_parameters.homing_dir_invert[2]) {
                    motion_controller_send_crc32("G38.3Z0F" + std::to_string(globals->nc_control_view->machine_parameters.z_probe_feedrate));
                }
                else {
                    motion_controller_send_crc32("G38.3Z-" + std::to_string(globals->nc_control_view->machine_parameters.machine_extents[2]) + "F" + std::to_string(globals->nc_control_view->machine_parameters.z_probe_feedrate));
                }
            }
        }
        else if (line.find("touch_torch") != std::string::npos)
        {
            LOG_F(INFO, "[touch_torch] Sending probing cycle! - Waiting for probe to finish!");
            std::vector<std::string> args = split(line, ' ');
            if (args.size() == 4)
            {
                callback_args["pierce_height"] = (double)atof(args[1].c_str());
                callback_args["pierce_delay"] = (double)atof(args[2].c_str());
                callback_args["cut_height"] = (double)atof(args[3].c_str());
            }
            else
            {
                LOG_F(WARNING, "[touch_torch] No arguments - Using default!");
                callback_args["pierce_height"] = DEFAULT_PIERCE_HEIGHT;
                callback_args["pierce_delay"] = DEFAULT_PIERCE_DELAY_S;
                callback_args["cut_height"] = DEFAULT_CUT_HEIGHT;
            }
            okay_callback = NULL;
            probe_callback = &touch_torch_and_pierce;
            if (globals->nc_control_view->machine_parameters.homing_dir_invert[2]) {
                motion_controller_send_crc32("G38.3ZF0" + std::to_string(globals->nc_control_view->machine_parameters.z_probe_feedrate));
            }
            else {
                motion_controller_send_crc32("G38.3Z-" + std::to_string(globals->nc_control_view->machine_parameters.machine_extents[2]) + "F" + std::to_string(globals->nc_control_view->machine_parameters.z_probe_feedrate));
            }
        }
        else if(line.find("WAIT_FOR_ARC_OKAY") != std::string::npos)
        {
            LOG_F(INFO, "[run_pop] Setting arc_okay callback and arc_okay expire timer => fires in %.4f ms!", (3 + (double)callback_args["pierce_delay"]) * 1000);
            globals->renderer->PushTimer((3 + (double)callback_args["pierce_delay"]) * 1000, &arc_okay_expire_timer);
            arc_okay_callback = &raise_to_cut_height_and_run_program;
            okay_callback = NULL;
            probe_callback = NULL;
        }
        else if (line.find("torch_off") != std::string::npos)
        {
            okay_callback = NULL;
            motion_sync_callback = &torch_off_and_retract;
        }
        else if (line.find("M30") != std::string::npos)
        {
            motion_sync_callback = &program_finished;
            okay_callback = NULL;
            probe_callback = NULL;
        }
        else
        {
            line.erase(std::remove(line.begin(), line.end(), ' '), line.end());
            
            LOG_F(INFO, "(runpop) sending %s", line.c_str());
            motion_controller_send_crc32(line);
        }
    }
    else
    {
        okay_callback = NULL;
    }
    
}
/*          end callbacks           */
nlohmann::json motion_controller_get_dro()
{
    return dro_data;
}
nlohmann::json motion_controller_get_run_time()
{
    if (program_run_time > 0)
    {
        unsigned long m = (EasyRender::Millis() - program_run_time);
        unsigned long seconds=(m/1000)%60;
        unsigned long minutes=(m/(1000*60))%60;
        unsigned long hours=(m/(1000*60*60))%24;
        return {
            {"seconds", seconds},
            {"minutes", minutes},
            {"hours", hours}
        };
    }
    else
    {
        return NULL;
    }
}
void motion_controller_log_runtime()
{
    nlohmann::json runtime;
    unsigned long m = (EasyRender::Millis() - program_run_time);
    unsigned long seconds=(m/1000)%60;
    unsigned long minutes=(m/(1000*60))%60;
    unsigned long hours=(m/(1000*60*60))%24;
    LOG_F(INFO, "Logging program run time %luh %lum %lus to total", hours, minutes, seconds);
    nlohmann::json runtime_json = globals->renderer->ParseJsonFromFile(globals->renderer->GetConfigDirectory() + "runtime.json");
    if (runtime_json != NULL)
    {
        try
        {
            runtime["total_milliseconds"] = (unsigned long)runtime_json["total_milliseconds"] + m;
        }
        catch(...)
        {
            LOG_F(WARNING, "Error parsing runtime file!");
        }
        globals->renderer->DumpJsonToFile(globals->renderer->GetConfigDirectory() + "runtime.json", runtime);
    }
    else
    {
        runtime["total_milliseconds"] = m;
        globals->renderer->DumpJsonToFile(globals->renderer->GetConfigDirectory() + "runtime.json", runtime);
    }
}
bool motion_controller_is_torch_on()
{
    return torch_on;
}
void motion_controller_cmd(std::string cmd)
{
    if (cmd == "abort")
    {
        LOG_F(INFO, "Aborting!");
        motion_controller_send("!");
        abort_pending = true;
        okay_callback = NULL;
        probe_callback = NULL;
        motion_sync_callback = NULL;
        arc_okay_callback = NULL;
        motion_controller_clear_stack();
    }
}
void motion_controller_clear_stack()
{
    gcode_stack.clear();
}
void motion_controller_push_stack(std::string gcode)
{
    gcode_stack.push_back(gcode);
}
void motion_controller_run_stack()
{
    program_run_time = EasyRender::Millis();
    run_pop();
    okay_callback = &run_pop;
}

uint32_t motion_controller_crc32c(uint32_t crc, const char *buf, size_t len)
{
    int k;
    crc = ~crc;
    while (len--) {
        crc ^= *buf++;
        for (k = 0; k < 8; k++)
            crc = crc & 1 ? (crc >> 1) ^ POLY : crc >> 1;
    }
    return ~crc;
}
void motion_controller_log_controller_error(int error)
{
    std::string ret;
    switch(error)
    {
        case 1: ret = "G-code words consist of a letter and a value. Letter was not found"; break;
        case 2: ret = "Numeric value format is not valid or missing an expected value."; break;
        case 3: ret = "System command was not recognized or supported."; break;
        case 4: ret = "Negative value received for an expected positive value."; break;
        case 5: ret = "Homing cycle is not enabled via settings."; break;
        case 6: ret = "Minimum step pulse time must be greater than 3usec"; break;
        case 7: ret = "EEPROM read failed. Reset and restored to default values."; break;
        case 8: ret = "Real-Time command cannot be used unless machine is IDLE. Ensures smooth operation during a job."; break;
        case 9: ret = "G-code locked out during alarm or jog state. Z Probe input may have been engaged before probing cycle began."; needs_homed = true; break;
        case 10: ret = "Soft limits cannot be enabled without homing also enabled."; break;
        case 11: ret = "Max characters per line exceeded. Line was not processed and executed."; break;
        case 12: ret = "Setting value exceeds the maximum step rate supported."; break;
        case 13: ret = "Safety door detected as opened and door state initiated."; break;
        case 14: ret = "Build info or startup line exceeded EEPROM line length limit."; break;
        case 15: ret = "Jog target exceeds machine travel. Command ignored."; break;
        case 16: ret = "Jog command with no '=' or contains prohibited g-code."; break;
        case 17: ret = "Laser mode requires PWM output."; break;
        case 20: ret = "Unsupported or invalid g-code command found in block."; break;
        case 21: ret = "More than one g-code command from same modal group found in block."; break;
        case 22: ret = "Feed rate has not yet been set or is undefined."; break;
        case 23: ret = "G-code command in block requires an integer value."; break;
        case 24: ret = "Two G-code commands that both require the use of the XYZ axis words were detected in the block."; break;
        case 25: ret = "A G-code word was repeated in the block."; break;
        case 26: ret = "A G-code command implicitly or explicitly requires XYZ axis words in the block, but none were detected."; break;
        case 27: ret = "N line number value is not within the valid range of 1 - 9,999,999."; break;
        case 28: ret = "A G-code command was sent, but is missing some required P or L value words in the line."; break;
        case 29: ret = "System only supports six work coordinate systems G54-G59. G59.1, G59.2, and G59.3 are not supported."; break;
        case 30: ret = "The G53 G-code command requires either a G0 seek or G1 feed motion mode to be active. A different motion was active."; break;
        case 31: ret = "There are unused axis words in the block and G80 motion mode cancel is active."; break;
        case 32: ret = "A G2 or G3 arc was commanded but there are no XYZ axis words in the selected plane to trace the arc."; break;
        case 33: ret = "The motion command has an invalid target. G2, G3, and G38.2 generates this error, if the arc is impossible to generate or if the probe target is the current position."; break;
        case 34: ret = "A G2 or G3 arc, traced with the radius definition, had a mathematical error when computing the arc geometry. Try either breaking up the arc into semi-circles or quadrants, or redefine them with the arc offset definition."; break;
        case 35: ret = "A G2 or G3 arc, traced with the offset definition, is missing the IJK offset word in the selected plane to trace the arc."; break;
        case 36: ret = "There are unused, leftover G-code words that aren't used by any command in the block."; break;
        case 37: ret = "The G43.1 dynamic tool length offset command cannot apply an offset to an axis other than its configured axis. The Grbl default axis is the Z-axis."; break;
        case 38: ret = "Tool number greater than max supported value."; break;
        case 100: ret = "Communication Redundancy Check Failed. Program Aborted to ensure unintended behavior does not occur!"; break;
        default: ret = "Firmware Error is unknown. Motion Controller may be malfunctioning or Electrical noise is interfering with serial communication!"; break;
    }
    LOG_F(ERROR, "Firmware Error %d => %s", error, ret.c_str());
    dialogs_set_info_value(ret.c_str());
    motion_controller_clear_stack();
}
void motion_controller_handle_alarm(int alarm)
{
    std::string ret;
    switch(alarm)
    {
        case 1: ret = "Hard limit triggered. Machine position is likely lost due to sudden and immediate halt. Re-homing is highly recommended."; needs_homed = true; break;
        case 2: ret = "G-code motion target exceeds machine travel. Machine position safely retained. Alarm may be unlocked."; break;
        case 3: ret = "Reset while in motion. Grbl cannot guarantee position. Lost steps are likely. Re-homing is highly recommended."; needs_homed = true; break;
        case 4: ret = "Probe fail. The probe is not in the expected initial state before starting probe cycle, where G38.2 and G38.3 is not triggered and G38.4 and G38.5 is triggered."; break;
        case 5: ret = "Probe fail. Probe did not contact the workpiece within the programmed travel for G38.2 and G38.4."; break;
        case 6: ret = "Homing fail. Reset during active homing cycle."; needs_homed = true; break;
        case 7: ret = "Homing fail. Safety door was opened during active homing cycle."; needs_homed = true; break;
        case 8: ret = "Homing fail. Cycle failed to clear limit switch when pulling off. Try increasing pull-off setting or check wiring."; needs_homed = true; break;
        case 9: ret = "Homing fail. Could not find limit switch within search distance. Defined as 1.5 * max_travel on search and 5 * pulloff on locate phases."; needs_homed = true; break;
        default: ret = "Alarm code is unknown. Motion Controller may be malfunctioning or Electrical noise is interfering with serial communication!"; break;
    }
    controller_ready = false;
    LOG_F(ERROR, "Alarm %d => %s", alarm, ret.c_str());
    dialogs_set_controller_alarm_value(ret);
    dialogs_show_controller_alarm_window(true);
    motion_controller_clear_stack();
}
void line_handler(std::string line)
{
    LOG_F(INFO, "\nHandling line %s", line.c_str());
    if (controller_ready == true)
    {
        if (line.find("{") != std::string::npos)
        {
            try
            {
                dro_data = nlohmann::json::parse(line);
                if (dro_data["ARC_OK"] == false) //Logic is inverted. Bool goes low when controller has arc_ok signal
                {
                    if (arc_okay_callback != NULL)
                    {
                        arc_okay_callback();
                        arc_okay_callback = NULL;
                        arc_okay_timer = EasyRender::Millis();
                    }
                }
                if (abort_pending == true && (bool)dro_data["IN_MOTION"] == false)
                {
                    motion_controller_log_runtime();
                    motion_controller.delay(300);
                    LOG_F(INFO, "Handling pending abort -> Sending Reset!");
                    motion_controller.send_byte(0x18);
                    motion_controller.delay(300);
                    abort_pending = false;
                    program_run_time = 0;
                    arc_retry_count = 0;
                    torch_on = false;
                    handling_crash = false;
                    controller_ready = false;
                }
            }
            catch(...)
            {
                LOG_F(ERROR, "Error parsing DRO JSON data!");
            }
        }
        else if (line.find("[MSG:'$H'|'$X' to unlock]") != std::string::npos)
        {
            if (globals->nc_control_view->machine_parameters.homing_enabled == false)
            {
                LOG_F(WARNING, "Controller lockout for unknown reason. Automatically unlocking...");
                motion_controller_send("$X");
            }
            else {
                LOG_F(WARNING, "Controller is locked. Homing automatically...");
                needs_homed = true;
            }
        }
        else if (line.find("[CHECKSUM_FAILURE]") != std::string::npos)
        {
            if (last_checksum_error > 0 && (EasyRender::Millis() - last_checksum_error) < 100)
            {
                dialogs_set_info_value("Program aborted due to multiple communication checksum errors in a short period of time!");
                motion_controller_cmd("abort");
            }
            else
            {
                LOG_F(WARNING, "(motion_control) Communication checksum error, resending last send communication!");
                last_checksum_error = EasyRender::Millis();
                motion_controller.resend();
            }
        }
        else if (line.find("error") != std::string::npos)
        {
            removeSubstrs(line, "error:");
            motion_controller_log_controller_error(atoi(line.c_str()));
        }
        else if (line.find("ALARM") != std::string::npos)
        {
            removeSubstrs(line, "ALARM:");
            motion_controller_handle_alarm(atoi(line.c_str()));
        }
        else if (line.find("[CRASH]") != std::string::npos && handling_crash == false)
        {
            if (torch_on == true && (EasyRender::Millis() - torch_on_timer) > 2000)
            {
                dialogs_set_info_value("Program was aborted because torch crash was detected!");
                motion_controller_cmd("abort");
                handling_crash = true;
            }
        }
        else if (line.find("[PRB") != std::string::npos)
        {
            LOG_F(INFO, "Probe finished - Running callback!");
            if (probe_callback != NULL) probe_callback();
        }
        else if (line.find("Grbl") != std::string::npos)
        {
            LOG_F(WARNING, "Received Grbl start message while in ready state. Was the controller reset?");
        }
        else
        {
            LOG_F(WARNING, "Unidentified line recived - %s", line.c_str());
        }
    }
    else if (controller_ready == false)
    {
        if (line.find("Grbl") != std::string::npos)
        {
            LOG_F(INFO, "Controller ready!");
            controller_ready = true;
            motion_controller_push_stack("G10 L2 P0 X" + std::to_string(globals->nc_control_view->machine_parameters.work_offset[0]) + " Y" + std::to_string(globals->nc_control_view->machine_parameters.work_offset[1]) + " Z" + std::to_string(globals->nc_control_view->machine_parameters.work_offset[2]));
            motion_controller_push_stack("M30");
            motion_controller_run_stack();
        }
        else if (line.find("[MSG:'$H'|'$X' to unlock]") != std::string::npos)
        {
            if (globals->nc_control_view->machine_parameters.homing_enabled == true)
            {
                LOG_F(INFO, "Controller ready, but needs homing.");
                controller_ready = true;
                needs_homed = true;
                motion_controller_push_stack("G10 L2 P0 X" + std::to_string(globals->nc_control_view->machine_parameters.work_offset[0]) + " Y" + std::to_string(globals->nc_control_view->machine_parameters.work_offset[1]) + " Z" + std::to_string(globals->nc_control_view->machine_parameters.work_offset[2]));
                motion_controller_push_stack("M30");
                motion_controller_run_stack();
            }
            else
            {
                LOG_F(WARNING, "Controller locked out. Unlocking...");
                motion_controller_send("$X");
            }
        }
    }
}
void motion_controller_save_machine_parameters()
{
    //Write to file
    nlohmann::json preferences;
    preferences["work_offset"]["x"] = globals->nc_control_view->machine_parameters.work_offset[0];
    preferences["work_offset"]["y"] = globals->nc_control_view->machine_parameters.work_offset[1];
    preferences["work_offset"]["z"] = globals->nc_control_view->machine_parameters.work_offset[2];
    preferences["machine_extents"]["x"] = globals->nc_control_view->machine_parameters.machine_extents[0];
    preferences["machine_extents"]["y"] = globals->nc_control_view->machine_parameters.machine_extents[1];
    preferences["machine_extents"]["z"] = globals->nc_control_view->machine_parameters.machine_extents[2];
    preferences["cutting_extents"]["x1"] = globals->nc_control_view->machine_parameters.cutting_extents[0];
    preferences["cutting_extents"]["y1"] = globals->nc_control_view->machine_parameters.cutting_extents[1];
    preferences["cutting_extents"]["x2"] = globals->nc_control_view->machine_parameters.cutting_extents[2];
    preferences["cutting_extents"]["y2"] = globals->nc_control_view->machine_parameters.cutting_extents[3];
    preferences["axis_scale"]["x"] = globals->nc_control_view->machine_parameters.axis_scale[0];
    preferences["axis_scale"]["y"] = globals->nc_control_view->machine_parameters.axis_scale[1];
    preferences["axis_scale"]["z"] = globals->nc_control_view->machine_parameters.axis_scale[2];
    preferences["max_vel"]["x"] = globals->nc_control_view->machine_parameters.max_vel[0];
    preferences["max_vel"]["y"] = globals->nc_control_view->machine_parameters.max_vel[1];
    preferences["max_vel"]["z"] = globals->nc_control_view->machine_parameters.max_vel[2];
    preferences["max_accel"]["x"] = globals->nc_control_view->machine_parameters.max_accel[0];
    preferences["max_accel"]["y"] = globals->nc_control_view->machine_parameters.max_accel[1];
    preferences["max_accel"]["z"] = globals->nc_control_view->machine_parameters.max_accel[2];
    preferences["junction_deviation"] = globals->nc_control_view->machine_parameters.junction_deviation;
    preferences["arc_stabilization_time"] = globals->nc_control_view->machine_parameters.arc_stabilization_time;
    preferences["arc_voltage_divider"] = globals->nc_control_view->machine_parameters.arc_voltage_divider;
    preferences["floating_head_backlash"] = globals->nc_control_view->machine_parameters.floating_head_backlash;
    preferences["z_probe_feedrate"] = globals->nc_control_view->machine_parameters.z_probe_feedrate;
    preferences["axis_invert"]["x"] = globals->nc_control_view->machine_parameters.axis_invert[0];
    preferences["axis_invert"]["y1"] = globals->nc_control_view->machine_parameters.axis_invert[1];
    preferences["axis_invert"]["y2"] = globals->nc_control_view->machine_parameters.axis_invert[2];
    preferences["axis_invert"]["z"] = globals->nc_control_view->machine_parameters.axis_invert[3];
    preferences["soft_limits_enabled"] = globals->nc_control_view->machine_parameters.soft_limits_enabled;
    preferences["homing_enabled"] = globals->nc_control_view->machine_parameters.homing_enabled;
    preferences["homing_dir_invert"]["x"] = globals->nc_control_view->machine_parameters.homing_dir_invert[0];
    preferences["homing_dir_invert"]["y1"] = globals->nc_control_view->machine_parameters.homing_dir_invert[1];
    preferences["homing_dir_invert"]["y2"] = globals->nc_control_view->machine_parameters.homing_dir_invert[2];
    preferences["homing_dir_invert"]["z"] = globals->nc_control_view->machine_parameters.homing_dir_invert[3];
    preferences["homing_feed"] = globals->nc_control_view->machine_parameters.homing_feed;
    preferences["homing_seek"] = globals->nc_control_view->machine_parameters.homing_seek;
    preferences["homing_debounce"] = globals->nc_control_view->machine_parameters.homing_debounce;
    preferences["homing_pull_off"] = globals->nc_control_view->machine_parameters.homing_pull_off;
    preferences["invert_limit_pins"] = globals->nc_control_view->machine_parameters.invert_limit_pins;
    preferences["invert_probe_pin"] = globals->nc_control_view->machine_parameters.invert_probe_pin;
    preferences["invert_step_enable"] = globals->nc_control_view->machine_parameters.invert_step_enable;
    preferences["precise_jog_units"] = globals->nc_control_view->machine_parameters.precise_jog_units;

    globals->nc_control_view->machine_plane->bottom_left.x = 0;
    globals->nc_control_view->machine_plane->bottom_left.y = 0;
    globals->nc_control_view->machine_plane->width = globals->nc_control_view->machine_parameters.machine_extents[0];
    globals->nc_control_view->machine_plane->height = globals->nc_control_view->machine_parameters.machine_extents[1];
    globals->nc_control_view->cuttable_plane->bottom_left.x = globals->nc_control_view->machine_parameters.cutting_extents[0];
    globals->nc_control_view->cuttable_plane->bottom_left.y = globals->nc_control_view->machine_parameters.cutting_extents[1];
    globals->nc_control_view->cuttable_plane->width = (globals->nc_control_view->machine_parameters.machine_extents[0] + globals->nc_control_view->machine_parameters.cutting_extents[2]) - globals->nc_control_view->machine_parameters.cutting_extents[0];
    globals->nc_control_view->cuttable_plane->height = (globals->nc_control_view->machine_parameters.machine_extents[1] + globals->nc_control_view->machine_parameters.cutting_extents[3]) - globals->nc_control_view->machine_parameters.cutting_extents[1];
    try
    {
        std::ofstream out(globals->renderer->GetConfigDirectory() + "machine_parameters.json");
        out << preferences.dump();
        out.close();
    }
    catch(...)
    {
        LOG_F(ERROR, "Could not write parameters file!");
    }
}
void motion_controller_write_parameters_to_controller()
{
    if (motion_controller.is_connected) //Write controller specific parameters to flash
    {
        uint8_t dir_invert_mask = 0b00000000;
        if (globals->nc_control_view->machine_parameters.axis_invert[0]) dir_invert_mask |= 0b00000001;
        if (globals->nc_control_view->machine_parameters.axis_invert[1]) dir_invert_mask |= 0b00000010;
        if (globals->nc_control_view->machine_parameters.axis_invert[3]) dir_invert_mask |= 0b00000100;
        if (globals->nc_control_view->machine_parameters.axis_invert[2]) dir_invert_mask |= 0b00001000;

        uint8_t homing_dir_invert_mask = 0b00000000;
        if (globals->nc_control_view->machine_parameters.homing_dir_invert[0]) homing_dir_invert_mask |= 0b00000001;
        if (globals->nc_control_view->machine_parameters.homing_dir_invert[1]) homing_dir_invert_mask |= 0b00000010;
        if (globals->nc_control_view->machine_parameters.homing_dir_invert[3]) homing_dir_invert_mask |= 0b00000100;
        if (globals->nc_control_view->machine_parameters.homing_dir_invert[2]) homing_dir_invert_mask |= 0b00001000;
        
        motion_controller_push_stack("$X");
        motion_controller_push_stack("$0=" + std::to_string(10)); //Step Pulse, usec
        motion_controller_push_stack("$1=" + std::to_string(255)); //Step Idle Delay, usec (always on)
        motion_controller_push_stack("$3=" + std::to_string(dir_invert_mask));

        motion_controller_push_stack("$4=" + std::to_string((int)globals->nc_control_view->machine_parameters.invert_step_enable)); // Step enable invert
        motion_controller_push_stack("$5=" + std::to_string((int)globals->nc_control_view->machine_parameters.invert_limit_pins)); // Limit Pins invert
        motion_controller_push_stack("$6=" + std::to_string((int)globals->nc_control_view->machine_parameters.invert_probe_pin));
        motion_controller_push_stack("$10=" + std::to_string(3)); //Status report mask

        motion_controller_push_stack("$11=" + std::to_string(globals->nc_control_view->machine_parameters.junction_deviation));
        motion_controller_push_stack("$12=" + std::to_string(0.002)); //Arc Tolerance
        
        motion_controller_push_stack("$13=" + std::to_string(0)); //Report Inches (TODO: not yet supported)
        
        motion_controller_push_stack("$22=" + std::to_string((int)globals->nc_control_view->machine_parameters.homing_enabled)); //Homing Cycle Enable (Homing must be enbled before soft-limits...)
        motion_controller_push_stack("$20=" + std::to_string((int)globals->nc_control_view->machine_parameters.soft_limits_enabled)); //Soft Limits Enable
        motion_controller_push_stack("$21=" + std::to_string(0)); //Hard Limits Enable (TODO: not yet supported)
        motion_controller_push_stack("$23=" + std::to_string(homing_dir_invert_mask));
        motion_controller_push_stack("$24=" + std::to_string(globals->nc_control_view->machine_parameters.homing_feed));
        motion_controller_push_stack("$25=" + std::to_string(globals->nc_control_view->machine_parameters.homing_seek));
        motion_controller_push_stack("$26=" + std::to_string(globals->nc_control_view->machine_parameters.homing_debounce));
        motion_controller_push_stack("$27=" + std::to_string(globals->nc_control_view->machine_parameters.homing_pull_off));
        motion_controller_push_stack("$100=" + std::to_string(globals->nc_control_view->machine_parameters.axis_scale[0]));
        motion_controller_push_stack("$101=" + std::to_string(globals->nc_control_view->machine_parameters.axis_scale[1]));
        motion_controller_push_stack("$102=" + std::to_string(globals->nc_control_view->machine_parameters.axis_scale[2]));
        motion_controller_push_stack("$110=" + std::to_string(globals->nc_control_view->machine_parameters.max_vel[0]));
        motion_controller_push_stack("$111=" + std::to_string(globals->nc_control_view->machine_parameters.max_vel[1]));
        motion_controller_push_stack("$112=" + std::to_string(globals->nc_control_view->machine_parameters.max_vel[2]));
        motion_controller_push_stack("$120=" + std::to_string(globals->nc_control_view->machine_parameters.max_accel[0]));
        motion_controller_push_stack("$121=" + std::to_string(globals->nc_control_view->machine_parameters.max_accel[1]));
        motion_controller_push_stack("$122=" + std::to_string(globals->nc_control_view->machine_parameters.max_accel[2]));
        motion_controller_push_stack("$130=" + std::to_string(fabs(globals->nc_control_view->machine_parameters.machine_extents[0]))); //x max travel
        motion_controller_push_stack("$131=" + std::to_string(fabs(globals->nc_control_view->machine_parameters.machine_extents[1]))); //y max travel
        motion_controller_push_stack("$132=" + std::to_string(fabs(globals->nc_control_view->machine_parameters.machine_extents[2]))); //z max travel
        motion_controller_push_stack("M30");
        motion_controller_run_stack();
    }
}
void motion_controller_trigger_reset()
{
    LOG_F(INFO, "Resetting Motion Controller!");
    motion_controller.serial.setDTR(true);
    motion_controller.delay(100);
    motion_controller.serial.setDTR(false);
    controller_ready = false;
}
bool byte_handler(uint8_t b)
{
    if (controller_ready == true)
    {
        if (b == '>')
        {
            LOG_F(INFO, "Recieved ok byte!");
            if (okay_callback != NULL) okay_callback();
            return true;
        }
    }
    return false;
}
void motion_controller_send_crc32(std::string s)
{
    if (controller_ready == true)
    {
        s.erase(remove(s.begin(), s.end(), ' '), s.end());
        s.erase(remove(s.begin(), s.end(), '\n'), s.end());
        s.erase(remove(s.begin(), s.end(), '\r'), s.end());
        uint32_t checksum = motion_controller_crc32c(0, s.c_str(), s.size());
        motion_controller.send_string(s + "*" + std::to_string(checksum) + "\n");
    }
}
void motion_controller_send(std::string s)
{
    if (controller_ready == true)
    {
        s.erase(remove(s.begin(), s.end(), ' '), s.end());
        s.erase(remove(s.begin(), s.end(), '\n'), s.end());
        s.erase(remove(s.begin(), s.end(), '\r'), s.end());
        //LOG_F(INFO, "Sending: %s", s.c_str());
        motion_controller.send_string(s + "\n");
    }
}
void motion_controller_send_rt(char s)
{
    if (controller_ready == true)
    {
        LOG_F(INFO, "(motion_controller_send_rt) Byte: %c\n", s);
        motion_controller.send_byte(s);
    }
}
bool motion_control_status_timer()
{
    //also use this to check if auto thc should be turned on...
    if (controller_ready == true)
    {
        if (dro_data.contains("STATUS"))
        {
            if (motion_sync_callback != NULL && (bool)dro_data["IN_MOTION"] == false && (EasyRender::Millis() - program_run_time) > 500)
            {
                LOG_F(INFO, "Motion is synced, calling pending callback!");
                motion_sync_callback();
                motion_sync_callback = NULL;
            }
        }
    }
    motion_controller_send("?");
    return true;
}

void motion_controller_set_needs_homed(bool h)
{
    needs_homed = h;
}

void motion_control_init()
{
    controller_ready = false;
    torch_on = false;
    torch_on_timer = 0;
    arc_okay_timer = 0;
    abort_pending = false;
    handling_crash = false;
    program_run_time = 0;
    arc_retry_count = 0;
    if (globals->nc_control_view->machine_parameters.homing_enabled == true)
    {
        needs_homed = true;
    }
    else
    {
        needs_homed = false;
    }
    globals->renderer->PushTimer(100, motion_control_status_timer);
}
void motion_control_tick()
{
    motion_controller.tick();
    dialogs_show_controller_offline_window(!motion_controller.is_connected);
    if (needs_homed == true && globals->nc_control_view->machine_parameters.homing_enabled == true && motion_controller.is_connected == true && controller_ready == true && dialogs_controller_alarm_window_visible() == false)
    {
        dialogs_show_controller_homing_window(true);
    }
    else
    {
        dialogs_show_controller_homing_window(false);
    }
    if (motion_controller.is_connected == false)
    {
        controller_ready = false;
        needs_homed = true;
    }
    try
    {
        if (torch_on == true && (EasyRender::Millis() - arc_okay_timer) > (globals->nc_control_view->machine_parameters.arc_stabilization_time + ((float)callback_args["pierce_delay"] * 1000)))
        {
            //LOG_F(INFO, "Arc is stabalized!");
            /*
                Need to implement real-time command that takes 4 bytes.
                byte 1 -> THC set command
                byte 2 -> value low bit
                byte 3 -> value high bit
                byte 4 -> End THC set command

                This way THC can be changed on the fly withouth injecting into the gcode stream which under long line moves could b
                some time after this event occurs... Also would allow setting thc on the fly manually
            */
        }
    }
    catch(...)
    {
        LOG_F(ERROR, "Error parsing callback args for Smart THC");
    }
}
