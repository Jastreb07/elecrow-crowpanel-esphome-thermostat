# Setup

Technical reference for building, flashing, and configuring the Smart
Thermostat Knob. For what the project is and what it looks like, see
[README.md](README.md).

## Requirements

- Windows with Python 3.13+ and ESPHome 2026.6.5
- Home Assistant with the target climate/light/cover entities
- USB cable for the first flash
- Wi-Fi credentials and ESPHome secrets in `secrets.yaml`

## Toolchain Install

The current local setup uses:

- Python 3.13.14
- pip 26.1.2
- ESPHome 2026.6.5

ESPHome 2026.6.5 requires Python `>=3.11,<3.15`, so Python 3.13.14 is a valid
runtime for this project.

### Install Python

Install Python 3.13 for the current Windows user:

```powershell
winget install --id Python.Python.3.13 --exact --scope user --accept-package-agreements --accept-source-agreements
```

Open a new PowerShell terminal after the installer finishes, then verify:

```powershell
python --version
```

Expected result:

```text
Python 3.13.14
```

If `python` is not found in the current terminal, use the full path:

```powershell
& "$env:USERPROFILE\AppData\Local\Programs\Python\Python313\python.exe" --version
```

### Verify pip

pip is installed with the Python.org Windows installer.

```powershell
pip --version
```

If `pip` is not found, use Python directly:

```powershell
python -m pip --version
```

### Install ESPHome

Install or upgrade ESPHome globally in the Python 3.13 user installation:

```powershell
python -m pip install --upgrade esphome
```

Verify:

```powershell
esphome version
```

If `esphome` is not found, open a new terminal or call the executable
directly:

```powershell
& "$env:USERPROFILE\AppData\Local\Programs\Python\Python313\Scripts\esphome.exe" version
```

### PATH Entries

The user PATH should include these directories:

```text
%USERPROFILE%\AppData\Local\Programs\Python\Python313
%USERPROFILE%\AppData\Local\Programs\Python\Python313\Scripts
```

New terminals pick up PATH changes automatically. Already-open terminals may
need to be restarted.

## Quick Start

Open a new PowerShell terminal after installing Python or changing PATH, then
run commands from the project directory:

```powershell
cd $env:USERPROFILE\PhpstormProjects\esp\thermostat
```

Copy the example secrets file and edit the values (Wi-Fi credentials, OTA
password, fallback AP password):

```powershell
Copy-Item secrets.yaml.example secrets.yaml
notepad secrets.yaml
```

Validate the configuration without flashing:

```powershell
esphome config thermostat_480.yaml
esphome config thermostat_240.yaml
```

`config` checks YAML syntax, resolves packages/includes, validates ESPHome
components, and reports warnings without touching a device.

## Flash Firmware

List available serial ports from PowerShell:

```powershell
[System.IO.Ports.SerialPort]::GetPortNames()
Get-CimInstance Win32_SerialPort | Select-Object DeviceID,Name,Description
```

If the board is already connected, unplug it, run one of the commands, plug
the board back in, and run it again. The newly appearing port is the one to
use.

Compile and flash over USB. Replace `COM5` and `COM6` with the actual serial
ports:

```powershell
esphome run thermostat_240.yaml --device COM5
esphome run thermostat_480.yaml --device COM6
```

OR compile and flash over IP/Network. Replace `192.168.178.138` and
`192.168.178.137` with the actual IP addresses:

```powershell
esphome run thermostat_240.yaml --device 192.168.178.138
esphome run thermostat_480.yaml --device 192.168.178.137
```

After the first USB flash, ESPHome can usually flash over OTA when the
device is reachable on the network.

