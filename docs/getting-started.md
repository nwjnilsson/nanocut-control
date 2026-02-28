# Getting Started

## Prerequisites

Before using NanoCut Control, you need:

- A CNC plasma cutting machine with a compatible grbl controller
- [NanoCut firmware](https://github.com/nwjnilsson/nanocut-firmware) flashed on the controller
- A USB connection between the controller and your computer
- A DXF file to cut (or you can use any DXF editor to create one)

## A Note on Units

NanoCut Control uses **metric units (millimeters)** by default. If you need imperial
units (inches), the application must be recompiled with the `USE_INCH_DEFAULTS` flag.

## First Launch

1. Connect your CNC controller to the computer via USB.
2. Launch NanoCut Control. The application opens in **Control View** by default.
3. Home the machine by pressing the **Home** button. This establishes the machine's
   coordinate system by finding the limit switches.

## Quick Workflow Overview

A typical cutting job follows these steps:

### 1. Switch to CAM View

Press **Tab** to switch to [CAM View](cam-view.md), the toolpath design workspace.

### 2. Import a DXF File

Use **File > Import Part** (or the import button in the left pane) to load your DXF
file. The importer will auto-detect units and convert geometry.

### 3. Set Up Job Options

Configure the **material size** (width and height) to match your sheet metal. Choose
the origin corner to match your work zero location.

### 4. Create a Tool

Open the **Tool Library** and create a tool with the correct plasma parameters for your
material:

- **Pierce height** -- Z height during the initial pierce
- **Pierce delay** -- time to wait for the pierce to complete
- **Cut height** -- Z height during cutting
- **Feed rate** -- cutting speed
- **Kerf width** -- width of material removed by the plasma arc
- **THC target voltage** -- arc voltage setpoint for Torch Height Control

### 5. Create a Tool Operation

Create an operation that assigns your tool to a DXF layer. Configure lead-in and
lead-out lengths (defaults to 1.5x kerf width).

### 6. Arrange Parts on the Material

Position parts on the sheet using the **Nesting Tool**. Drag parts to move them, use
**Ctrl+scroll** to rotate (5-degree increments), or press **Arrange** for automatic
nesting.

### 7. Send to Controller

Use **File > Save GCode** to export a G-code file, or use **Send to Controller** to
transfer the program directly and switch to Control View.

### 8. Switch to Control View

Press **Tab** to return to [Control View](control-view.md) (or it switches
automatically after Send to Controller).

### 9. Home, Touch Off, Run

1. **Home** the machine if not already done.
2. Jog to your desired work zero and press **Zero X** / **Zero Y**.
3. Press **Touch** to probe the material surface height.
4. Press **Test Run** to verify the path without firing the torch.
5. Press **Run** to cut the part.

## Essential Shortcuts

| Shortcut | Action |
|---|---|
| Tab | Switch between CAM View and Control View |
| Escape | Abort current operation |
| Arrow keys | Jog the machine (hold to move) |
| Ctrl+Arrow keys | Precise jog (fixed distance) |
| Ctrl+click | Jump-in: start program from a specific contour |

See the full [Keyboard Shortcuts](keyboard-shortcuts.md) reference for more.
