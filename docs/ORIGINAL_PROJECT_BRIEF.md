# Original Project Brief

This file keeps the initial project idea as an English historical reference.
The implemented project now uses ESPHome LVGL YAML directly instead of a
SquareLine Studio export.

## Goal

Build a professional Home Assistant thermostat and AC controller for an
Elecrow CrowPanel ESP32-S3 rotary display with a round touch screen and a
rotary encoder.

The device should:

- Connect to Home Assistant through ESPHome.
- Show current and target temperature.
- Change target temperature with the rotary encoder.
- Change HVAC mode and presets.
- Support room or entity changes in a future expansion.
- Show connection state and important status information.
- Provide a modern round UI that works well on the CrowPanel display.

## Preferred Architecture

Use ESPHome whenever possible. Keep the project split into:

- Board-specific ESPHome YAML files.
- Shared ESPHome logic.
- Local components.
- Assets.
- Documentation.

The current implementation follows that architecture with:

- `thermostat_480.yaml`
- `thermostat_240.yaml`
- `thermostat_common.yaml`
- `components/cst826/`
- `assets/`
- `docs/`

## Interaction Requirements

- Rotate right: increase target temperature.
- Rotate left: decrease target temperature.
- Short press: open the next relevant screen or setting.
- Long press: go back.
- Double click or swipe: switch active target in dual-setpoint mode.
- Debounce Home Assistant service calls so fast encoder movement does not send
  excessive updates.

## Documentation Requirements

The project should document:

- What the firmware does.
- Hardware assumptions.
- Python, pip, and ESPHome installation.
- YAML validation.
- Flashing.
- Home Assistant configuration.
- Controls.
- UI concept and future extension points.
