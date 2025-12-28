# NanoCut Refactoring Roadmap

**Last Updated**: 2025-12-27

This document tracks architectural improvements and refactoring opportunities identified during comprehensive code review. The codebase has been successfully modernized from C-style to modern C++20, but significant opportunities remain for improving type safety, reducing boilerplate, and enhancing maintainability.

---

## 🎯 Executive Summary

**Key Problem Areas**:
- JSON overuse for runtime data structures (should only be used for serialization)
- String-based dispatch instead of type-safe enums
- God objects violating single responsibility principle
- Type safety gaps (unchecked array access, exceptions for control flow)
- 200+ lines of repetitive JSON serialization boilerplate

**Estimated Impact**:
- ~500 lines of code could be eliminated
- Significant runtime performance improvements in hot paths
- Compile-time safety for entire classes of bugs
- Improved maintainability and testability

---

## Phase 1: Type Safety Foundation

**Goal**: Eliminate JSON for runtime data and string-based dispatch
**Estimated Effort**: 1-2 weeks
**Priority**: Critical

### 1.1 Replace JSON Runtime Data Structures ⚠️ CRITICAL

**Files**: `src/NcControlView/motion_control/motion_controller.h`, `motion_controller.cpp`

- [x] **Replace `m_dro_data` JSON with typed struct** ✅ COMPLETED
  - **Location**: `motion_controller.h:31-45`
  - **Solution Implemented**: Created `MachineStatus` enum (lines 14-23), `MCSCoordinates` struct (lines 25-29), and `DROData` struct (lines 31-45)
  - **Impact**: Achieved compile-time checking, better performance, clearer API
  - **References**: Used throughout `motion_controller.cpp` and `hmi.cpp`

- [x] **Replace `m_callback_args` JSON with typed struct** ✅ COMPLETED
  - **Location**: `motion_controller.h:47-51`
  - **Solution Implemented**: Created `TorchParameters` struct with pierce_height, pierce_delay, and cut_height fields
  - **Impact**: Type safety achieved for critical cutting parameters

- [x] **Replace `Primitive::data` JSON field** ✅ COMPLETED
  - **Location**: `src/NcRender/primitives/Primitive.h:25-26,44`
  - **Solution Implemented**: Created `PrimitiveUserData` variant type
    - Using `std::variant<std::monostate, int, double, std::string>` for type-safe storage
    - Replaced `nlohmann::json data` with `PrimitiveUserData user_data`
    - Updated usage in `gcode.cpp:145` to store rapid_line as typed int
    - Updated usage in `hmi.cpp:413,454` to extract int with `std::get<int>()`
  - **Impact**: Type safety, reduced allocations, extensible for future types

- [x] **Replace `getRunTime()` JSON return with typed struct** ✅ COMPLETED
  - **Location**: `motion_controller.h:53-57,98`, `motion_controller.cpp:179-192`, `hmi.cpp:648-653`
  - **Solution Implemented**:
    - Created `RuntimeData` struct with `hours`, `minutes`, `seconds` fields (lines 53-57 in motion_controller.h)
    - Changed return type from `nlohmann::json` to `const RuntimeData`
    - Updated implementation to return typed struct instead of JSON object
    - Updated call site in hmi.cpp to access struct fields directly
  - **Impact**: Type safety, no JSON allocation for simple time data, clearer API

### 1.2 String-Based Dispatch → Enums ⚠️ CRITICAL

- [x] **Convert NcCamView action dispatch to polymorphic Command pattern** ✅ COMPLETED
  - **Location**: `NcCamView.h:72-130`, `NcCamView.cpp:976-1210, 1457-1466`
  - **Solution Implemented**: Full polymorphic Command pattern with virtual dispatch
    - Base `CamAction` class with virtual `execute()` method
    - 7 concrete action classes: `PostProcessAction`, `RebuildToolpathsAction`, `DeleteOperationAction`, `DeleteDuplicatePartsAction`, `DeletePartsAction`, `DeletePartAction`, `DuplicatePartAction`
    - Each action has strongly-typed member variables (no JSON!)
    - `m_action_stack` uses `std::vector<std::unique_ptr<CamAction>>`
    - Virtual dispatch in `tick()`: `action->execute(this)`
  - **Impact**:
    - ✅ Full type safety - compile-time checking for all action data
    - ✅ No JSON data passing - each action stores exactly what it needs
    - ✅ Virtual dispatch - cleaner than switch statements
    - ✅ Easy to extend - just add a new class
    - ✅ Modern C++ - proper use of polymorphism and smart pointers

