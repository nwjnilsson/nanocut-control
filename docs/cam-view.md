# CAM View (Toolpath Workbench)

## Overview

CAM View is where you design toolpaths from DXF files. Here you import parts, configure
cutting tools, create operations, arrange parts on the material sheet, and generate
G-code for the controller.

Switch to CAM View by pressing **Tab** from Control View.

## Importing a DXF

Open **File > Import Part** (or use the import button in the left pane) to load a DXF
file.

### DXF Importer Dialog

When importing, you can configure:

- **Quality** (8--16): Controls curve approximation resolution. Higher values produce
  smoother curves with more line segments.
- **Scale factor**: Multiplier applied to the imported geometry.
- **Unit auto-detection**: The importer reads the DXF header to determine whether the
  file uses millimeters or inches and converts accordingly.

### Supported Geometry

The importer handles the following DXF entities:

- Lines
- Arcs
- Circles
- Polylines (including bulge arcs)
- Splines
- Ellipses

### Layer Filtering

Layers named "Construction" and entities with non-continuous linetypes are automatically
skipped during import.

## Parts Panel (Left Pane)

The left pane shows a tree view of all imported parts.

- **Master parts and duplicates**: When you duplicate a part, the copy is linked to the
  master. Changes to the master propagate to duplicates.
- **Layer visibility**: Toggle individual layers on or off.
- **Right-click context menu**: Delete parts, view properties.
- **Properties window**: Shows dimensions, path count, vertex count, scale, rotation
  angle, and simplification settings.

## Job Options

Configure the cutting job parameters:

- **Material size**: Set the width and height of your sheet metal. This defines the
  working area shown in the viewport.
- **Origin corner**: Choose which corner of the material corresponds to the work
  zero position. Four options are available (one for each corner).

## Tool Library

The Tool Library stores your cutting tool definitions. Each tool represents a set of
plasma cutting parameters for a specific material and thickness.

### Creating and Editing Tools

Each tool has the following parameters:

| Parameter | Description |
|---|---|
| **Tool name** | A descriptive name (e.g., "3mm Mild Steel") |
| **Pierce height** | Z height during the initial pierce. Set higher than cut height to protect the torch from blowback. |
| **Pierce delay** | Time in seconds to wait after firing the torch before moving. Allows the arc to fully penetrate the material. |
| **Cut height** | Z height maintained during cutting. Critical for cut quality. |
| **Kerf width** | Width of material removed by the plasma arc. Used for offset compensation so finished parts match the design dimensions. |
| **Feed rate** | Cutting speed. Too fast causes dross on the bottom; too slow causes excessive heat and wider kerf. |
| **THC target voltage** | Arc voltage setpoint for Torch Height Control. THC adjusts the torch height during cutting to maintain this voltage, which corresponds to a consistent cut height. |

## Tool Operations

Operations connect tools to DXF layers, defining how each layer gets cut.

### Creating an Operation

1. Select a **tool** from the Tool Library.
2. Select a **layer** from the imported parts.
3. Configure **lead-in length** and **lead-out length**. These control the approach and
   exit paths that prevent the pierce from damaging the cut edge. Default is 1.5x the
   tool's kerf width.

### Managing Operations

- **Enable/disable**: Toggle operations on or off without deleting them.
- **Operations list**: All operations appear in the left pane. The order determines the
  cutting sequence.

## Part Layout and Nesting

### Manual Positioning

Switch to **Nesting Tool** mode to move parts:

- **Drag** parts to reposition them on the material sheet.
- **Ctrl+scroll** on a selected part to rotate it in 5-degree increments.
- **Shift+scroll** on a selected part to scale it.

### Automatic Nesting

Press the **Arrange** button to run automatic nesting optimization. This positions parts
to minimize material waste.

## Exporting

### Save G-code to File

Use **File > Save GCode** to write the generated toolpath to a `.nc` or `.gcode` file
for later use or transfer to another machine.

### Send to Controller

Press **Send to Controller** to transfer the G-code directly to the machine. This
automatically switches to [Control View](control-view.md) so you can run the job.

## View Controls

| Control | Action |
|---|---|
| Right-click drag | Pan the view |
| Scroll wheel | Zoom in/out |
| Contour Tool | Select and edit contours |
| Nesting Tool | Select, move, rotate, and scale parts |
