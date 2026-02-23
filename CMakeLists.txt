cmake_minimum_required(VERSION 3.21)  # FetchContent improvements in 3.21+
project(NanoCut LANGUAGES CXX C)

# Allow FetchContent_Populate for manual dependency configuration
if(POLICY CMP0169)
    cmake_policy(SET CMP0169 OLD)
endif()

#
# ------------------------------------------------------------
# Custom CMake modules
# ------------------------------------------------------------
#
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(EmbedResource)

#
# ------------------------------------------------------------
# Build type and C++ version
# ------------------------------------------------------------
#
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Debug or Release" FORCE)
endif()

#
# ------------------------------------------------------------
# Output folders
# ------------------------------------------------------------
#
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/${CMAKE_BUILD_TYPE})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/build/${CMAKE_BUILD_TYPE})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/build/${CMAKE_BUILD_TYPE})

#
# ------------------------------------------------------------
# Compiler flags
# ------------------------------------------------------------
#
set(BASE_COMPILE_FLAGS "-std=c++20")

set(CMAKE_CXX_FLAGS_DEBUG   "${BASE_COMPILE_FLAGS} -O0 -g -DDEBUG -DLOGURU_DEBUG_LOGGING")
set(CMAKE_CXX_FLAGS_RELEASE "${BASE_COMPILE_FLAGS} -DNDEBUG -O3")

#
# ------------------------------------------------------------
# Third-Party Dependencies via FetchContent
# ------------------------------------------------------------
#
include(FetchContent)

message(STATUS "Fetching third-party dependencies...")

# nlohmann/json - Header-only JSON library
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    FIND_PACKAGE_ARGS NAMES nlohmann_json
)

# Loguru - Logging library
FetchContent_Declare(
    loguru
    GIT_REPOSITORY https://github.com/emilk/loguru.git
    GIT_TAG v2.1.0
)

# ImGui - Immediate mode GUI library
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.92.6-docking
)

# ImGuiFileDialog - File dialog extension for ImGui
FetchContent_Declare(
    imguifiledialog
    GIT_REPOSITORY https://github.com/aiekick/ImGuiFileDialog.git
    GIT_TAG v0.6.7
)

# Mongoose - Embedded web server and networking library
FetchContent_Declare(
    mongoose
    GIT_REPOSITORY https://github.com/cesanta/mongoose.git
    GIT_TAG 7.2
)

# stb - Single-file public domain libraries (stb_truetype, stb_image, etc.)
FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG master
)

# Serial - Cross-platform serial port library by wjwwood
FetchContent_Declare(
    serial
    GIT_REPOSITORY https://github.com/wjwwood/serial.git
    GIT_TAG 1.2.1
)

# Make nlohmann/json available (has proper CMakeLists.txt)
message(STATUS "Setting up nlohmann/json...")
FetchContent_MakeAvailable(nlohmann_json)

# Loguru - fetch and manually create library target
message(STATUS "Setting up Loguru...")
FetchContent_GetProperties(loguru)
if(NOT loguru_POPULATED)
    FetchContent_Populate(loguru)
    add_library(loguru STATIC ${loguru_SOURCE_DIR}/loguru.cpp)
    target_include_directories(loguru PUBLIC ${loguru_SOURCE_DIR})
    target_compile_options(loguru PRIVATE -w)  # Suppress warnings
    if(NOT WIN32)
        target_link_libraries(loguru PUBLIC dl pthread)
    endif()
endif()

# ImGui - fetch and manually create library target (no CMakeLists.txt)
message(STATUS "Setting up ImGui...")
FetchContent_GetProperties(imgui)
if(NOT imgui_POPULATED)
    FetchContent_Populate(imgui)
    add_library(imgui STATIC
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_demo.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl2.cpp
    )
    target_include_directories(imgui PUBLIC
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
    )
    target_compile_options(imgui PRIVATE -w)  # Suppress warnings
endif()

# ImGuiFileDialog - fetch and manually create library target
message(STATUS "Setting up ImGuiFileDialog...")
FetchContent_GetProperties(imguifiledialog)
if(NOT imguifiledialog_POPULATED)
    FetchContent_Populate(imguifiledialog)
    add_library(imguifiledialog STATIC ${imguifiledialog_SOURCE_DIR}/ImGuiFileDialog.cpp)
    target_include_directories(imguifiledialog PUBLIC ${imguifiledialog_SOURCE_DIR})
    target_link_libraries(imguifiledialog PUBLIC imgui)
    target_compile_options(imguifiledialog PRIVATE -w)  # Suppress warnings
endif()

# Mongoose - fetch and manually create library target
message(STATUS "Setting up Mongoose...")
FetchContent_GetProperties(mongoose)
if(NOT mongoose_POPULATED)
    FetchContent_Populate(mongoose)
    add_library(mongoose STATIC ${mongoose_SOURCE_DIR}/mongoose.c)
    target_include_directories(mongoose PUBLIC ${mongoose_SOURCE_DIR})
    target_compile_definitions(mongoose PUBLIC MG_ENABLE_PACKED_FS=1)
    target_compile_options(mongoose PRIVATE -w)  # Suppress warnings
    if(WIN32)
        target_compile_definitions(mongoose PRIVATE _WIN32_WINNT=0x0600)
        target_link_libraries(mongoose PUBLIC ws2_32)
    endif()
