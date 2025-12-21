# NanoCut Refactoring Roadmap

**Last Updated**: 2025-12-21

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

- [ ] **Replace `m_dro_data` JSON with typed struct** (HIGH PRIORITY)
  - **Location**: `motion_controller.h:57`
  - **Problem**: Digital readout data stored as untyped JSON, accessed every frame
  - **Current**: `m_dro_data["IN_MOTION"]`, `m_dro_data["ARC_OK"]`, `m_dro_data["MCS"]["x"]`
  - **Solution**:
    ```cpp
    struct MCSCoordinates { float x, y, z; };
    struct DROData {
        MCSCoordinates mcs;
        MCSCoordinates wcs;
        bool in_motion;
        bool arc_ok;
        bool torch_on;
        // ... other fields
    };
    ```
  - **Impact**: Compile-time checking, better performance, clearer API
  - **References**: Used throughout `motion_controller.cpp` and `hmi.cpp`

- [ ] **Replace `m_callback_args` JSON with typed struct** (HIGH PRIORITY)
  - **Location**: `motion_controller.h:58`
  - **Problem**: Torch parameters stored as JSON, no compile-time checking
  - **Current**: Lines 398-410 show string key access
  - **Solution**:
    ```cpp
    struct TorchParameters {
        double pierce_height;
        double pierce_delay;
        double cut_height;
        double kerf_width;
        double traverse_height;
    };
    ```
  - **Impact**: Type safety for critical cutting parameters

- [ ] **Replace `Primitive::data` JSON field** (MEDIUM PRIORITY)
  - **Location**: `src/NcRender/primitives/Primitive.h:36`
  - **Problem**: Generic JSON storage for arbitrary primitive data
  - **Current**: `g->data = { { "rapid_line", rapid_line } };` in `gcode.cpp:144`
  - **Solution**: Use `std::variant` or templated user data
  - **Impact**: Better type safety, reduced allocations

### 1.2 String-Based Dispatch → Enums ⚠️ CRITICAL

- [ ] **Convert NcCamView action dispatch to enums** (HIGH PRIORITY)
  - **Location**: `src/NcCamView/NcCamView.cpp:1213-1233`
  - **Problem**: String comparisons for command dispatch
  - **Current**: `if (action.action_id == "post_process") { ... }`
  - **Solution**:
    ```cpp
    enum class ActionType {
        PostProcess,
        RebuildToolpaths,
        DeleteOperation,
        DeleteDuplicateParts,
        DeleteParts,
        DeletePart,
        DuplicatePart
    };
    ```
  - **Impact**: Compile-time checking, typo prevention, better performance

- [ ] **Convert HMI button IDs to enums** (HIGH PRIORITY)
  - **Location**: `src/NcControlView/hmi/hmi.cpp:76-354`
  - **Problem**: 278 lines of string comparisons for button handling
  - **Current**: `if (id == "Wpos") { ... } else if (id == "Park") { ... }`
  - **Solution**: Enum class with switch statement or lookup map
  - **Impact**: Much cleaner, faster, typo-proof

- [ ] **Refactor G-code protocol parsing** (HIGH PRIORITY)
  - **Location**: `src/NcControlView/motion_control/motion_controller.cpp:238-382`
  - **Problem**: 15+ `line.find("...")` checks in performance-critical path
  - **Current**: Fragile string matching for each message type
  - **Solution**:
    ```cpp
    enum class GrblMessageType {
        DROData,
        UnlockRequired,
        ChecksumFailure,
        Error,
        Alarm,
        Crash,
        ProbeResult,
        GrblReady,
        Unknown
    };
    GrblMessageType parseMessageType(const std::string& line);
    ```
  - **Impact**: Better performance (called for every serial line), more maintainable

### 1.3 Array Access Safety

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

- [ ] **Use NLOHMANN_DEFINE_TYPE for MachineParameters** (HIGH PRIORITY)
  - **Location**: `src/NcControlView/NcControlView.cpp:67-206`, `motion_controller.cpp:927-1042`
  - **Problem**: 140 lines of manual JSON deserialization + 116 lines of serialization
  - **Current**: Repetitive `param.field = json["field"]` patterns
  - **Solution**: Use `NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE` macro
  - **Impact**: ~200 lines eliminated, less error-prone

- [ ] **Convert NcCamView structs to typed fields** (HIGH PRIORITY)
  - **Location**: `src/NcCamView/NcCamView.h:44-58`
  - **Problem**: `ToolData::params` uses `std::unordered_map<std::string, float>` with magic string keys
  - **Current**: Access like `params["pierce_height"]`, `params["cut_height"]`
  - **Solution**: Convert to strongly-typed struct fields
  - **Impact**: Type safety, autocomplete, compile-time checking

### 2.2 Safe JSON Helpers

- [ ] **Create safe JSON accessor helpers** (MEDIUM PRIORITY)
  - **Location**: Throughout codebase
  - **Problem**: Using `.get<T>()` without error handling
  - **Solution**:
    ```cpp
    template<typename T>
    std::optional<T> safe_get(const nlohmann::json& j, const std::string& key) {
        if (j.contains(key)) {
            try {
                return j[key].get<T>();
            } catch (...) {
                return std::nullopt;
            }
        }
        return std::nullopt;
    }
    ```
  - **Impact**: Exception safety, clearer error handling