- [x] **Convert HMI button IDs to enums** ✅ COMPLETED
  - **Location**: `src/NcControlView/hmi/hmi.h:19-36`, `hmi.cpp:13-48, 122-393`
  - **Solution Implemented**: Created `HmiButtonId` enum class with helper function `parseButtonId()` for string-to-enum conversion. Button handling now uses clean switch statements.
  - **Impact**: Much cleaner, faster, typo-proof code achieved

- [x] **Refactor G-code protocol parsing** ✅ COMPLETED
  - **Location**: `motion_controller.h:53-63`, `motion_controller.cpp:44-73, 284-468`
  - **Solution Implemented**: Created `GrblMessageType` enum with 9 message types and `parseMessageType()` helper function. Refactored `lineHandler()` to use clean switch statements for type-safe dispatch.
  - **Impact**: Better performance (called for every serial line), more maintainable, compile-time type safety achieved

### 1.3 Event System JSON Elimination ⚠️ CRITICAL

- [ ] **Replace JSON event callbacks with typed structs** (HIGH PRIORITY)
  - **Location**: `src/NcRender/primitives/Primitive.h:39-43`, `src/NcControlView/hmi/hmi.h:97,127-138`, `hmi.cpp:1408-1461`
  - **Problem**: Event system uses JSON for runtime event data
    - `Primitive::mouse_callback` signature: `std::function<void(Primitive*, const nlohmann::json&)>`
    - `Primitive::m_mouse_event` is `nlohmann::json`
    - All HMI key callbacks take `const nlohmann::json&`: `escapeKeyCallback()`, `upKeyCallback()`, etc.
    - Events converted from typed structs → JSON → handlers extract fields back out
    - Example (hmi.cpp:1408-1414):
      ```cpp
      void NcHmi::handleKeyEvent(const KeyEvent& e, const InputState& input) {
        // Convert to JSON format for existing handlers
        nlohmann::json json_event = { { "key", e.key }, { "scancode", e.scancode }, ... };
        // Then handlers like escapeKeyCallback(json_event) extract the fields
      }
      ```
  - **Solution**:
    1. Define typed event structs (already exist: `KeyEvent`, `MouseEvent`, `CursorPosEvent`, `WindowSizeEvent`)
    2. Change callback signatures:
       ```cpp
       // Primitive.h
       std::function<void(Primitive*, const MouseEvent&)> mouse_callback;
       MouseEvent m_mouse_event;  // instead of nlohmann::json

       // hmi.h
       void mouseCallback(Primitive* c, const MouseEvent& e);
       void escapeKeyCallback(const KeyEvent& e);
       // ... etc for all key callbacks
       void mouseMotionCallback(const CursorPosEvent& e);
       void resizeCallback(const WindowSizeEvent& e);
       ```
    3. Remove JSON conversion in `handleKeyEvent()`, `handleCursorPosEvent()`, `handleWindowSizeEvent()`
    4. Update all call sites (button callbacks, HMI handlers, etc.)
  - **Impact**:
    - **Type safety**: Compile-time checking for event data
    - **Performance**: Eliminate JSON creation/parsing for every event
    - **Clarity**: Clear event types instead of opaque JSON
    - **No runtime overhead**: Direct struct passing instead of JSON serialization
  - **References**: Event system used throughout UI interaction (hot path!)

### 1.4 Array Access Safety

- [ ] **Add bounds checking for array access** (HIGH PRIORITY)
  - **Location**: Throughout `motion_controller.cpp`, 50+ instances
  - **Problem**: Unchecked array indexing on `MachineParameters` arrays
  - **Current**: `m_control_view->m_machine_parameters.axis_invert[0]`
  - **Solution**: Use `std::array<T, N>` and `.at()` for debug bounds checking
  - **Impact**: Catch potential buffer overruns

---

## Phase 2: Eliminate Boilerplate

