# NanoCut Control
Front-End Control Software for NanoCut CNC Controller (see [NanoCut CNC firmware](https://github.com/Applooza/nanocut-firmware)). NanoCut was forked from [ncPilot](https://github.com/UnfinishedBusiness/ncPilot/) to continue development for my own plasma machine. NanoCut is Native Cross-Platform (Windows, Linux, and MacOS), but I have currently only tested it on Linux (Debian).

# Features
- 2D Gcode Viewer
- Built-In Machine Parameter Editing (No editing of Config files necessary)
- Has a primary focus on CNC Plasma cutting and CNC Routing as a secondary focus
- Click 'n Point Jogging
- Arrow Key Jogging
- Click and Point Gcode Jump in (No need to manually find a gcode line to jump into). Just hold control and click on the contour you would like to start from
- User configurable Layout
- Built in torch touchoff routine. Instead of a gcode, use fire_torch [pierce_height] [pierce_delay] [cut_height]. torch_off to shut torch off
- Built-in Toolpath workbench. Setup job options material size. Lay parts out in any configuration and post Gcode ready to run on the machine. Also has an early Auto-Nesting feature.
## Changes made by me
- Add support limit pin inversion
- Add support non-standard homing position for X-axis
- Remove firmware update feature
- Add customizable precise jog distance
- Add support for click-and-drag movement of the control view (press and hold right click)

**NOTE:** In its current state, this software does not fully support negative machine extents.
Positioning in the negative quadrant is apparently common traditionally for CNC machines, and GRBL
reports positions like this by default. I have instead decided to wire the motors and set the axis
inversion so that the machine operates in the positive quadrant so that I don't have to change
too much of the code.

# Post Processors
- SheetCAM post processor is included with the NanoCut firmware repository at [nanocut-firmware](https://github.com/Applooza/nanocut-firmware)
- NanoCut's built-in Toolpath Workbench posts gcode that specifically runs with NanoCut control/firmware setups.

# Simple plasma Gcode Program for slicing a sheet
```
fire_torch 0.160 1.6 0.075
G1 X0 Y0 F45
torch_off
M30
```

# Windows Build Instructions
- Install MSYS2
- pacman -Syu
- pacman -Su
- pacman -S mingw-w64-x86_64-toolchain
- pacman --noconfirm -S mingw64/mingw-w64-x86_64-glfw mingw64/mingw-w64-x86_64-freeglut
- PATH=$PATH:/mingw64/bin/
- cd /path/to/this/repo
- mingw32-make.exe
- ./NanoCut

# Linux (Debian) Build Instructions
- sudo apt-get -y install build-essential libglfw3-dev mesa-common-dev libglu1-mesa-dev freeglut3-dev libzlib1-dev
- cd /path/to/this/repo
- make
- ./NanoCut

# MacOSX Build Instructions
- brew install glfw
- cd /path/to/this/repo
- make
- ./NanoCut