endif()

# stb - header-only library, just make headers available
message(STATUS "Setting up stb...")
FetchContent_GetProperties(stb)
if(NOT stb_POPULATED)
    FetchContent_Populate(stb)
    # stb is header-only, no library target needed
    # Will add include directory to NanoCut target later
endif()

# Serial - Cross-platform serial port library (manual build due to old CMake)
message(STATUS "Setting up Serial...")
FetchContent_GetProperties(serial)
if(NOT serial_POPULATED)
    FetchContent_Populate(serial)

    # Platform-specific source files
    set(SERIAL_SRCS ${serial_SOURCE_DIR}/src/serial.cc)

    if(APPLE)
        list(APPEND SERIAL_SRCS
            ${serial_SOURCE_DIR}/src/impl/unix.cc
            ${serial_SOURCE_DIR}/src/impl/list_ports/list_ports_osx.cc
        )
    elseif(UNIX)
        list(APPEND SERIAL_SRCS
            ${serial_SOURCE_DIR}/src/impl/unix.cc
            ${serial_SOURCE_DIR}/src/impl/list_ports/list_ports_linux.cc
        )
    elseif(WIN32)
        list(APPEND SERIAL_SRCS
            ${serial_SOURCE_DIR}/src/impl/win.cc
            ${serial_SOURCE_DIR}/src/impl/list_ports/list_ports_win.cc
        )
    endif()

    add_library(serial STATIC ${SERIAL_SRCS})
    target_include_directories(serial PUBLIC ${serial_SOURCE_DIR}/include)
    target_compile_options(serial PRIVATE -w)  # Suppress warnings

    # Platform-specific libraries
    if(APPLE)
        target_link_libraries(serial PUBLIC "-framework IOKit" "-framework Foundation")
    elseif(UNIX)
        target_link_libraries(serial PUBLIC rt pthread)
    elseif(WIN32)
        target_link_libraries(serial PUBLIC setupapi)
    endif()
endif()

message(STATUS "All third-party dependencies configured successfully")

#
# ------------------------------------------------------------
# Embed application icons as C headers
# ------------------------------------------------------------
#
set(GENERATED_ICON_DIR "${CMAKE_BINARY_DIR}/generated/icons")
file(MAKE_DIRECTORY "${GENERATED_ICON_DIR}")

embed_resource("${CMAKE_SOURCE_DIR}/assets/nanocut-64.png"
               "${GENERATED_ICON_DIR}/nanocut_64_png.h" "nanocut_64_png")
embed_resource("${CMAKE_SOURCE_DIR}/assets/nanocut-128.png"
               "${GENERATED_ICON_DIR}/nanocut_128_png.h" "nanocut_128_png")
embed_resource("${CMAKE_SOURCE_DIR}/assets/nanocut-256.png"
               "${GENERATED_ICON_DIR}/nanocut_256_png.h" "nanocut_256_png")

#
# ------------------------------------------------------------
# dxflib - DXF file parsing library (local in ext/)
# ------------------------------------------------------------
#
add_library(dxflib STATIC
    ${CMAKE_SOURCE_DIR}/ext/dxflib/dl_dxf.cpp
    ${CMAKE_SOURCE_DIR}/ext/dxflib/dl_writer_ascii.cpp
)
target_include_directories(dxflib PUBLIC ${CMAKE_SOURCE_DIR}/ext)
target_compile_options(dxflib PRIVATE -w)  # Suppress warnings

