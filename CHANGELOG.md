# Changelog

All notable changes to the Smart Thermostat Knob firmware are documented
here. The version headings below match `firmware/version.txt` and the
GitHub Release tags (`v<version>`) - the release workflow extracts the
section for the version being released as the release notes body.

## 1.0.3

- Fixed the web flasher's "Connect and Install" dialog showing the version
  twice ("... v1.0.2 1.0.2?") - the release name no longer duplicates the
  version that the dialog already appends on its own.

## 1.0.2

- Fixed the release workflow not actually applying the firmware version to
  compiled builds (`esphome/build-action` has no `substitutions` input, so
  the override was silently ignored) - compiled firmware and both release
  manifests were still reporting `0.0.0` instead of the real version.
- The web flasher's "Connect and Install" dialog now shows a readable
  release name instead of the raw internal project id.

## 1.0.1

- Fixed the update entity reporting the ESPHome core version instead of the
  actual firmware version, which made "Update available" reappear right
  after a successful install instead of clearing.
- The update-available notification now shows a "Press = show update"
  subtitle, and its LED lights up steady instead of blinking.
- Settings > System > Update gains an "Ignore" choice: dismissing a
  version persists across reboots and silences the notification/LED for
  that release until a newer one is published.

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
