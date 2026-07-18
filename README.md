# Thermostat Knob - CrowPanel ESP32-S3 Rotary Displays

[![Open your Home Assistant instance and open a repository inside the Home Assistant Community Store.](https://my.home-assistant.io/badges/hacs_repository.svg)](https://my.home-assistant.io/redirect/hacs_repository/?owner=Jastreb07&repository=elecrow-crowpanel-esphome-thermostat&category=integration)

ESPHome firmware for a Home Assistant thermostat, light, and cover controller
built for Elecrow CrowPanel ESP32-S3 rotary displays. Includes an optional
HACS-installable Home Assistant integration for configuring which entities
show up on the device — see [What the integration
does](#what-the-integration-does).

The project currently supports two board layouts:

- `thermostat_480.yaml` for the CrowPanel 2.1" 480x480 rotary display
- `thermostat_240.yaml` for the CrowPanel 1.28" 240x240 rotary display

The UI is a compact, dark, Nest-like dashboard with a large temperature
readout, a circular setpoint ring, per-entity screens for lights and covers,
Home Assistant integration, touch input, and rotary encoder control.

## Video Walkthrough

| 1.28" (240x240) | 2.1" (480x480) |
|---|---|
| [![240x240 demo](https://img.youtube.com/vi/REPLACE_WITH_240_VIDEO_ID/0.jpg)](https://www.youtube.com/watch?v=REPLACE_WITH_240_VIDEO_ID) | [![480x480 demo](https://img.youtube.com/vi/REPLACE_WITH_480_VIDEO_ID/0.jpg)](https://www.youtube.com/watch?v=REPLACE_WITH_480_VIDEO_ID) |

Replace `REPLACE_WITH_240_VIDEO_ID` and `REPLACE_WITH_480_VIDEO_ID` with the
actual YouTube video IDs once the walkthroughs are recorded.

## Screens

| Loading | Home | Thermostat |
|---|---|---|
| ![Loading screen](images/screen_loading.png) | ![Home screen](images/screen_home.png) | ![Thermostat screen](images/screen_thermostat.png) |

| Light brightness | Light temperature | Light color |
|---|---|---|
| ![Light brightness screen](images/screen_light_brightness.png) | ![Light temperature screen](images/screen_light_temperature.png) | ![Light color screen](images/screen_light_color.png) |

| Cover | Entity navigation |
|---|---|
| ![Cover screen](images/screen_cover.png) | ![Navigation screen](images/screen_navigation.png) |

## Features

- Shows current and target temperature from Home Assistant.
- Changes the target temperature with the rotary encoder.
- Supports HVAC mode and preset changes through Home Assistant services.
- Supports single-setpoint and dual-setpoint climate entities.
- Can control a thermostat-only setup, an AC-only setup, or a combined
  heat/cool climate entity.
- Controls Home Assistant lights: brightness, color temperature, and color,
  with dedicated screens for each mode.
- Controls Home Assistant covers (shutters, blinds, curtains): position,
  stop, and full open/close, including covers without position support.
- A shared entity overview screen to jump between any configured
  thermostat, light, or cover.
- Shows Home Assistant connection state on the display.
- Provides a local HTML preview for the LVGL screens.

The firmware calls Home Assistant services such as `climate.set_temperature`,
`climate.set_hvac_mode`, `climate.set_preset_mode`, `light.turn_on`,
`cover.set_cover_position`, and `cover.open_cover`/`close_cover`/`stop_cover`
through the native ESPHome API. No extra Home Assistant automation YAML is
required for the basic control flow.

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

The device itself does not know which climate, light, or cover entities to
show. It reads that list from one Home Assistant sensor attribute
(`sensor.smart_knob_config` by default), formatted as JSON. There are two
compatible ways to provide it:

1. **Custom integration (recommended)** — a config-flow UI that builds and
   maintains that sensor for you. See [What the integration
   does](#what-the-integration-does) below.
2. **Manual Template-Entity configuration** — hand-write the sensor and its
   JSON attributes yourself. See the example below.

Both produce the same `sensor.smart_knob_config` interface. Do not enable both
with the same entity ID at the same time.

### What the integration does

The **Smart Thermostat Knob** custom integration (`custom_components/smart_thermostat_knob`)
replaces manually written Template-Entity YAML with a normal Home Assistant
config flow. It exists because the device's page list has to live somewhere
Home Assistant can maintain, and picking entities through **Settings > Devices
& services > Add integration** is far less error-prone than typing entity IDs
and JSON by hand.

![Home Assistant entity picker used by the Smart Thermostat Knob integration](images/ha-integration-helper.png)

With it installed you can, per physical Smart Knob:

- Pick any number of climate, light, and cover entities through Home
  Assistant's native entity selectors (autocomplete, no manual entity IDs or
  JSON editing).
- Give the knob a friendly name; the integration derives a unique config
  sensor entity ID from it (for example `sensor.living_room_knob_config`).
- Point the integration at the device's **Home Assistant Config Entity** text
  entity so it auto-writes the sensor entity ID into the ESP and restarts it,
  no manual copy-pasting of entity IDs between HA and the device.
- **Reconfigure** an existing knob later (rename it or change its entity
  list) without recreating the whole setup, and run multiple independent
  Smart Knobs from one Home Assistant instance, each with its own entry.

It is a configuration helper only — it does not add new entities to Home
Assistant itself, does not talk to the ESP device over the network, and adds
no extra automations. All firmware behavior described elsewhere in this
README (control flow, service calls, debouncing) works the same whether the
sensor was built by this integration or written by hand.

Full installation steps (HACS button and manual copy) and the exact fields
in the config flow are in [`integration/README.md`](integration/README.md).

### Manual Template-Entity example

Create one template sensor in Home Assistant's `configuration.yaml`. Users only
edit `entity_id` and `name` inside the lists:

```yaml
template:
  - sensor:
      - name: "Smart Knob Config"
        unique_id: smart_knob_config
        default_entity_id: sensor.smart_knob_config
        icon: mdi:tune-variant
        state: "ready"
        attributes:
          climates: >-
            {{ "json:" ~ ([
              {
                "entity_id": "climate.wohnzimmer_better_thermostat",
                "name": "Living Room"
              }
            ] | to_json) }}

          lights: >-
            {{ "json:" ~ ([
              {
                "entity_id": "light.licht_wohnzimmer",
                "name": "Licht Wohnzimmer"
              },
              {
                "entity_id": "light.lampe_hinter_der_couch",
                "name": "Lampe hinter der Couch"
              }
            ] | to_json) }}

          covers: >-
            {{ "json:" ~ ([
              {
                "entity_id": "cover.rollladen_wohnzimmer",
                "name": "Rollladen Wohnzimmer"
              },
              {
                "entity_id": "cover.gardine_schlafzimmer",
                "name": "Gardine Schlafzimmer"
              }
            ] | to_json) }}
```

The `json:` prefix deliberately keeps Home Assistant from converting the JSON
back into a native Python-style list before sending it over the ESPHome API.
Every entry adds one logical page. Reload the Template entities after changing
the lists; the ESP updates without recompiling or rebooting.

The ESPHome device exposes a persisted text entity named **Home Assistant
Config Entity**. Its initial value is `sensor.smart_knob_config`. Change it in
Home Assistant if the template sensor has another entity ID; the ESP stores the
selection in flash and restores it after a restart.

Each thermostat page keeps its live state, TRV/AC selection, targets, HVAC
mode, preset, and humidity separate. For outgoing service calls to work,
enable the Home Assistant device option that allows the ESPHome device to
perform Home Assistant actions.

`name` is optional. If omitted, the name line on that thermostat page remains
empty.

The controller reads each entity's `supported_color_modes` attribute. A short
press therefore cycles only through controls supported by that light:
brightness, color temperature, and color.

Cover pages (shutters, blinds, curtains) show position 0–100 % as a
liquid-fill gauge that rises from the bottom of the screen; turning the knob
moves the target position, a short press stops the cover mid-move, and
swiping fully opens or closes it. Covers
that only support open/close/stop (no `set_cover_position`) still work: the
knob nudges an internal position estimate and the direction of the last
nudge decides between `cover.open_cover` and `cover.close_cover`.

## Controls

| Action | Result |
|---|---|
| Rotate encoder | Change the current thermostat, light, cover, or menu value |
| Short press | Switch the TRV/AC target, the light control mode, or stop a moving cover |
| Double click | Jump to thermostat or switch its active target |
| Hold 800 ms on thermostat | Toggle HVAC off/on and restore its last mode |
| Hold 800 ms on a light page | Toggle the selected light on/off |
| Hold 800 ms on a cover page | Jump back to the thermostat |
| Hold 800 ms on the settings page | Go back one menu level |
| Swipe (light/cover pages) | Open settings or the entity overview |
| Swipe (cover page, second axis) | Fully open or close the cover |
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
|-- hacs.json                # HACS metadata for the custom integration
|-- custom_components/
|   `-- smart_thermostat_knob/ # HACS-installable Home Assistant integration
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
|-- images/                  # Screenshots used in this README
|-- integration/
|   `-- README.md            # Custom integration install/usage docs
`-- components/
    |-- cst826/                 # Local CST826 touch component
    |-- ha_climate_controller/  # Variable-length Home Assistant climate list
    |-- ha_light_controller/    # Variable-length Home Assistant light list
    `-- ha_cover_controller/    # Variable-length Home Assistant cover list
```

## Roadmap

Done:

- [x] Thermostat control (single- and dual-setpoint, HVAC mode, presets)
- [x] Light control (brightness, color temperature, color)
- [x] Cover control (position, stop, full open/close)
- [x] Shared entity overview and settings navigation
- [x] Optional custom Home Assistant integration for entity selection
- [x] Local LVGL HTML preview tool

Planned:

- [ ] Fan control screen (speed, oscillate, preset)
- [ ] Media player screen (volume, play/pause, track skip)
- [ ] Per-page idle timeout / screen-saver customization
- [ ] Additional language packs for on-device text

Have a feature request? Open an issue with your use case.

## Development Notes

- The LVGL UI is defined directly in `thermostat_480.yaml` and
  `thermostat_240.yaml`.
- Shared behavior lives in `thermostat_common.yaml`.
- Board-specific hardware configuration stays in the board YAML files.
- Keep new user-facing text and comments in English.
