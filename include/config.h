// IMPORTANT:
//
// Uncomment this define if you want to use inches. This affects the default pierce and cut height values
// that the program will fall back to if the gcode is missing these parameters.
// #define USE_INCH_DEFAULTS



#if defined(USE_INCH_DEFAULTS)
#  define SCALE(x) x / 25.4
#else
#  define SCALE(x) x
#endif

// Default values (in mm), change as you see fit
#define DEFAULT_MATERIAL_SIZE SCALE(500.f) // The work piece in CAM view (500x500)



// Other parameters that depend on external factors
#define ADC_INPUT_VOLTAGE               5.f
#define ADC_RESOLUTION                  1024
#define THC_MIN_THRESHOLD               150 // defined in nanocut-firmware thc.h
