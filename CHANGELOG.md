# Changelog

All notable changes to the Smart Thermostat Knob firmware are documented
here. The version headings below match `firmware/version.txt` and the
GitHub Release tags (`v<version>`) - the release workflow extracts the
section for the version being released as the release notes body.

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
