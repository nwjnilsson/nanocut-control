# Machine Parameters

## Accessing Parameters

Open the **Machine Parameters** dialog from Control View to view and modify machine
settings. Changes are written to the controller immediately.

## Standard grbl Parameters

NanoCut firmware is based on grbl. The following standard parameters are available:

- **Machine extents**: Maximum travel for each axis
- **Axis scale**: Steps per unit (steps/mm or steps/inch)
- **Max velocity**: Maximum speed for each axis
- **Max acceleration**: Maximum acceleration for each axis
- **Homing settings**: Feed rate, seek rate, debounce time, pull-off distance, and
  direction for each axis
- **Axis inversion**: Reverse direction for individual axes
- **Limit pin inversion**: Invert the logic of limit switch inputs
- **Probe pin inversion**: Invert the logic of the probe input
- **Soft limits**: Prevent the machine from moving beyond its defined extents

For detailed descriptions of all standard grbl parameters, refer to the
[grbl v1.1 Configuration documentation](https://github.com/gnea/grbl/wiki/Grbl-v1.1-Configuration).

## NanoCut-Specific Parameters

These parameters are specific to the NanoCut firmware and control plasma cutting
behavior.

### Arc Voltage Divider ($33)

Calibration factor for reading arc voltage from the plasma cutter.

- **Default**: 50 (1:50 voltage divider)
- **Range**: 1--500

This value must match your hardware voltage divider circuit. The controller reads a
scaled-down voltage from the plasma cutter and multiplies it by this factor to
calculate the actual arc voltage. If your arc voltage readings are incorrect, adjust
this parameter to match your divider ratio.

### Arc Stabilization Time

Time in milliseconds to wait after the torch fires before THC becomes active.

- **Default**: 2000 ms

When the torch first fires, the arc voltage fluctuates as it stabilizes. This delay
prevents THC from making unnecessary height adjustments during that initial period.
Increase this value if you notice erratic torch movement at the start of each cut.

### Floating Head Backlash

Distance the floating head travels from its gravity rest position to the probe trigger
point.

This compensates for mechanical backlash in the Z-axis probing system. When the torch
probe touches the material, the floating head must travel through this backlash
distance before the probe switch triggers. Setting this correctly ensures accurate
touch-off measurements.

### Precise Jog Distance

The distance moved when using **Ctrl+Arrow keys** for precise jogging.

Set this to match your typical fine-positioning needs. Useful for accurately setting
work zero or verifying clearances.

## Cutting Extents

Defines a safe rectangle within the machine extents where cutting is allowed. This
provides a safety margin around the edges of the machine's travel. The safe cutting
zone is displayed as a green rectangle in [Control View](control-view.md).

Programs that extend outside the cutting extents will be rejected before running.

## Work Offsets

Work offsets define the relationship between machine coordinates and work coordinates.
Set them using the **Zero X** and **Zero Y** buttons in Control View. These offsets
are saved automatically and persist across sessions.