**Goal**: Remove repetitive JSON serialization code
**Estimated Effort**: 1 week
**Priority**: High

### 2.1 JSON Serialization Automation

- [x] **Use custom to_json/from_json for MachineParameters** ✅ COMPLETED (HIGH PRIORITY)
  - **Location**: `src/NcControlView/NcControlView.h:76-165`, `NcControlView.cpp:67-87`, `motion_controller.cpp:998-1002`
  - **Problem**: 140 lines of manual JSON deserialization + 116 lines of serialization
  - **Solution Implemented**: Created custom `to_json` and `from_json` functions (nlohmann/json 3.7.3 lacks NLOHMANN_DEFINE_TYPE macros)
  - **Impact**: Eliminated ~160 lines of boilerplate code
    - Deserialization: 80 lines → 1 line (`from_json(parameters, m_machine_parameters)`)
    - Serialization: 80 lines → 1 line (`NcControlView::to_json(preferences, m_machine_parameters)`)
  - **Files Modified**:
    - `NcControlView.h`: Added static `to_json()` and `from_json()` functions (50 lines of reusable serialization logic)
    - `NcControlView.cpp`: Replaced manual deserialization with single function call
    - `motion_controller.cpp`: Replaced manual serialization with single function call

- [x] **Convert NcCamView structs to typed fields** ✅ COMPLETED (HIGH PRIORITY)
  - **Location**: `src/NcCamView/NcCamView.h:51-63`, `NcCamView.cpp:15-38, 525-1448`
  - **Problem**: `ToolData::params` used `std::unordered_map<std::string, float>` with magic string keys, `char tool_name[1024]` C-style array
  - **Solution Implemented**:
    - Replaced `std::unordered_map<std::string, float> params` with 6 typed fields: `pierce_height`, `pierce_delay`, `cut_height`, `kerf_width`, `feed_rate`, `thc`
    - Replaced `char tool_name[1024]` with `std::string tool_name`
    - Created custom `to_json()` and `from_json()` functions for automatic serialization
  - **Impact**:
    - ✅ Full compile-time type safety - no more magic strings
    - ✅ IDE autocomplete for all tool parameters
    - ✅ Cleaner, more readable code throughout NcCamView
    - ✅ Simplified serialization/deserialization (3 lines vs. nested loops)
    - ✅ Better performance - direct field access instead of hash map lookups
  - **Files Modified**:
    - `NcCamView.h`: Converted ToolData to typed struct with 6 float fields + std::string, added serialization functions
    - `NcCamView.cpp`: Updated ~25 usage sites across UI rendering, tool editing, toolpath generation, and JSON I/O

## Phase 3: Architectural Improvements

### 3.4 Refactor Utility Classes to Namespaces

