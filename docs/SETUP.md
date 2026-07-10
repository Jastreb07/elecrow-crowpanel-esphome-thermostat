# Setup

This guide documents the toolchain used for this project on Windows.

## Versions

The current local setup uses:

- Python 3.14.6
- pip 26.1.2
- ESPHome 2026.6.5

ESPHome 2026.6.5 requires Python `>=3.11,<3.15`, so Python 3.14.6 is a valid
runtime for this project.

## Install Python

Install Python 3.14 for the current Windows user:

```powershell
winget install --id Python.Python.3.14 --exact --scope user --accept-package-agreements --accept-source-agreements
```

Open a new PowerShell terminal after the installer finishes.

Verify Python:

```powershell
python --version
```

Expected result:

```text
Python 3.14.6
```

If `python` is not found in the current terminal, use the full path:

```powershell
& "$env:USERPROFILE\AppData\Local\Programs\Python\Python314\python.exe" --version
```

## Verify pip

pip is installed with the Python.org Windows installer.

```powershell
pip --version
```

Expected result:

```text
pip 26.1.2
```

If `pip` is not found, use Python directly:

```powershell
python -m pip --version
```

## Install ESPHome

Install or upgrade ESPHome globally in the Python 3.14 user installation:

```powershell
python -m pip install --upgrade esphome
```

Verify ESPHome:

```powershell
esphome version
```

Expected result:

```text
Version: 2026.6.5
```

If `esphome` is not found, open a new terminal or call the executable directly:

```powershell
& "$env:USERPROFILE\AppData\Local\Programs\Python\Python314\Scripts\esphome.exe" version
```

## PATH Entries

The user PATH should include these directories:

```text
%USERPROFILE%\AppData\Local\Programs\Python\Python314
%USERPROFILE%\AppData\Local\Programs\Python\Python314\Scripts
```

New terminals pick up PATH changes automatically. Already-open terminals may
need to be restarted.

## Project Setup

Open the project directory:

```powershell
cd $env:USERPROFILE\PhpstormProjects\esp\thermostat
```

Create the local secrets file:

```powershell
Copy-Item secrets.yaml.example secrets.yaml
notepad secrets.yaml
```

Fill in Wi-Fi credentials, OTA password, and API settings.

## Validate ESPHome YAML

Validate the 480x480 board:

```powershell
esphome config thermostat_480.yaml
```

Validate the 240x240 board:

```powershell
esphome config thermostat_240.yaml
```

`config` checks YAML syntax, resolves packages/includes, validates ESPHome
components, and reports warnings without flashing a device.

## Flash Firmware

Connect the board over USB and check the COM port. Windows Device Manager shows
the port under "Ports (COM & LPT)", but you can also check it from PowerShell.

List only the available COM port names:

```powershell
[System.IO.Ports.SerialPort]::GetPortNames()
```

Show more detail, including USB serial adapter names when Windows exposes them:

```powershell
Get-CimInstance Win32_SerialPort | Select-Object DeviceID,Name,Description
```

If the board is already connected, unplug it, run one of the commands, plug the
board back in, and run it again. The newly appearing port is the one to use.

Then run the matching board command:

```powershell
esphome run thermostat_480.yaml --device COM30
```

or:

```powershell
esphome run thermostat_240.yaml --device COM30
```

Replace `COM30` with the actual serial port.

After the first successful USB flash, ESPHome can usually use OTA updates when
the device is on Wi-Fi:

```powershell
esphome run thermostat_480.yaml
```

## Run the LVGL Preview

Start the live HTML preview:

```powershell
python tools\lvgl_preview.py
```

Open:

```text
http://localhost:8123/tools/preview.html
```

Stop the watcher with `Ctrl+C`.

Generate the preview once without running the watcher:

```powershell
python tools\lvgl_preview.py --once
```
