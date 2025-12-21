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
constexpr float DEFAULT_MATERIAL_SIZE = SCALE(500.0f); // The work piece in CAM view
