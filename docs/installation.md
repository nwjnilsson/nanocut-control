# Installation Guide

## Prerequisites

NanoCut Control requires the following dependencies:

### Required Dependencies
- **Git** (for version information and dependency fetching)
- **CMake 3.21+**
- **C++20 compatible compiler** (GCC 11+, Clang 14+, MSVC 2022+)
- **OpenGL** development libraries
- **GLFW** development libraries

## Building from Source
Building is similar for all platforms thanks to CMake. An installation step is
provided via CMake, which will install the executable, themes, and fonts.

### Pre-requisites
Downloading/updating is done with Git. Windows users can run `winget install Git.Git`, then restart your terminal.

#### First-time setup

Clone the repository and check out the desired release:

    git clone https://github.com/nwjnilsson/nanocut-control.git
    cd nanocut-control
    git checkout v1.0.0

#### Updating

Fetch the latest tags and update the source:

    git fetch --tags
    git checkout latest

I don't recommend staying on `main`.

### Windows
A powershell helper script is provided to help Windows users build from source.
The script will check for CMake, Ninja, and Visual Studio Build Tools —
offering to install any that are missing via `winget` — then configure, build,
and install the executable to `bin\NanoCut.exe` inside the repository.

Simply run:

    .\scripts\build-windows.ps1

If you already have vcpkg, you can point to it with e.g `-VcpkgPath C:\vcpkg`

### Linux
#### NixOS

1. A `shell.nix` is provided, enter the environment:

         nix-shell

2. Configure and build:

         mkdir build && cd build
         cmake .. -DCMAKE_INSTALL_PREFIX=.... -DCMAKE_BUILD_TYPE=Release
         make

#### APT-based

1. Install required dependencies:

         sudo apt-get install -y \
          build-essential \
          pkg-config \
          cmake \
          libglfw3-dev \
          mesa-common-dev \
          libglu1-mesa-dev \
          libgl1-mesa-dev \
          zlib1g-dev

2. Configure and build:

         mkdir build && cd build
         cmake .. -DCMAKE_INSTALL_PREFIX=. -DCMAKE_BUILD_TYPE=Release
         make

### macOS

I have not tested on macOS myself but the process should look something like this. I'm sure you'll figure it out :)

1. Install dependencies using Homebrew:

         brew install cmake glfw3 ...

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
cmake [same parameters as the first time]
make/ninja/......
```
