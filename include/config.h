#ifndef CONFIG_H
#define CONFIG_H

// Default values (in mm), change as you see fit
constexpr float DEFAULT_MATERIAL_SIZE = 500.0f;

constexpr float MAX_ARC_VOLTAGE = 250.0f;
constexpr float MIN_THC_OFFSET = -12.0f;
constexpr float MAX_THC_OFFSET = 12.0f;

constexpr float MAX_ARC_VOLTAGE_DIVIDER = 500.0f;
constexpr float DEFAULT_ARC_VOLTAGE_DIVIDER = 50.0f;

constexpr unsigned int DEFAULT_ARC_STABILIZATION_TIME = 2000; // milliseconds

#endif // CONFIG_H