Prefer flashing from a browser instead of installing ESPHome? See
[Web Flasher](#web-flasher) below.

## Building Standalone Firmware Binaries

`esphome run` compiles and flashes in one step. To build a `.bin` file
without flashing (for archiving a release, flashing with a separate tool, or
copying to another machine), compile only:

```powershell
esphome compile thermostat_240.yaml
esphome compile thermostat_480.yaml
```

`esphome compile` writes to ESPHome's default, hidden cache directory
(`esphome.build_path` is not honored by `esphome compile` in the ESPHome
version this project uses, so don't rely on overriding it):

```text
.esphome/build/thermostat240/.pioenvs/thermostat240/
.esphome/build/thermostat480/.pioenvs/thermostat480/
```

That folder also contains `firmware.factory.bin` — ESPHome/PlatformIO's
already-merged image (bootloader + partition table + OTA marker + app, at
their correct flash offsets, combined into one file you can flash starting
at offset `0`). That's the file the Web Flasher uses; see below.

## Web Flasher

[smart-knob.vexur-software.com](https://smart-knob.vexur-software.com)
flashes either board straight from the browser (Chrome/Edge over USB, no
ESPHome install needed), using
[ESP Web Tools](https://esphome.github.io/esp-web-tools/). After flashing it
walks through Wi-Fi setup with Improv, the same "pick a network, type the
password" step other ESP Web Tools installers use.

The page lives in [`web-flasher/`](web-flasher). Its manifests
(`manifest_240.json`, `manifest_480.json`) fetch `firmware.factory.bin`
straight from `raw.githubusercontent.com`, not from wherever the page
itself is hosted — pushing it to `master` is the only "deploy" step firmware
needs.

```powershell
.\build.ps1
git add web-flasher/firmware
git commit -m "chore(web-flasher): update firmware binaries"
git push
```

`build.ps1` compiles both boards and copies each board's
`firmware.factory.bin` into `web-flasher/firmware/<board>/`. The page
itself (`index.html` + manifests) is hosted on our own web space, not
GitHub Pages — see [`web-flasher/README.md`](web-flasher/README.md) for
the upload steps.

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

The preview opens at `http://localhost:8123/tools/preview.html`. YAML
changes are detected automatically and the browser reloads. Stop the
watcher with `Ctrl+C`.

Generate the HTML file once:

```powershell
python tools\lvgl_preview.py --once
```

## Home Assistant Setup

### Adding the device

Once the ESP has joined Wi-Fi, Home Assistant's built-in **ESPHome**
integration should discover it automatically and show a notification. If
it doesn't, add it manually:

1. Go to **Settings > Devices & services > Add integration**, search for
   **ESPHome**, and select it.
2. Enter the device's hostname or IP address (for example
   `thermostat480.local` or `192.168.178.137`) and confirm.
3. If `api.encryption.key` is set in `secrets.yaml`, Home Assistant asks
   for that key on this step — paste it in.
4. Home Assistant creates one device with the sensors, buttons, and other
   entities the firmware exposes (Wi-Fi signal, backlight, restart button,
   etc.).

This is separate from the **Smart Thermostat Knob** custom integration
below, which only configures *which* climate/light/cover entities show up
as pages on the device — add the ESP itself first.

The device itself does not know which climate, light, or cover entities to
show. It reads that list from one Home Assistant sensor attribute
(`sensor.smart_knob_config` by default), formatted as JSON. There are two
compatible ways to provide it:

1. **Custom integration (recommended)** — a config-flow UI that builds and
   maintains that sensor for you. See [What the integration does](README.md#what-the-integration-does)
   in the README for what it's for, then follow the
   [integration repo's README](https://github.com/Jastreb07/elecrow-crowpanel-esphome-thermostat-integration#readme)
   for HACS/manual install steps and the exact config-flow fields. It's a
   separate repository from this firmware, installed and updated
   independently.
2. **Manual Template-Entity configuration** — hand-write the sensor and its
   JSON attributes yourself. See the example below.

Both produce the same `sensor.smart_knob_config` interface. Do not enable
both with the same entity ID at the same time.

### Manual Template-Entity example

Create one template sensor in Home Assistant's `configuration.yaml`. Users
only edit `entity_id` and `name` inside the lists:

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

The `json:` prefix deliberately keeps Home Assistant from converting the
JSON back into a native Python-style list before sending it over the
ESPHome API. Every entry adds one logical page. Reload the Template entities
after changing the lists; the ESP updates without recompiling or rebooting.

The ESPHome device exposes a persisted text entity named **Home Assistant
Config Entity**. Its initial value is `sensor.smart_knob_config`. Change it
in Home Assistant if the template sensor has another entity ID; the ESP
stores the selection in flash and restores it after a restart.

Each thermostat page keeps its live state, TRV/AC selection, targets, HVAC
mode, preset, and humidity separate. For outgoing service calls to work,
enable the Home Assistant device option that allows the ESPHome device to
perform Home Assistant actions.

`name` is optional. If omitted, the name line on that thermostat page
remains empty.

The controller reads each entity's `supported_color_modes` attribute. A
short press therefore cycles only through controls supported by that light:
brightness, color temperature, and color.

Cover pages (shutters, blinds, curtains) show position 0–100 % as a
liquid-fill gauge that rises from the bottom of the screen; turning the
knob moves the target position, a short press stops the cover mid-move, and
swiping fully opens or closes it. Covers that only support open/close/stop
(no `set_cover_position`) still work: the knob nudges an internal position
estimate and the direction of the last nudge decides between
`cover.open_cover` and `cover.close_cover`.

## Timer & Progress

Unlike the thermostat/light/cover pages, Timer and Progress are **not**
configured through `sensor.smart_knob_config` or the custom integration —
each device has exactly one of each, backed by plain entities the built-in
ESPHome integration already exposes (see [Adding the
device](#adding-the-device)). Set them from an automation, script, or
dashboard like any other `text`/`number`/`button` entity:

| Entity | Purpose |
|---|---|
| `text.*_timer_end_time` | Target end time, either `YYYY-MM-DD HH:MM:SS` or just `HH:MM:SS` (today's date). Empty = no timer set. Writing a new value always restarts the countdown from "now" — there's no separate start-time field. |
| `text.*_timer_title` | Optional label shown under the countdown. Defaults to "Timer"; empty hides it. |
| `number.*_progress` | Progress percentage, 0–100. |
| `text.*_progress_title` | Optional label shown under the percentage. Defaults to "Progress"; empty hides it. |
| `button.*_show_home_screen` | Jumps the device back to the Home screen. |
| `button.*_show_timer_screen` | Jumps the device to the Timer screen. |
| `button.*_show_progress_screen` | Jumps the device to the Progress screen. |

Example automation setting a 20-minute timer and showing it immediately:

```yaml
service: text.set_value
target:
  entity_id: text.thermostat480_timer_end_time
data:
  value: "{{ (now() + timedelta(minutes=20)).strftime('%Y-%m-%d %H:%M:%S') }}"
---
service: button.press
target:
  entity_id: button.thermostat480_show_timer_screen
```

Both screens are excluded from the idle-to-home timeout, so once shown they
stay up until you swipe away manually. They also appear as extra rows in
the on-device entity overview menu, but only while actually "running" — the
timer while its end time is still in the future, progress while its value
is strictly between 0 and 100 — so a finished timer or a 0%/100% progress
value quietly drops out of the menu again.

### Controlling the Timer with the knob

On the Timer screen itself, the rotary encoder edits the end time directly —
no Home Assistant round-trip needed:

- **Turning the knob** moves the end time by one **Timer Knob Step** (see
  below, default 10s) per detent. If no timer is set yet, the first turn
  starts one from scratch (`now + one step`); if a timer is already running,
  turning extends or shortens the remaining time.
- **A short knob press** shows a "Cancel timer?" confirmation overlay. A
  second press within 5 seconds cancels the timer — this clears both the
  end time and the title. Otherwise the overlay disappears on its own after
  5 seconds and the timer keeps running untouched.

The Progress screen doesn't have knob controls — its value only comes from
`number.*_progress`.

### What happens when a timer/progress finishes

Configurable on the device itself, under **Settings > Smart Knob**, one pair
of settings each for Timer and Progress (same settings menu the existing
Idle Time/Dim/LED options already live in — turn the knob to open **Settings
> Smart Knob**, press to enter one of the four items below):

| Setting | Behavior |
|---|---|
| Timer Auto-Home | Seconds after the countdown reaches zero before the device jumps back to the Home screen on its own. `0` = never (stays on the finished Timer screen until you swipe away). Only fires while the Timer screen is still showing. |
| Timer Finish Blink | When on, the Timer screen's background flashes white/green (every 500 ms) once finished, forcing full display brightness for as long as it blinks. Off by default. |
| Progress Auto-Home | Same as Timer Auto-Home, but for the Progress screen reaching 100%. |
| Progress Finish Blink | Same as Timer Finish Blink, but for Progress reaching 100%. |
| Timer Knob Step | Seconds the rotary encoder moves the Timer's end time per detent (see [Controlling the Timer with the knob](#controlling-the-timer-with-the-knob)). Default `10`, range 5-60s. |

All five are also plain Home Assistant entities if you'd rather set them
from there: `number.*_timer_auto_home_after_s`, `switch.*_timer_finish_blink`,
`number.*_progress_auto_home_after_s`, `switch.*_progress_finish_blink`,
`number.*_timer_knob_step_s`.

## Project Layout

```text
thermostat/
|-- thermostat_480.yaml      # ESPHome entry point for the 2.1" 480x480 board
|-- thermostat_240.yaml      # ESPHome entry point for the 1.28" 240x240 board
|-- thermostat_common.yaml   # Shared logic, Home Assistant integration, scripts
|-- thermostat_helpers.h     # C++ helper functions for labels and icons
|-- secrets.yaml.example     # Template for Wi-Fi/API/OTA secrets
|-- README.md                # Product overview
|-- SETUP.md                 # This file: build, flash, and configure
|-- build.ps1                # Compiles both boards, exports firmware.factory.bin to web-flasher/firmware/
|-- .esphome/                # esphome compile output cache (git-ignored)
|-- custom_components/
|   `-- smart_thermostat_knob/ # Git-ignored: own repo (elecrow-crowpanel-esphome-thermostat-integration),
|                             # nested here only for local side-by-side development
|-- docs/
|   |-- README.md            # Documentation index
|   `-- UI_CONCEPT.md        # Current UI model and interaction notes
|-- tools/
|   |-- lvgl_preview.py      # Local HTML preview generator
|   |-- preview.html         # Generated preview
|   `-- preview_version.txt  # Reload marker for the preview watcher
|-- assets/
|   |-- icons/
|   `-- fonts/
|-- images/                  # Screenshots used in the README
|-- 3D Print/                # Printable knob/enclosure STL files, per board
|-- integration/
|   `-- README.md            # Pointer to the integration's own repository
|-- web-flasher/             # Browser-based flasher (ESP Web Tools), hosted on our own web space
|   |-- index.html
|   |-- manifest_240.json
|   |-- manifest_480.json
|   `-- firmware/            # Flashable .bin parts per board (see web-flasher/README.md)
`-- components/
    |-- cst826/                 # Local CST826 touch component
    |-- ha_climate_controller/  # Variable-length Home Assistant climate list
    |-- ha_light_controller/    # Variable-length Home Assistant light list
    `-- ha_cover_controller/    # Variable-length Home Assistant cover list
```

## Development Notes

- The LVGL UI is defined directly in `thermostat_480.yaml` and
  `thermostat_240.yaml`.
- Shared behavior lives in `thermostat_common.yaml`.
- Board-specific hardware configuration stays in the board YAML files.
- Keep new user-facing text and comments in English.

### Branching

Day-to-day work happens on **`dev`**, not `master`. `master` only receives
commits when `dev` is merged into it on purpose (a manual merge, done by
the maintainer):

```powershell
git checkout dev
# ...commit, push to origin/dev as usual...

# When dev is ready to ship:
git checkout master
git merge dev
git push origin master
git checkout dev
```
