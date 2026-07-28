# Changelog

All notable changes to the Smart Thermostat Knob firmware are documented
here. The version headings below match `firmware/version.txt` and the
GitHub Release tags (`v<version>`) - the release workflow extracts the
section for the version being released as the release notes body.

## 1.0.3

- Replaced the frozen Progress screen during OTA updates with the Notify
  screen showing "Bitte warten" / "Update läuft..." and a steady yellow
  LED - the download still blocks the display for its duration (an
  ESPHome core limitation, not fixable from here), but the screen now
  reliably shows this message before that happens instead of sometimes
  freezing mid-transition.

## 1.0.2

No functional changes - version bump to test the on-device OTA
self-update flow (Settings > System > Update) end-to-end against a real
release.

## 1.0.1

- Fixed the update entity reporting the ESPHome core version instead of the
  actual firmware version, which made "Update available" reappear right
  after a successful install instead of clearing. Compiled builds and both
  release manifests now consistently carry the real firmware version.
- The update-available notification now shows a "Press = show update"
  subtitle, and its LED lights up steady instead of blinking.
- Settings > System > Update gains an "Ignore" choice: dismissing a
  version persists across reboots and silences the notification/LED for
  that release until a newer one is published.
- Fixed Wi-Fi credentials set via Improv (Setup Wizard / web flasher) not
  surviving firmware updates - ESPHome keys the saved-credentials flash slot
  by the compiled config's version hash whenever a static `ssid:`/`password:`
  is configured, and that hash changes with every release, so each update
  looked up an empty slot and silently fell back to the placeholder network.
  Removed the static network from `wifi:` so the lookup key stays constant
  across builds. **If you're updating from 1.0.0, re-enter Wi-Fi once more
  after this update** (the key changes one final time); it then survives
  all future updates.
- The web flasher's "Connect and Install" dialog now shows a readable
  release name instead of the raw internal project id, and no longer shows
  the version twice.
- The Improv "Connected to ..." popup (shown for an already-flashed device)
  now shows a readable project name ("Vexur-Software.Smart Thermostat
  Knob") instead of the raw internal project id.

## 1.0.0

Initial public release.

- Rotary-knob smart thermostat UI for the Elecrow CrowPanel ESP32-S3 boards
  (1.28" 240x240 and 2.1" 480x480), synced with Home Assistant climates,
  lights and covers via the companion HACS integration.
- First-boot Setup Wizard covering Wi-Fi, language and Home Assistant
  integration setup, reachable again anytime from Settings > System.
- Multi-language UI (English/German) via the on-device i18n component.
- Notification, progress and timer queues with configurable LED feedback.
- Self-service OTA updates: the device checks GitHub Releases automatically,
  shows available updates as a high-priority notification, and installs
  them from Settings > System > Update.
- Browser-based flashing tool (web-flasher) for installing the firmware
  without any local toolchain.