- [x] **Convert Geometry class to namespace and eliminate JSON** ✅ COMPLETED
  - **Location**: `src/NcRender/geometry/geometry.h`, `geometry.cpp`
  - **Problem 1**: Stateless class used as function container - classic "class as namespace" anti-pattern
    - Class with only public methods (no member variables except helper functions)
    - All methods are stateless geometric calculations
    - Requires unnecessary instantiation: `Geometry geom; geom.distance(p1, p2);`
  - **Problem 2**: Extensive JSON misuse for geometric data structures
    - `arcToLineSegments()`, `circleToLineSegments()` return `nlohmann::json` instead of typed line segments
    - `normalize()`, `chainify()`, `offset()`, `slot()` use JSON for paths/contours
    - `getExtents()` returns JSON instead of a proper Extents struct
    - JSON accessed every frame for geometric operations - major performance issue
  - **Solution**:
    1. Convert class to namespace
    2. Define proper geometric types:
    ```cpp
    namespace geo {
        // Proper geometric types
        struct Line { Point2d start; Point2d end; };
        struct Arc { Point2d center; double radius; double start_angle; double end_angle; };
        struct Circle { Point2d center; double radius; };
        struct Extents { Point2d min; Point2d max; };

        using Path = std::vector<Point2d>;
        using Contour = std::vector<Point2d>;

        // Utility functions with proper types
        bool between(double x, double min, double max);
        bool pointsMatch(Point2d p1, Point2d p2);
        double distance(Point2d p1, Point2d p2);
        Point2d midpoint(Point2d p1, Point2d p2);

        // Geometric conversions with proper types
        std::vector<Line> arcToLineSegments(const Arc& arc, int num_segments = 100);
        std::vector<Line> circleToLineSegments(const Circle& circle);
        std::vector<Line> normalize(const std::vector<GeometryPrimitive>& stack);
        Extents getExtents(const std::vector<Line>& lines);
        std::vector<Contour> chainify(const std::vector<Line>& lines, double tolerance);
        std::vector<Path> offset(const Path& path, double offset);
        // ... etc
    }
    // Usage: geo::distance(p1, p2);
    ```
  - **Impact**:
    - **Type safety**: Compile-time checking for geometric operations
    - **Performance**: Eliminate JSON parsing/serialization in hot paths
    - **Clarity**: Clear API - `std::vector<Line>` is immediately understandable, JSON is opaque
    - **API design**: Functions that don't need state shouldn't be in a class
    - **Maintainability**: Proper types make code self-documenting
  - **Solution Implemented**:
    - ✅ Converted Geometry class to `geo` namespace
    - ✅ Created proper geometric types: `Line`, `Arc`, `Circle`, `Extents`, `Path`, `Contour`
    - ✅ Created `GeometryPrimitive` variant type with union for mixed geometry
    - ✅ All functions now use proper typed parameters and return values
    - ✅ Updated all call sites in Arc, Circle, Line, Path, Part primitives
    - ✅ Updated all call sites in DXFParsePathAdaptor and gcode.cpp
    - ✅ Removed unused JSON compatibility layer
    - ✅ Kept `double_line_t` typedef for backwards compatibility (can be removed later)
  - **Impact Achieved**:
    - ✅ Full type safety with compile-time checking
    - ✅ Eliminated JSON parsing/serialization from hot paths
    - ✅ Clear, self-documenting API
    - ✅ Better performance in geometric operations
  - **Files Modified**:
    - `geometry.h`: Namespace definition with typed structs (Line, Arc, Circle, Extents)
    - `geometry.cpp`: Implementation with proper types (~650 lines, removed ~250 lines of JSON compat code)
    - All primitive classes: Updated to use `geo::` namespace functions
    - `DXFParsePathAdaptor.cpp`: Updated chainify and geometric calculations
    - `gcode.cpp`: Updated path simplification and arrow rendering

---

## Phase 4: Type Safety & Modern C++

**Goal**: Leverage modern C++ features for safety and clarity
**Estimated Effort**: Ongoing
**Priority**: Medium

### 4.1 Error Handling Improvements

- [ ] **Add [[nodiscard]] to functions that can fail** (HIGH PRIORITY)
  - **Location**: `motion_controller.cpp` - `send()` functions
  - **Problem**: Return void, callers can't detect failures
  - **Solution**:
    ```cpp
    [[nodiscard]] bool send(const std::string& s);
    // Or with C++23: std::expected<void, SendError>
    ```
  - **Impact**: Better error handling, prevents ignoring failures

- [ ] **Unified error handling strategy** (MEDIUM PRIORITY)
  - **Location**: Throughout codebase
  - **Problem**: Mix of exceptions, bool returns, and logging
  - **Solution**: Consistent strategy using `std::expected<T, Error>` or custom Result type
  - **Impact**: Consistent, predictable error handling

### 4.2 Type Safety Enhancements

- [ ] **Replace sentinel values with std::optional** (MEDIUM PRIORITY)
  - **Location**: `motion_controller.cpp:573`
  - **Current**: `if (m_program_start_time != std::chrono::steady_clock::time_point{})`
  - **Solution**: `std::optional<std::chrono::steady_clock::time_point>`
  - **Impact**: Clearer intent

- [ ] **Replace fixed-size char arrays with std::string** (MEDIUM PRIORITY)
  - **Location**: `src/NcCamView/NcCamView.h:51-58`
  - **Current**: `char tool_name[1024]` with `sprintf`
  - **Solution**: Use `std::string`
  - **Impact**: No buffer overflow risk

### 4.3 Modern C++ Features

