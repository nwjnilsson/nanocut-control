# Installation Guide

## Prerequisites

NanoCut Control requires the following dependencies:

### Required Dependencies
- **CMake 3.21+** (required for FetchContent improvements)
- **C++20 compatible compiler** (GCC 11+, Clang 14+, MSVC 2022+)
- **OpenGL** development libraries
- **GLFW** development libraries
- **Git** (for version information and dependency fetching)

### Optional Dependencies
- **pkg-config** (fallback for GLFW detection)

## Building from Source
Building is similar for all platforms thanks to CMake. An installation step is
provided via CMake, which will install the themes and fonts used by the program.
You specify where to install the program with e.g<br>
`-DCMAKE_INSTALL_PREFIX="C:\Users\MyNameJeff\AppData\Local\NanoCut"`

### Windows

#### Option 1: Using MSVC with Ninja (Lightweight)

1. **Install build tools via winget:**

        winget install --id Kitware.CMake --accept-package-agreements --accept-source-agreements
        winget install --id Git.Git --accept-package-agreements --accept-source-agreements
        winget install --id Microsoft.VisualStudio.2022.BuildTools --accept-package-agreements --accept-source-agreements
        winget install --id Ninja-build.Ninja --accept-package-agreements --accept-source-agreements

2. **Install dependencies via vcpkg:**

        git clone https://github.com/microsoft/vcpkg.git
        .\vcpkg\bootstrap-vcpkg.bat
        .\vcpkg\vcpkg install glfw3 zlib

3. **Configure CMake with Ninja generator:**

        mkdir build
        cd build
        cmake .. -G Ninja -DCMAKE_TOOLCHAIN_FILE=..\vcpkg\scripts\buildsystems\vcpkg.cmake -DCMAKE_BUILD_TYPE=Release

4. **Build:**

        ninja

5. **Install (optional):**

        cmake --install . --prefix "%APPDATA%\NanoCut"

#### Option 2: Using MSYS2 (Alternative)

1. **Install MSYS2:**

        winget install --id MSYS2.MSYS2 --accept-package-agreements --accept-source-agreements

2. **Update and install dependencies:**

        pacman -Syu
        pacman -S --needed base-devel mingw-w64-x86_64-toolchain mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja git mingw-w64-x86_64-glfw

3. **Configure and build:**

        mkdir build
        cd build
        cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release
        ninja

**Note:** For both options, ensure all tools are in your PATH before running CMake commands.

### Linux (NixOS)

1. Enter the development environment:

         nix-shell

2. Configure and build:

         mkdir build && cd build
         cmake .. -DCMAKE_INSTALL_PREFIX=. -DCMAKE_BUILD_TYPE=Release
         make

You can of course install it to wherever you want.

### Linux (Non-NixOS)

1. Install required dependencies:

         sudo apt-get install -y \
          build-essential \
          pkg-config \
          cmake \
          libglfw3-dev \
          mesa-common-dev \
          libglu1-mesa-dev \
          freeglut3-dev \
          libgl1-mesa-dev \
          zlib1g-dev

2. Configure and build:

         mkdir build && cd build
         cmake .. -DCMAKE_INSTALL_PREFIX=. -DCMAKE_BUILD_TYPE=Release
         make

### macOS (TODO..)

1. Install dependencies using Homebrew:

         brew install cmake

2. Configure and build:

         mkdir build && cd build
         cmake .. -DCMAKE_INSTALL_PREFIX=. -DCMAKE_BUILD_TYPE=Release
         make

### Installation

To install the application (Linux users need to `sudo` if installing to protected directories):

```bash
make install
```

This will install:

- The `NanoCut` executable to e.g `/usr/local/bin/` (your CMAKE_INSTALL_PREFIX)
- Fonts to `~/.config/NanoCut/fonts/`
- Themes to `~/.config/NanoCut/themes/`

## Running the Application

After building, you can run the application:

```bash
# From build directory
./bin/Release/NanoCut

# After installation
NanoCut
```

## CMake Configuration Details

The build system uses CMake's FetchContent to automatically download and build all third-party dependencies:

- **nlohmann/json** v3.11.3 (JSON library)
- **Loguru** v2.1.0 (logging library)
- **ImGui** v1.92.6-docking (GUI library)
- **ImGuiFileDialog** v0.6.7 (file dialog extension)
- **Mongoose** 7.2 (web server/networking)
- **stb** (header-only utilities)
- **serial** 1.2.1 (cross-platform serial port library)
- **dxflib** (local DXF parsing library)

## Troubleshooting

### Common Issues

**CMake version too old:**
```
CMake Error: cmake_minimum_required(VERSION 3.21)
```
Upgrade CMake to version 3.21 or later.

**Missing OpenGL development packages:**
```
fatal error: GL/gl.h: No such file or directory
```
Install OpenGL development packages for your distribution.

**GLFW not found:**
```
Could NOT find glfw3
```
Install GLFW development packages or let CMake download it automatically.

**Compiler doesn't support C++20:**
```
error: ‘std::format’ is not a member of ‘std’
```
Upgrade your compiler to GCC 11+, Clang 14+, or MSVC 2022+.

### Clean Build

If you encounter build issues, try a clean build:

```bash
rm -rf build
mkdir build && cd build
cmake ..
make
```
