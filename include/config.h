#ifndef CONFIG_H
#define CONFIG_H

/* IMPORTANT:
Uncomment this define if you want to use inches. This affects the default
pierce and cut height values that the program will fall back to if the gcode
is missing these parameters.
*/
// #define USE_INCH_DEFAULTS

#if defined(USE_INCH_DEFAULTS)
constexpr auto SCALE(float x) { return x / 25.4f; }
#else
constexpr auto SCALE(float x) { return x; }
#endif

// Default values (in mm), change as you see fit
constexpr float DEFAULT_MATERIAL_SIZE = SCALE(500.0f);

constexpr float MAX_ARC_VOLTAGE = 250.0f;
constexpr float MIN_THC_OFFSET = -12.0f;
constexpr float MAX_THC_OFFSET = 12.0f;

constexpr float MAX_ARC_VOLTAGE_DIVIDER = 500.0f;
constexpr float DEFAULT_ARC_VOLTAGE_DIVIDER = 50.0f;

constexpr unsigned int DEFAULT_ARC_STABILIZATION_TIME = 2000; // milliseconds

#endif // CONFIG_H