- [ ] **Convert error code mapping to constexpr array** (MEDIUM PRIORITY)
  - **Location**: `motion_controller.cpp:715-926`
  - **Problem**: 200+ line switch statement mapping error codes to strings
  - **Solution**:
    ```cpp
    struct ErrorInfo {
        int code;
        std::string_view message;
    };
    constexpr std::array<ErrorInfo, 50> ERROR_TABLE = {{
        {1, "G-code words consist of..."},
        {2, "Numeric value format..."},
        // ...
    }};
    ```
  - **Impact**: Cleaner, more maintainable

- [ ] **Add concepts for type requirements** (LOW PRIORITY)
  - **Location**: Template code throughout
  - **Solution**: Add concepts for serializable types, numeric ranges
  - **Impact**: Better compile-time error messages

- [ ] **Consider C++20 ranges where appropriate** (LOW PRIORITY)
  - **Location**: Manual loops throughout
  - **Current**: `forEachPart`, `forEachVisiblePart` templates (already good!)
  - **Solution**: Could use ranges views for filtering/transforming
  - **Impact**: More expressive, but current approach is already solid

---

## Phase 5: Polish & Cleanup

**Goal**: Code consistency and maintainability
**Estimated Effort**: Ongoing
**Priority**: Low-Medium

### 5.1 Magic String Elimination

- [ ] **Replace custom G-code command strings with enums** (MEDIUM PRIORITY)
  - **Location**: `motion_controller.cpp:393-475`
  - **Current**: `"fire_torch"`, `"touch_torch"`, `"WAIT_FOR_ARC_OKAY"`, `"torch_off"`, `"M30"`
  - **Solution**: Parse into enum-based command structure
  - **Impact**: Type safety for custom commands

### 5.2 Style Consistency

- [ ] **Eliminate unnecessary this-> usage** (LOW PRIORITY)
  - **Location**: Throughout codebase
  - **Problem**: Mix of `this->m_member` and `m_member`
  - **Solution**: Consistently use `m_member` without `this->`
  - **Impact**: Code consistency (noted in CLAUDE.md)

- [ ] **Remove commented-out code** (LOW PRIORITY)
  - **Location**: Various, e.g., `hmi.cpp:132-151`
  - **Problem**: Large blocks of commented code (20+ lines)
  - **Solution**: Remove or use version control
  - **Impact**: Cleaner code

### 5.3 Performance Optimizations

- [ ] **Consider object pooling for primitives** (LOW PRIORITY)
  - **Location**: Rendering system
  - **Problem**: Frequent heap allocations during render
  - **Solution**: Object pooling for frequently created/destroyed primitives
  - **Impact**: Reduced allocation overhead (likely not a bottleneck)

---

## Quick Wins (Highest ROI)

If tackling only a few improvements, prioritize these:

1. **Replace `m_dro_data` and `m_callback_args` JSON with structs** ✅ COMPLETED
   - Immediate type safety
   - Better performance
   - Clearer API
   - ~200 lines affected

2. **Type-safe action/button dispatch** ✅ FULLY COMPLETE
   - ✅ HMI button dispatch converted to enums
   - ✅ G-code protocol parsing converted to enums
   - ✅ NcCamView action dispatch converted to polymorphic Command pattern
   - Eliminated entire class of runtime bugs
   - Better performance
   - Full compile-time checking
   - ~300+ lines affected, JSON data passing eliminated

3. **Convert Geometry class to namespace and eliminate JSON** ✅ COMPLETED
   - ✅ Full type safety with compile-time checking
   - ✅ Eliminated JSON from hot paths (rendering, toolpath generation)
   - ✅ ~650 lines of proper types, removed ~250 lines of JSON compat code
   - Better performance in geometric operations

4. **Replace JSON event callbacks with typed structs** ⚠️ NEXT PRIORITY
   - Event system is a hot path (every user interaction)
   - Eliminate JSON creation/parsing for every mouse/keyboard event
   - Type safety for all UI events
   - ~50+ callback sites affected
   - Major performance improvement

5. **JSON Serialization Automation** ✅ COMPLETED
   - Delete ~200 lines of boilerplate
   - Less error-prone
   - Easier to maintain

---

## Progress Tracking