#
# ------------------------------------------------------------
# Source files
# ------------------------------------------------------------
#
file(GLOB_RECURSE SRC_FILES src/*.cpp)

# Windows executable icon via resource file
if(WIN32 AND EXISTS "${CMAKE_SOURCE_DIR}/assets/nanocut.ico")
    set(WIN32_RESOURCES "${CMAKE_SOURCE_DIR}/assets/nanocut.rc")
else()
    set(WIN32_RESOURCES "")
endif()

add_executable(NanoCut ${SRC_FILES} ${WIN32_RESOURCES})

# Suppress warnings in stb_image implementation
set_source_files_properties(src/NcApp/stb_image_impl.cpp
    PROPERTIES COMPILE_FLAGS "-w")

target_include_directories(NanoCut PRIVATE
    src
    ${stb_SOURCE_DIR}              # stb header-only libraries
    ${CMAKE_BINARY_DIR}/generated  # embedded icon headers
)

#
# ------------------------------------------------------------
# Platform-specific config directory setup
# ------------------------------------------------------------
#
if(WIN32)
    set(CONFIG_DIR "$ENV{APPDATA}/NanoCut")
elseif(APPLE)
    set(CONFIG_DIR "$ENV{HOME}/Library/NanoCut")
else() # Linux and other Unix-like systems
    set(CONFIG_DIR "$ENV{HOME}/.config/NanoCut")
endif()

target_compile_definitions(NanoCut PRIVATE)

#
# ------------------------------------------------------------
# Git versioning (mirrors Makefile)
# ------------------------------------------------------------
#
find_package(Git QUIET)
if(GIT_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --tags --long --dirty --always
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        RESULT_VARIABLE GIT_RESULT
        OUTPUT_VARIABLE GIT_DESC
        ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    if(GIT_RESULT EQUAL 0)
        string(REGEX MATCH "([0-9]+)\\.([0-9]+)\\.([0-9]+)-([0-9]+)-(.*)"
               _ ${GIT_DESC})
        set(VERSION_MAJOR ${CMAKE_MATCH_1})
        set(VERSION_MINOR ${CMAKE_MATCH_2})
        set(VERSION_PATCH ${CMAKE_MATCH_3})
        set(VERSION_REVISION ${CMAKE_MATCH_4})
        set(VERSION_HASH ${CMAKE_MATCH_5})

        target_compile_definitions(NanoCut PRIVATE
            VERSION_MAJOR=${VERSION_MAJOR}
            VERSION_MINOR=${VERSION_MINOR}
            VERSION_PATCH=${VERSION_PATCH}
            VERSION_REVISION=${VERSION_REVISION}
            VERSION_HASH="${VERSION_HASH}"
        )
    endif()
endif()

#
# ------------------------------------------------------------
# Link Dependencies
# ------------------------------------------------------------
#
target_link_libraries(NanoCut PRIVATE
    nlohmann_json::nlohmann_json
    loguru
    imgui
    imguifiledialog
    mongoose
    dxflib
    serial
)

#
# ------------------------------------------------------------
# GLFW (prefer CONFIG, fallback to pkg-config behavior)
# ------------------------------------------------------------
#
find_package(glfw3 QUIET CONFIG)
if(glfw3_FOUND)
    target_link_libraries(NanoCut PRIVATE glfw)
    target_link_libraries(imgui PUBLIC glfw)  # ImGui backends need GLFW
else()
    if(WIN32)
        message(FATAL_ERROR "glfw3 not found, but is required to build. Install with e.g 'vcpkg install glfw3'")
    endif()
    # fallback to pkg-config
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GLFW REQUIRED glfw3)
    target_include_directories(NanoCut PRIVATE ${GLFW_INCLUDE_DIRS})
    target_link_libraries(NanoCut PRIVATE ${GLFW_LIBRARIES})
    target_include_directories(imgui PUBLIC ${GLFW_INCLUDE_DIRS})
    target_link_libraries(imgui PUBLIC ${GLFW_LIBRARIES})
endif()

#
# ------------------------------------------------------------
# OpenGL
# ------------------------------------------------------------
#
# Check for OpenGL headers and libraries
find_package(OpenGL QUIET)
if(NOT OPENGL_FOUND)
    message(FATAL_ERROR "OpenGL not found, but is required to build. Install OpenGL development packages.")
endif()

target_include_directories(NanoCut PRIVATE ${OPENGL_INCLUDE_DIRS})

#
# ------------------------------------------------------------
# Platform-specific linker settings
# ------------------------------------------------------------
#
if(APPLE)
    # macOS uses frameworks for OpenGL
    target_link_libraries(NanoCut PRIVATE
        "-framework Cocoa"
        "-framework IOKit"
        "-framework OpenGL"
    )
elif(WIN32)
    # Corresponds to NT block in Makefile
    target_link_libraries(NanoCut PRIVATE
        ${OPENGL_LIBRARIES}
        ws2_32 ole32 oleaut32 comdlg32 hid setupapi pthread
    )
else() # Linux
    target_link_libraries(NanoCut PRIVATE ${OPENGL_LIBRARIES} dl pthread)
endif()

# Always link z and m like the Makefile
target_link_libraries(NanoCut PRIVATE z m)

#
# ------------------------------------------------------------
# Installation
# ------------------------------------------------------------
#
install(TARGETS NanoCut DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

# Install fonts to system data directory
install(DIRECTORY assets/fonts/ DESTINATION ${CONFIG_DIR}/fonts)

# Install themes to system data directory
install(DIRECTORY assets/themes/ DESTINATION ${CONFIG_DIR}/themes)

# Linux desktop integration (freedesktop icons and .desktop entry)
if(UNIX AND NOT APPLE)
    include(GNUInstallDirs)
    install(FILES assets/nanocut-64.png
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/64x64/apps
        RENAME nanocut.png)
    install(FILES assets/nanocut-128.png
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/128x128/apps
        RENAME nanocut.png)
    install(FILES assets/nanocut-256.png
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps
        RENAME nanocut.png)
    install(FILES assets/nanocut-512.png
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/512x512/apps
        RENAME nanocut.png)
    install(FILES assets/NanoCut.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
endif()

#
# ------------------------------------------------------------
# Symlink like Makefile does in its "all" target
# ------------------------------------------------------------
#
if(UNIX)
    add_custom_target(symlink ALL
        COMMAND ${CMAKE_COMMAND} -E create_symlink
            ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/NanoCut
            ${CMAKE_BINARY_DIR}/NanoCut
        DEPENDS NanoCut
    )
endif()

