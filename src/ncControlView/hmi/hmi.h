#ifndef HMI_
#define HMI_

#include <EasyRender/EasyRender.h>
#include <application.h>

struct hmi_button_t {
  std::string          name;
  EasyPrimitive::Box*  object;
  EasyPrimitive::Text* label;
};
struct hmi_button_group_t {
  hmi_button_t button_one;
  hmi_button_t button_two;
};
struct dro_data_t {
  EasyPrimitive::Text* label;
  EasyPrimitive::Text* work_readout;
  EasyPrimitive::Text* absolute_readout;
  EasyPrimitive::Box*  divider;
};
struct dro_group_data_t {
  dro_data_t           x;
  dro_data_t           y;
  dro_data_t           z;
  EasyPrimitive::Text* feed;
  EasyPrimitive::Text* arc_readout;
  EasyPrimitive::Text* arc_set;
  EasyPrimitive::Text* run_time;
};

void hmi_init();
void hmi_handle_button(std::string id);
void hmi_mouse_callback(PrimitiveContainer* c, const nlohmann::json& e);
void hmi_view_matrix(PrimitiveContainer* p);

#endif
