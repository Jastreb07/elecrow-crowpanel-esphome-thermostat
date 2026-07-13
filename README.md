# Thermostat Knob - CrowPanel ESP32-S3 Rotary Displays

ESPHome firmware for a Home Assistant thermostat controller built for
Elecrow CrowPanel ESP32-S3 rotary displays.

The project currently supports two board layouts:

- `thermostat_480.yaml` for the CrowPanel 2.1" 480x480 rotary display
- `thermostat_240.yaml` for the CrowPanel 1.28" 240x240 rotary display

The UI is a compact, dark, Nest-like thermostat dashboard with a large
temperature readout, a circular setpoint ring, Home Assistant climate
integration, touch input, and rotary encoder control.

## Features

- Shows current and target temperature from Home Assistant.
- Changes the target temperature with the rotary encoder.
- Supports HVAC mode and preset changes through Home Assistant services.
- Supports single-setpoint and dual-setpoint climate entities.
- Can control a thermostat-only setup, an AC-only setup, or a combined
  heat/cool climate entity.
- Shows Home Assistant connection state on the display.
- Provides a local HTML preview for the LVGL screens.

The firmware calls Home Assistant services such as `climate.set_temperature`,
`climate.set_hvac_mode`, and `climate.set_preset_mode` through the native
ESPHome API. No extra Home Assistant automation YAML is required for the
basic thermostat control flow.

## Hardware

- Elecrow CrowPanel 2.1" HMI ESP32-S3 rotary display, 480x480
- Elecrow CrowPanel 1.28" HMI ESP32-S3 rotary display, 240x240
- Built-in rotary encoder with push button
- Touch display

Board-specific display, touch, output, and encoder pins live in
`thermostat_480.yaml` and `thermostat_240.yaml`. Check the pins against the
official Elecrow documentation before flashing a physical board.

## Requirements

- Windows with Python 3.14.6, pip 26.1.2, and ESPHome 2026.6.5
- Home Assistant with the target climate entity
- USB cable for first-time flashing
- Wi-Fi credentials and ESPHome secrets in `secrets.yaml`

Full installation instructions are in [docs/SETUP.md](docs/SETUP.md).

## Quick Start

Open a new PowerShell terminal after installing Python or changing PATH, then
run commands from the project directory:

```powershell
cd $env:USERPROFILE\PhpstormProjects\esp\thermostat
```

Copy the example secrets file and edit the values:

```powershell
Copy-Item secrets.yaml.example secrets.yaml
notepad secrets.yaml
```

Validate the configuration without flashing:

```powershell
esphome config thermostat_480.yaml
esphome config thermostat_240.yaml
```

List available serial ports from PowerShell:

```powershell
[System.IO.Ports.SerialPort]::GetPortNames()
Get-CimInstance Win32_SerialPort | Select-Object DeviceID,Name,Description
```

Compile and flash over USB. Replace `COM5` and `COM6` with the actual serial ports:

```powershell
esphome run thermostat_240.yaml --device COM5
esphome run thermostat_480.yaml --device COM6
```

OR Compile and flash over IP/Network. Replace `192.168.178.138` and `192.168.178.137` with the actual IP addresses:

```powershell
esphome run thermostat_240.yaml --device 192.168.178.138
esphome run thermostat_480.yaml --device 192.168.178.137
```

After the first USB flash, ESPHome can usually flash over OTA when the device
is reachable on the network.

If an old terminal does not find `esphome`, open a new terminal or call the
installed executable directly:

```powershell
& "$env:USERPROFILE\AppData\Local\Programs\Python\Python314\Scripts\esphome.exe" version
```

## LVGL Preview

`tools\lvgl_preview.py` generates a local HTML preview of the LVGL screens.
It loads `thermostat_480.yaml`, `thermostat_240.yaml`, and
`thermostat_common.yaml`, then renders a fast approximation of both board
layouts. It is useful for checking positions, sizes, labels, and spacing
before compiling firmware.

Start the live watcher:

```powershell
python tools\lvgl_preview.py
```

The preview opens at `http://localhost:8123/tools/preview.html`. YAML changes
are detected automatically and the browser reloads. Stop the watcher with
`Ctrl+C`.

Generate the HTML file once:

```powershell
python tools\lvgl_preview.py --once
```

## Home Assistant Setup

The ESPHome integration in Home Assistant should discover the device after it
joins Wi-Fi. Home Assistant asks for the API encryption key if one is defined
in `secrets.yaml`.

Thermostat pages are configured as an array in `thermostat_common.yaml`:

```yaml
ha_climate_controller:
  id: ha_climates
  climates:
    - entity_id: "climate.wohnzimmer_better_thermostat"
      name: "Living Room"
    - entity_id: "climate.schlafzimmer"
      name: "Bedroom"
```

Every entry adds one logical thermostat page. Each page keeps its live state,
TRV/AC selection, targets, HVAC mode, preset, and humidity separate. For
outgoing service calls to work, enable the Home Assistant device option that
allows the ESPHome device to perform Home Assistant actions.

`name` is optional. If omitted, the name line on that thermostat page remains
empty.

Light pages are configured as an array in `thermostat_common.yaml`. Every
entry adds one logical page to the horizontal swipe order:

```yaml
ha_light_controller:
  id: ha_lights
  lights:
    - entity_id: "light.lampe_hinter_der_couch"
      name: "Light"
    - entity_id: "light.stehlampe"
      name: "Floor Light"
```

The controller reads each entity's `supported_color_modes` attribute. A short
press therefore cycles only through controls supported by that light:
brightness, color temperature, and color.

## Controls

| Action | Result |
|---|---|
| Rotate encoder | Change the current thermostat, light, or menu value |
| Short press | Switch the TRV/AC target or the available light control mode |
| Double click | Jump to thermostat or switch its active target |
| Hold 800 ms on thermostat | Toggle HVAC off/on and restore its last mode |
| Hold 800 ms on a light page | Toggle the selected light on/off |
| Hold 800 ms on the settings page | Go back one menu level |
| Horizontal swipe | Thermostat pages → light pages → settings, or back |
| Touch the display | Wake the display |

Temperature changes are debounced. The UI updates immediately, but
`climate.set_temperature` is sent only after the configured quiet period.

## Project Layout

```text
thermostat/
|-- thermostat_480.yaml      # ESPHome entry point for the 2.1" 480x480 board
|-- thermostat_240.yaml      # ESPHome entry point for the 1.28" 240x240 board
|-- thermostat_common.yaml   # Shared logic, Home Assistant integration, scripts
|-- thermostat_helpers.h     # C++ helper functions for labels and icons
|-- secrets.yaml.example     # Template for Wi-Fi/API/OTA secrets
|-- README.md                # Project overview
|-- docs/
|   |-- README.md            # Documentation index
|   |-- UI_CONCEPT.md        # Current UI model and interaction notes
|   |-- SETUP.md             # Python, pip, ESPHome, and firmware setup
|   `-- vorstelleng.md       # Original project brief in English
|-- tools/
|   |-- lvgl_preview.py      # Local HTML preview generator
|   |-- preview.html         # Generated preview
|   `-- preview_version.txt  # Reload marker for the preview watcher
|-- assets/
|   |-- icons/
|   `-- fonts/
`-- components/
    |-- cst826/              # Local CST826 touch component
    `-- ha_light_controller/ # Variable-length Home Assistant light list
```

## Development Notes

- The LVGL UI is defined directly in `thermostat_480.yaml` and
  `thermostat_240.yaml`.
- Shared behavior lives in `thermostat_common.yaml`.
- Board-specific hardware configuration stays in the board YAML files.
- Keep new user-facing text and comments in English.
