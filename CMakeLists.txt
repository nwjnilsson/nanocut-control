cmake_minimum_required(VERSION 3.16)
project(NanoCut LANGUAGES CXX)

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
# Output folders (matching Makefile)
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
set(BASE_COMPILE_FLAGS "-std=c++17")

set(CMAKE_CXX_FLAGS_DEBUG   "${BASE_COMPILE_FLAGS} -O0 -g -DDEBUG -DLOGURU_DEBUG_LOGGING")
set(CMAKE_CXX_FLAGS_RELEASE "${BASE_COMPILE_FLAGS} -DNDEBUG -O3")

#
# ------------------------------------------------------------
# Source files
# ------------------------------------------------------------
#
file(GLOB_RECURSE SRC_FILES src/*.cpp)

add_executable(NanoCut ${SRC_FILES})

target_include_directories(NanoCut PRIVATE src)

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

target_compile_definitions(NanoCut PRIVATE 
    CONFIG_DIRECTORY="${CONFIG_DIR_DEFINE}"
)

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
# OpenMP
# ------------------------------------------------------------
#
find_package(OpenMP REQUIRED)
target_link_libraries(NanoCut PRIVATE OpenMP::OpenMP_CXX)

#
# ------------------------------------------------------------
# GLFW (prefer CONFIG, fallback to pkg-config behavior)
# ------------------------------------------------------------
#
find_package(glfw3 QUIET CONFIG)
if(glfw3_FOUND)
    target_link_libraries(NanoCut PRIVATE glfw)
else()
    # fallback to pkg-config
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GLFW REQUIRED glfw3)
    target_include_directories(NanoCut PRIVATE ${GLFW_INCLUDE_DIRS})
    target_link_libraries(NanoCut PRIVATE ${GLFW_LIBRARIES})
endif()

#
# ------------------------------------------------------------
# Platform-specific linker settings (exact Makefile ports)
# ------------------------------------------------------------
#
if(APPLE)
    target_link_libraries(NanoCut PRIVATE
        "-framework Cocoa"
        "-framework IOKit"
        "-framework OpenGL"
    )
elseif(WIN32)
    # Corresponds to NT block in Makefile
    target_compile_definitions(NanoCut PRIVATE FREEGLUT_STATIC)
    target_link_libraries(NanoCut PRIVATE
        opengl32
        glu32
        ws2_32 ole32 oleaut32 comdlg32 hid setupapi pthread
    )
else() # Linux
    target_link_libraries(NanoCut PRIVATE GL GLU dl pthread)
endif()

# Always link z and m like the Makefile
target_link_libraries(NanoCut PRIVATE z m)

#
# ------------------------------------------------------------
# Installation (matches Makefile)
# ------------------------------------------------------------
#
install(TARGETS NanoCut DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

# Install fonts to system data directory
install(DIRECTORY assets/fonts/ DESTINATION ${CONFIG_DIR}/fonts)

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