### Completed Items
- Phase 1: MotionController class implementation ✅
- Phase 2: NcApp and View architecture ✅
- Phase 3: Modern callback system and GUI modernization ✅
- Phase 4: Right-click pan and independent view transforms ✅
- Phase 5: Directory structure reorganization ✅
- **Phase 1.1: Replace JSON Runtime Data Structures** ✅ FULLY COMPLETE
  - Replaced `m_dro_data` JSON with typed `DROData` struct
  - Replaced `m_callback_args` JSON with typed `TorchParameters` struct
  - Replaced `Primitive::data` JSON with `PrimitiveUserData` std::variant
  - Replaced `getRunTime()` JSON return with typed `RuntimeData` struct
  - All runtime data now uses proper type-safe structs
- Phase 1.2: String-based dispatch → Type-safe dispatch (enums + polymorphism) ✅
  - HMI button IDs converted to enums
  - G-code protocol parsing converted to enums
  - NcCamView actions converted to polymorphic Command pattern
- Phase 2.1: JSON Serialization Automation for MachineParameters ✅
  - Created custom to_json/from_json functions
  - Eliminated ~160 lines of manual JSON boilerplate
  - Single-line serialization/deserialization
- Phase 2.1: Convert NcCamView ToolData to typed fields ✅
  - Replaced `std::unordered_map<std::string, float> params` with 6 typed fields
  - Replaced `char tool_name[1024]` with `std::string tool_name`
  - Created custom to_json/from_json functions for serialization
  - Updated ~25 usage sites throughout NcCamView
  - Full compile-time type safety, better performance, cleaner code
- Phase 3.4: Convert Geometry class to namespace and eliminate JSON ✅
  - Converted stateless class to `geo` namespace
  - Created typed structs (Line, Arc, Circle, Extents, Path, Contour)
  - Eliminated ~250 lines of JSON compatibility code
  - Updated all geometric operations to use proper types
  - Removed JSON from hot paths (rendering, toolpath generation)

### In Progress
- Phase 1.3: Event System JSON Elimination ⚠️ NEXT PRIORITY
  - Identified JSON overuse in event callbacks
  - Need to replace JSON event parameters with typed structs
  - Affects ~50+ callback sites (hot path - every user interaction)

### Blocked
- None currently

---

## Notes & Observations

### Positive Observations
- Codebase already modernized in many areas (lambda callbacks, smart pointers, RAII)
- Good use of templates in some areas (`forEachPart`, etc.)
- Modern member variable naming (`m_variableName`)
- Proper constructor initialization
- ✅ Geometry system now fully type-safe (no JSON in hot paths)
- ✅ Most critical JSON runtime data converted to typed structs

### Areas of Concern
- ⚠️ **Event system still uses JSON** - every mouse/keyboard event creates/parses JSON (hot path!)
- ~~Heavy reliance on JSON for internal data structures~~ ✅ RESOLVED (Phase 1.1 complete - all runtime data uses typed structs)
- ~~String-based dispatch in performance-critical paths~~ ✅ RESOLVED (HMI buttons, G-code protocol, CAM actions)
- Large classes with many responsibilities
- Inconsistent error handling strategies

### Recent Discoveries
- **2025-12-28**: Completed Phase 1.1 - All runtime JSON data structures replaced with typed structs
  - `Primitive::data` → `PrimitiveUserData` std::variant (type-safe, extensible)
  - `getRunTime()` → returns typed `RuntimeData` struct
  - All critical runtime data (DRO, torch params, primitives, runtime) now type-safe
- **Event System JSON Overuse**: Found that the event system unnecessarily converts typed events (KeyEvent, MouseEvent, etc.) → JSON → back to typed data in handlers. This is a hot path affecting every user interaction.
- **Runtime vs Serialization**: Successfully distinguished between:
  - ✅ **Appropriate JSON use**: File I/O, network protocols, configuration persistence
  - ❌ **Inappropriate JSON use**: Runtime event passing, geometric calculations, internal state
- **Performance Impact**: Eliminated JSON from geometry operations (hundreds of calls per frame) and motion control (critical real-time path)

### Future Considerations
- Consider adding unit tests for refactored subsystems
- Documentation for complex state machines
- Performance profiling after string → enum conversions
- Evaluate whether C++23 features could be leveraged

---

**Last Review Date**: 2025-12-28
**Next Review**: After Phase 1.3 completion (Event system JSON elimination)
