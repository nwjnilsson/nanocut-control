# Control View (Machine Control)

## Overview

Control View is where you operate the CNC plasma cutter. Here you jog the machine,
set work coordinates, run G-code programs, and monitor cutting in real time.

Switch to Control View by pressing **Tab** from CAM View.

## The DRO (Digital Readout) Pane

The DRO displays the current machine position across three axes: **X**, **Y**, and
**Z**.

### Coordinate Display

Each axis shows two numbers:

- **Large number** -- **Work Coordinate (WCS)**: Position relative to your work zero.
  This is the coordinate system you set with Zero X / Zero Y.
- **Small number** -- **Machine Coordinate (MCS)**: Absolute position relative to the
  machine home. This never changes unless you re-home.

### Status Line

Below the coordinates, four readouts show real-time cutting data:

| Readout | Description |
|---|---|
| **FEED** | Current feed rate (mm/min or in/min) |
| **ARC** | Current arc voltage reading from the plasma cutter |
| **SET** | THC voltage setpoint (the target voltage for torch height control) |
| **RUN** | Elapsed run time for the current program |

### Visual Indicators

- The DRO background turns **red** when the torch is firing.
- The **ARC** readout changes color to indicate arc_ok status (whether the plasma arc
  has transferred to the workpiece).

## The THC Widget

Below the DRO is a row of **6 buttons** for adjusting the torch height in real time
during cutting.

### How It Works

The buttons apply a voltage offset to the THC target:

| Button | Offset |
|---|---|
| -4 | Decrease target by 4V |
| -1.5 | Decrease target by 1.5V |
| -0.5 | Decrease target by 0.5V |
| Center | Displays current offset (e.g., "+2.0") |
| +0.5 | Increase target by 0.5V |
| +1.5 | Increase target by 1.5V |
| +4 | Increase target by 4V |

- **Offset range**: -12V to +12V
- **Effective voltage** = base THC target + offset (clamped to 0--250V)
- Adjustments only take effect while the torch is firing.

Use this to fine-tune cut height during a job if you notice the torch is too high or
too low.

## The HMI (Control Buttons) Pane

The HMI pane provides direct machine control buttons:

| Button | Description |
|---|---|
| **Zero X** | Sets the X work coordinate to zero at the current position |
| **Zero Y** | Sets the Y work coordinate to zero at the current position |
| **Touch** | Probes the material surface height (dry run, no torch fire). Performs a touch-off cycle to establish Z zero. |
| **Retract** | Raises the torch to Z=0 |
| **Fit** | Zooms the viewport to show all loaded G-code |
| **Clean** | Clears highlight paths from the view (e.g., arc-not-ok highlights) |
| **Wpos** | Rapid move to the work position origin (X0 Y0) |
| **Park** | Returns the machine to home position (machine coordinates) |
| **Home** | Initiates the homing cycle (finds limit switches) |
| **Test Run** | Runs the loaded G-code as a dry run -- probes the surface but does not fire the torch. Use this to verify the path before cutting. |
| **Run** | Executes the loaded G-code with the torch firing |
| **Abort** | Stops the current operation immediately |

## Machine Plane Display

The main viewport shows a top-down view of the machine working area:

- **Blue rectangle** -- Full machine working area (defined by machine extents).
- **Green rectangle** -- Safe cutting zone (cuttable extents). Programs that extend
  outside this area will be rejected.
- **Torch pointer** -- A circle that follows the real-time machine position.

### View Controls

| Control | Action |
|---|---|
| Right-click drag | Pan the view |
| Scroll wheel | Zoom in/out |
| **Fit** button | Zoom to show all G-code |

## Waypoint Navigation

Click anywhere on the machine plane to navigate the torch to that position. A
confirmation dialog will appear before the machine moves.

## Jump-In Feature

**Ctrl+left-click** on a G-code contour to start the program from that specific
contour. All contours before the clicked one are skipped. This is useful when:

- A cut failed partway through a job and you want to resume from a specific part.
- You only need to re-cut certain contours.

## Reverse Path

**Ctrl+right-click** on a contour to reverse its cutting direction. This is useful
when a cut fails mid-contour and you want to restart from the other end.

## Loading G-code

Load a G-code file via **File > Open**, or receive it directly from
[CAM View](cam-view.md) using **Send to Controller**.
