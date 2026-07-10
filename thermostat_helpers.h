// thermostat_helpers.h
//
// Helper functions for the ESPHome YAML files. They are included through
// `esphome: includes:` and are available inside lambdas. Keep them pure:
// no ESPHome ID access here.

#pragma once
#include <string>
#include <vector>
#include <cctype>

// ------------------------------------------------------------------
// Bootstrap Icons from assets/fonts/bootstrap-icons.woff. Codepoints were
// extracted from bootstrap-icons.json.
// ------------------------------------------------------------------
static const char *const ICON_FIRE = "";          // bi-fire
static const char *const ICON_SNOW = "";          // bi-snow
static const char *const ICON_TOGGLE_OFF = "";    // bi-toggle-off
static const char *const ICON_POWER = "";   // bi-power
static const char *const ICON_THERMO_HIGH = "";   // bi-thermometer-high
static const char *const ICON_DROPLET_HALF = "";  // bi-droplet-half

// Menu icons. Index matches the settings menu item.
static const char *const MENU_ICONS[] = {
    "",  //  0 HVAC mode      bi-thermometer-half
    "",  //  1 Preset         bi-sliders
    "",  //  2 Brightness     bi-brightness-high
    "",  //  3 Clock          bi-clock
    "",  //  4 Language       bi-translate
    "",  //  5 Design         bi-palette
    "",  //  6 Icons          bi-eye
    "",  //  7 Unit           bi-rulers
    "",  //  8 Idle time      bi-hourglass-split
    "",  //  9 Dim after      bi-brightness-low
    "",  // 10 Dim level      bi-percent
    "",  // 11 3s button      bi-stopwatch
    "",  // 12 Knob modes     bi-toggles
    "\uf56b",  // 13 Knob step      bi-sliders
    "",  // 14 WiFi           bi-wifi
    "",  // 15 Firmware       bi-info-circle
    "",  // 16 Reset          bi-arrow-counterclockwise
    "",  // 17 LED             bi-lightbulb
    "",  // 18 LED brightness bi-brightness-high
};
constexpr int SETTINGS_MENU_COUNT = 19;

// Icon per better_thermostat preset.
inline const char *ha_preset_icon(const std::string &p) {
  if (p == "eco") return "";       // bi-tree
  if (p == "comfort") return "";   // bi-cup-hot
  if (p == "home") return "";      // bi-house
  if (p == "away") return "";      // bi-person-walking
  if (p == "sleep") return "";     // bi-moon-stars
  if (p == "boost") return "";     // bi-lightning-charge
  if (p == "activity") return "";  // bi-activity
  return "";                       // bi-dash-circle (none/unknown)
}

// ------------------------------------------------------------------
// Labels are intentionally English-only.
// ------------------------------------------------------------------
inline std::string ha_hvac_mode_label(std::string mode, bool de) {
  (void) de;
  if (mode == "heat") return "Heat";
  if (mode == "cool") return "Cool";
  if (mode == "heat_cool") return "Heat/Cool";
  if (mode == "auto") return "Auto";
  if (mode == "off") return "Off";
  if (mode == "dry") return "Dry";
  if (mode == "fan_only") return "Fan";
  return "-";
}

inline std::string ha_preset_label(const std::string &p, bool de) {
  (void) de;
  if (p == "none") return "No preset";
  if (p == "eco") return "Eco";
  if (p == "comfort") return "Comfort";
  if (p == "home") return "Home";
  if (p == "away") return "Away";
  if (p == "sleep") return "Sleep";
  if (p == "boost") return "Boost";
  if (p == "activity") return "Activity";
  if (p.empty()) return "-";
  return p;
}

inline const char *settings_menu_name(int i, bool de) {
  (void) de;
  static const char *const EN[] = {
      "HVAC Mode",  "Preset", "Brightness", "Clock",     "Language",   "Design",
      "Icons",      "Unit",   "Idle Time",  "Dim After", "Dim Level",  "3s Button",
      "Knob Modes", "Knob Step", "WiFi",    "Firmware",  "Reset",      "LED",
      "LED Brightness"};
  if (i < 0 || i >= SETTINGS_MENU_COUNT) return "-";
  return EN[i];
}

// ------------------------------------------------------------------
// Status LED color per HVAC state on the 240x240 board. Off is white,
// cooling is white-blue, heating is orange. In dual-setpoint mode,
// adjust_target shows which target is active: 0 = TRV/heat, 1 = AC/cool.
// ------------------------------------------------------------------
struct LedColor { float r, g, b; };

inline LedColor hvac_led_color(const std::string &hvac, bool dual_setpoint, int adjust_target) {
  static constexpr LedColor WHITE{1.0f, 1.0f, 1.0f};
  static constexpr LedColor WHITE_BLUE{0.43f, 0.78f, 1.0f};
  static constexpr LedColor ORANGE{1.0f, 0.54f, 0.24f};
  if (hvac == "off") return WHITE;
  if (dual_setpoint) return adjust_target == 1 ? WHITE_BLUE : ORANGE;
  if (hvac == "cool" || hvac == "dry" || hvac == "fan_only") return WHITE_BLUE;
  return ORANGE;
}

// ------------------------------------------------------------------
// Temperature unit: internal values are always Celsius; conversion is display-only.
// ------------------------------------------------------------------
inline float display_temp(float celsius, bool fahrenheit) {
  return fahrenheit ? celsius * 1.8f + 32.0f : celsius;
}

// ------------------------------------------------------------------
// Parse Home Assistant list attributes, e.g. "['heat', 'off']" -> {"heat","off"}.
// ------------------------------------------------------------------
inline std::vector<std::string> ha_parse_list(const std::string &raw) {
  std::vector<std::string> out;
  std::string cur;
  for (char c : raw) {
    if (std::isalnum(static_cast<unsigned char>(c)) || c == '_') {
      cur += c;
    } else if (!cur.empty()) {
      out.push_back(cur);
      cur.clear();
    }
  }
  if (!cur.empty()) out.push_back(cur);
  return out;
}

inline std::vector<std::string> ha_filter_modes(const std::string &raw, bool only_heat_off) {
  auto v = ha_parse_list(raw);
  if (only_heat_off) {
    std::vector<std::string> f;
    for (auto &m : v)
      if (m == "heat" || m == "off") f.push_back(m);
    if (!f.empty()) v = f;
  }
  if (v.empty()) {
    v.push_back("heat");
    v.push_back("off");
  }
  return v;
}

inline std::vector<std::string> ha_filter_presets(const std::string &raw) {
  auto v = ha_parse_list(raw);
  if (v.empty()) v.push_back("none");
  return v;
}
