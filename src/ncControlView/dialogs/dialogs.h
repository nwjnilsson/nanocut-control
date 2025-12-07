#ifndef DIALOGS_
#define DIALOGS_

#include <application.h>
#include <EasyRender/EasyRender.h>

void dialogs_show_machine_parameters(bool s);

void dialogs_show_preferences(bool s);

//void dialogs_show_terminal_window(bool s);

void dialogs_set_progress_value(float p);
void dialogs_show_progress_window(bool s);

void dialogs_show_info_window(bool s);
void dialogs_set_info_value(std::string i);

void dialogs_show_controller_offline_window(bool s);
void dialogs_set_controller_offline_value(std::string i);

void dialogs_show_controller_alarm_window(bool s);
void dialogs_set_controller_alarm_value(std::string i);
bool dialogs_controller_alarm_window_visible();

void dialogs_show_controller_homing_window(bool s);
void dialogs_set_controller_homing_value(std::string i);

void dialogs_show_thc_window(bool s);

void dialogs_ask_yes_no(std::string a, void (*y)(PrimitiveContainer *), void (*n)(PrimitiveContainer *), PrimitiveContainer *args);

void dialogs_init();

#endif //DIALOGS_
