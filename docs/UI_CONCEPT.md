# UI Concept

The current UI is a compact round thermostat interface for the Elecrow
CrowPanel rotary displays. The layout is implemented directly in ESPHome LVGL
YAML, with separate native layouts for 480x480 and 240x240 panels.

## Design Goals

- Dark background with high-contrast white text.
- Large temperature readout optimized for quick reading.
- Circular target temperature ring.
- Minimal text on the device.
- Rotary encoder first, touch input second.
- No decorative animations or complex navigation.

## Screens

### Home

The home screen shows the current temperature, HVAC mode, optional preset,
humidity when available, time, date, and connection state.

Primary widgets:

- `label_home_current_temperature`
- `label_home_current_unit`
- `label_home_current_decimal`
- `label_home_hvac_mode`
- `label_home_preset`
- `label_home_time`
- `label_home_weekday`
- `label_home_date`

### Thermostat

The thermostat screen shows the target temperature, current temperature, HVAC
mode, preset, ring indicator, and optional dual-setpoint mode label.

Primary widgets:

- `arc_temperature`
- `label_target_temperature`
- `label_target_unit`
- `label_target_decimal`
- `label_current_temperature`
- `label_hvac_mode`
- `label_preset`
- `label_setpoint_mode`

### Settings

The settings screen is a rotary menu. The encoder scrolls through settings and
a short press opens or applies the selected setting.

Menu entries:

- HVAC Mode
- Preset
- Brightness
- Clock
- Language
- Design
- Icons
- Unit
- Idle Time
- Dim After
- Dim Level
- 3s Button
- Knob Modes
- Knob Step
- WiFi
- Firmware
- Reset
- LED
- LED Brightness

## Controls

| Action | Result |
|---|---|
| Rotate on home | Open thermostat screen |
| Short press on home | Open thermostat screen |
| Rotate on thermostat | Change target temperature |
| Short press on thermostat | Open settings |
| Double click on thermostat | Switch active dual-setpoint target |
| 1 second press | Go back one level |
| 3 second press | Open HVAC or preset submenu |
| Swipe on thermostat | Switch active dual-setpoint target |

## Color Behavior

Mono mode keeps the UI mostly white and gray. Color mode uses orange for heat
and blue for cooling. In dual-setpoint mode, the ring and mode icon follow the
currently active target.

## Component Naming

Widget IDs are intentionally stable across board variants where possible.
Board-specific YAML files provide the native positions, sizes, and fonts.
Shared behavior lives in `thermostat_common.yaml`.

## Future Work

- Add more board variants if needed.
- Add optional per-room climate selection.
- Add richer diagnostics screens for Wi-Fi, API state, and sensor data.
- Add automated screenshot checks for the generated HTML preview.
