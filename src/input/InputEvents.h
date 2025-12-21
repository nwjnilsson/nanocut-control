#pragma once

/**
 * Input event structures for type-safe event handling.
 * These replace the nlohmann::json-based event system for better
 * performance, type safety, and code readability.
 */

struct KeyEvent {
    int key;        // GLFW key code (GLFW_KEY_A, etc.)
    int scancode;   // Platform-specific scancode
    int action;     // GLFW_PRESS, GLFW_RELEASE, GLFW_REPEAT
    int mods;       // GLFW modifier flags (GLFW_MOD_CONTROL, etc.)
    
    KeyEvent() : key(0), scancode(0), action(0), mods(0) {}
    KeyEvent(int k, int sc, int a, int m) : key(k), scancode(sc), action(a), mods(m) {}
};

struct MouseButtonEvent {
    int button;     // GLFW_MOUSE_BUTTON_LEFT, GLFW_MOUSE_BUTTON_RIGHT, etc.
    int action;     // GLFW_PRESS, GLFW_RELEASE
    int mods;       // GLFW modifier flags
    
    MouseButtonEvent() : button(0), action(0), mods(0) {}
    MouseButtonEvent(int b, int a, int m) : button(b), action(a), mods(m) {}
};

struct MouseMoveEvent {
    double x;       // Current cursor X position
    double y;       // Current cursor Y position
    double prev_x;  // Previous cursor X position
    double prev_y;  // Previous cursor Y position
    double delta_x; // X movement delta
    double delta_y; // Y movement delta
    
    MouseMoveEvent() : x(0.0), y(0.0), prev_x(0.0), prev_y(0.0), delta_x(0.0), delta_y(0.0) {}
    MouseMoveEvent(double cx, double cy, double px, double py) 
        : x(cx), y(cy), prev_x(px), prev_y(py), delta_x(cx - px), delta_y(cy - py) {}
};

struct ScrollEvent {
    double x_offset;    // Horizontal scroll offset
    double y_offset;    // Vertical scroll offset
    
    ScrollEvent() : x_offset(0.0), y_offset(0.0) {}
    ScrollEvent(double x, double y) : x_offset(x), y_offset(y) {}
};

struct WindowResizeEvent {
    int width;      // New window width
    int height;     // New window height
    
    WindowResizeEvent() : width(0), height(0) {}
    WindowResizeEvent(int w, int h) : width(w), height(h) {}
};