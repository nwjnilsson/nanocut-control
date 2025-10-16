#include <algorithm>
#include <application.h>
#include <ncControlView/ncControlView.h>
#include <ncControlView/util.h>

float adc_sample_to_voltage(uint16_t val)
{
  // Assuming the ADC sample resolution is 1024 here
  return (static_cast<float>(val) / static_cast<float>(ADC_RESOLUTION)) *
         ADC_INPUT_VOLTAGE *
         globals->nc_control_view->machine_parameters.arc_voltage_divider;
}

int voltage_to_adc_sample(float val)
{
  // Assuming the ADC sample resolution is 1024 here
  return static_cast<int>(
    static_cast<float>(ADC_RESOLUTION) *
    (val / (ADC_INPUT_VOLTAGE *
            globals->nc_control_view->machine_parameters.arc_voltage_divider)));
}
