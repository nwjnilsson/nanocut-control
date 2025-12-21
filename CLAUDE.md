# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

This project uses CMake as the primary build system:

```bash
# Build the project (debug by default)
mkdir -p build && cd build
cmake ..
make

# Build release version
cmake -DCMAKE_BUILD_TYPE=Release ..
make

# Run the executable
./NanoCut

# Or from the repository root (symlink is created automatically)
./NanoCut
```

Alternative make commands are referenced in the README for different platforms:
- Linux: `make` (after installing deps: `sudo apt-get -y install build-essential libglfw3-dev mesa-common-dev libglu1-mesa-dev freeglut3-dev libzlib1-dev`)
- Windows (MSYS2): `mingw32-make.exe`
- macOS: `make` (after `brew install glfw`)

The project can also run in daemon mode: `./NanoCut --daemon`

## Code Architecture

### Main Components

**Core Application Structure:**
- `src/main.cpp` - Entry point, handles daemon mode and GUI initialization
- `src/application.h` - Legacy global state structure (`global_variables_t`) - deprecated
- `src/application/NcApp.h` - Modern application context with dependency injection and GLFW window ownership
- `src/application/View.h` - Base class for all views providing independent zoom/pan transforms
- `include/config.h` - Build-time configuration, notably `USE_INCH_DEFAULTS` for imperial units

**Three Primary Views (all inherit from View base class):**
1. **NcControlView** (`src/NcControlView/`) - CNC machine control interface
   - Inherits from View base class with independent zoom/pan transforms
   - Machine parameter management (GRBL settings, extents, homing, etc.)
   - Manual jogging with right-click drag pan functionality
   - G-code execution and monitoring
   - Torch height control (THC) for plasma cutting
   - Motion control via modernized `MotionController` class (`motion_control/motion_controller.h`)

2. **NcCamView** (`src/NcCamView/`) - CAM/toolpath workbench
   - Inherits from View base class with independent zoom/pan transforms
   - DXF file import and parsing (`DXFParsePathAdaptor/`)
   - Part layout and nesting (`PolyNest/`)
   - Toolpath generation and post-processing
   - Material setup and cutting parameters

3. **NcAdminView** (`src/NcAdminView/`) - System administration and configuration
   - Inherits from View base class with independent zoom/pan transforms
   - Network client management and debugging
   - System preferences and configuration

**Rendering and UI:**
- `src/NcRender/` - Custom OpenGL rendering engine with ImGui integration
  - Accepts GLFW window from NcApp (no longer creates its own window)
  - Modern std::function-based GUI callback system (no static function pointers)
  - Cross-platform OpenGL setup (Windows/Linux/macOS)
  - 2D geometry rendering (`geometry/`, `primitives/`)
  - GUI components and file dialogs with lambda-based callbacks
  - Built-in text editor, font management, and logging (loguru)

**Communication Systems:**
- `src/WebsocketServer/` - HTTP/WebSocket server using Mongoose
- `src/WebsocketClient/` - WebSocket client for machine communication
- `src/NcControlView/serial/` - Direct serial communication with CNC controller

**File Format Support:**
- `src/dxf/dxflib/` - DXF file parsing library
- `src/dxf/spline/` - NURBS/Bezier curve handling for advanced DXF geometry
- JSON configuration files for settings persistence

### Code Quality and Modernization

**Current State:** This codebase has been successfully modernized from C-style C++ to modern C++20 patterns:

**Modern Patterns (fully implemented):**
- `NcApp` class providing dependency injection and GLFW window ownership
- `View` base class with independent zoom/pan transforms for each view
- `MotionController` class encapsulating all motion control functionality
- Modern std::function-based callback system (no static function pointers)
- Lambda functions with proper capture semantics
- Smart pointer usage throughout (`std::unique_ptr`, `std::shared_ptr`)
- Modern member variable naming (`m_variableName`)
- RAII principles and proper constructor initialization

**Architectural Principles:**
- Dependency injection through constructor parameters
- Single responsibility principle - each class has a clear purpose
- Separation of concerns - NcApp (context), EasyRender (rendering), Views (UI logic)
- No global state or static instances (eliminated all s_current_instance patterns)
- Modern C++20 features: lambdas, std::function, smart pointers, range-based for loops

### Key Design Patterns

**Dependency Injection:** The `NcApp` class manages subsystem lifecycles and provides dependency injection. Views receive the NcApp context through constructor parameters, eliminating global state access.

**View-Based Architecture:** Each view (`NcControlView`, `NcCamView`, `NcAdminView`) inherits from the `View` base class, providing:
- Independent zoom/pan transforms (no shared state between views)
- Consistent event handling interface
- Modern lambda-based callback registration

**Modern Motion Control:** The `MotionController` class encapsulates all motion control functionality with a clean API, replacing the previous function-based approach.

**Event-Driven Architecture:** Views use std::function callbacks with lambda capture for user interactions (zoom, click-and-drag, keyboard input). No static function pointers.

**Window Management:** `NcApp` owns the GLFW window and delegates rendering to `NcRender`, ensuring proper resource management and callback ownership.

## Development Notes

**Units and Precision:** The codebase supports both metric and imperial units via the `SCALE()` macro. Default values are defined in millimeters in `config.h`.

**DXF Import Enhancement:** Recent work significantly improved DXF parsing to handle complex NURBS curves and advanced CAD geometry (see "deer.png" example in README).

**Platform Compatibility:** Code includes extensive platform-specific handling for OpenGL, file dialogs, and system integration across Windows, Linux, and macOS.

**Plasma Cutting Focus:** The software is specifically designed for CNC plasma cutting with features like torch height control, pierce/cut heights, and kerf compensation.

## Configuration Files

- Machine parameters stored in JSON format in config directory
- Uptime tracking in `uptime.json`
- User preferences per view stored separately
- G-code programs in `.nc` format (custom commands: `fire_torch`, `torch_off`)
- The json library has methods for getting certain types. Instead of static_casting, we can do `json.at("some_key").get<int>()` to get an int, for example. This style is clearer.

## Project Documentation

- `README.md` - Project overview, build instructions, and usage examples

## Architecture Migration Status

**Phase 1 ✅ Complete:** MotionController class implementation
- Converted function-based motion control to modern C++ class
- Implemented proper encapsulation and state management
- Eliminated legacy function-based approach

**Phase 2 ✅ Complete:** NcApp and View architecture
- Replaced `ApplicationContext` with `NcApp` class providing GLFW window ownership
- Implemented `View` base class with independent zoom/pan transforms
- Eliminated global variables and implemented dependency injection pattern

**Phase 3 ✅ Complete:** Modern callback system and GUI modernization
- Eliminated all static function pointers and s_current_instance patterns
- Implemented std::function-based callback system with lambda capture
- Modernized NcRender to accept GLFW window instead of creating its own
- All views now inherit from View base class with consistent interface

**Phase 4 ✅ Complete:** Right-click pan and independent view transforms
- Fixed right-click drag pan functionality in NcControlView
- Ensured complete separation of zoom/pan state between all views
- Comprehensive testing and bug fixes

**Phase 5 ✅ Complete:** Directory structure reorganization
- Renamed view directories to consistent naming convention (NcControlView, NcCamView, NcAdminView)
- Moved EasyRender to src/NcRender for better organization
- Consolidated all source code under src/ directory
- Updated all includes and imports to reflect new structure

**Final State:** The codebase has been fully modernized to C++20 standards with no legacy patterns remaining. All components follow modern C++ practices with proper RAII, dependency injection, and lambda-based callbacks.
- Try to get rid of unnecessary "this->" dereferencing
- Update @ROADMAP.md as progress is being made