- [ ] **Replace exception-based control flow** (HIGH PRIORITY)
  - **Location**: `src/NcControlView/hmi/hmi.cpp:100-150`
  - **Problem**: Using try/catch to handle missing JSON fields
  - **Current**:
    ```cpp
    try {
        control_view.m_machine_parameters.work_offset[0] =
            control_view.m_motion_controller->getDRO()["MCS"]["x"].get<float>();
    } catch (...) {
        LOG_F(ERROR, "Could not set x work offset!");
    }
    ```
  - **Solution**: Use `contains()` or return `std::optional<T>`
  - **Impact**: Better performance (exceptions expensive for control flow)

---

## Phase 3: Architectural Improvements

**Goal**: Break up God objects, improve separation of concerns
**Estimated Effort**: 2-3 weeks
**Priority**: High

### 3.1 Split MotionController God Object

- [ ] **Refactor MotionController (1,147 lines)** (HIGH PRIORITY)
  - **Location**: `src/NcControlView/motion_control/motion_controller.cpp`
  - **Problem**: Handles too many responsibilities:
    - Serial communication
    - G-code queue management
    - Callback management
    - Parameter persistence
    - Torch control state machine
    - Error handling and logging
    - CRC calculation
  - **Solution**: Split into focused subsystems:
    - `MotionController` - high-level coordination
    - `TorchStateMachine` - torch control logic
    - `GrblProtocolHandler` - message parsing
    - `ParameterManager` - save/load/validate parameters
  - **Impact**: Much more maintainable, testable, follows SRP

### 3.2 Split NcCamView God Object

- [ ] **Refactor NcCamView (1,359 lines)** (HIGH PRIORITY)
  - **Location**: `src/NcCamView/NcCamView.cpp`
  - **Problem**: Handles everything CAM-related:
    - DXF import
    - Toolpath generation
    - Part management
    - UI rendering (6 separate render methods)
    - Nesting algorithms
    - Action dispatch
    - Tool library management
  - **Solution**: Extract subsystems:
    - `PartManager` - part lifecycle
    - `ToolLibrary` - tool management
    - `ToolpathGenerator` - path operations
    - `DXFImporter` - file import
    - `NestingEngine` - layout optimization
  - **Impact**: Better separation of concerns, easier testing

### 3.3 Reduce Coupling

- [ ] **Dependency injection instead of NcApp access** (MEDIUM PRIORITY)
  - **Location**: Throughout all views, 50+ instances
  - **Problem**: Views directly access `m_app->getRenderer()`, `m_app->getControlView()`, etc.
  - **Current**: Tight coupling to entire NcApp
  - **Solution**: Inject specific dependencies in constructors
  - **Impact**: Follows dependency inversion principle, better testability

- [ ] **Fix MotionController direct UI manipulation** (HIGH PRIORITY)
  - **Location**: `motion_controller.cpp:1014-1032`
  - **Problem**: MotionController directly manipulates view primitives
  - **Current**: `m_control_view->m_machine_plane->m_bottom_left.x = 0;`
  - **Solution**: MotionController should notify view of changes, not manipulate UI
  - **Impact**: Better MVC separation, more maintainable

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

- [ ] **Replace color string literals with enums** (MEDIUM PRIORITY)
  - **Location**: `src/NcCamView/NcCamView.cpp:1108-1111`
  - **Problem**: String literals like `"blue"`, `"white"`, `"grey"` (with "wtf" comments!)
  - **Solution**: Enum for semantic color roles, separate color palette
  - **Impact**: More maintainable color management

- [ ] **Replace primitive ID strings with enums** (MEDIUM PRIORITY)
  - **Location**: Throughout rendering code
  - **Current**: `"gcode"`, `"gcode_arrows"`, `"gcode_highlights"`, `"material_plane"`
  - **Solution**: Enum class for well-known primitive IDs
  - **Impact**: Typo prevention, autocomplete

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

If tackling only 3 improvements, start here:

1. **Replace `m_dro_data` and `m_callback_args` JSON with structs**
   - Immediate type safety
   - Better performance
   - Clearer API
   - ~200 lines affected

2. **Enum-based action/button dispatch**
   - Eliminate entire class of runtime bugs
   - Better performance
   - Compile-time checking
   - ~300 lines affected

3. **Use NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE**
   - Delete ~200 lines of boilerplate
   - Less error-prone
   - Easier to maintain
   - Quick to implement

---

## Progress Tracking

### Completed Items
- Phase 1: MotionController class implementation ✅
- Phase 2: NcApp and View architecture ✅
- Phase 3: Modern callback system and GUI modernization ✅
- Phase 4: Right-click pan and independent view transforms ✅
- Phase 5: Directory structure reorganization ✅

### In Progress
- None currently

### Blocked
- None currently

---

## Notes & Observations

### Positive Observations
- Codebase already modernized in many areas (lambda callbacks, smart pointers, RAII)
- Good use of templates in some areas (`forEachPart`, etc.)
- Modern member variable naming (`m_variableName`)
- Proper constructor initialization

### Areas of Concern
- Heavy reliance on JSON for internal data structures (not just serialization)
- String-based dispatch in performance-critical paths
- Large classes with many responsibilities
- Inconsistent error handling strategies

### Future Considerations
- Consider adding unit tests for refactored subsystems
- Documentation for complex state machines
- Performance profiling after string → enum conversions
- Evaluate whether C++23 features could be leveraged

---

**Last Review Date**: 2025-12-21
**Next Review**: After Phase 1 completion
