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
// Material Design Icons (materialdesignicons-webfont.ttf, per URL in den
// YAML font:-Bloecken geladen). Codepoints: https://pictogrammers.com/library/mdi/
// ------------------------------------------------------------------
static const char *const ICON_FIRE = "󰈸";          // mdi-fire
static const char *const ICON_SNOW = "󰜗";          // mdi-snowflake
static const char *const ICON_TOGGLE_OFF = "󰔢";    // mdi-toggle-switch-off
static const char *const ICON_POWER = "󰐥";         // mdi-power
static const char *const ICON_THERMO_HIGH = "󰔏";   // mdi-thermometer
static const char *const ICON_DROPLET_HALF = "󰖎";  // mdi-water-percent

// Menu icons. Index matches the settings menu item.
static const char *const MENU_ICONS[] = {
    "󰎓",  //  0 HVAC mode      mdi-thermostat
    "󰘮",  //  1 Preset         mdi-tune
    "󰃞",  //  2 Brightness     mdi-brightness-6
    "󰅐",  //  3 Clock          mdi-clock-outline
    "󰗊",  //  4 Language       mdi-translate
    "󰏘",  //  5 Design         mdi-palette
    "󰈈",  //  6 Icons          mdi-eye
    "󰔄",  //  7 Unit           mdi-temperature-celsius
    "󰔟",  //  8 Idle time      mdi-timer-sand
    "󰃜",  //  9 Dim after      mdi-brightness-4
    "󰏰",  // 10 Dim level      mdi-percent
    "󰇣",  // 11 3s button      mdi-av-timer
    "󰔡",  // 12 Knob modes     mdi-toggle-switch
    "󰑧",  // 13 Knob step      mdi-rotate-right
    "󰖩",  // 14 WiFi           mdi-wifi
    "󰋽",  // 15 Firmware       mdi-information-outline
    "󰜉",  // 16 Reset          mdi-restart
    "󰌵",  // 17 LED            mdi-lightbulb
    "󰃟",  // 18 LED brightness mdi-brightness-7
};
constexpr int SETTINGS_MENU_COUNT = 19;

// Icon per better_thermostat preset.
inline const char *ha_preset_icon(const std::string &p) {
  if (p == "eco") return "󰌪";       // mdi-leaf
  if (p == "comfort") return "󰅶";   // mdi-coffee
  if (p == "home") return "󰋜";      // mdi-home
  if (p == "away") return "󰖃";      // mdi-walk
  if (p == "sleep") return "󰖔";     // mdi-weather-night
  if (p == "boost") return "󰉁";     // mdi-flash
  if (p == "activity") return "󰐰";  // mdi-pulse
  return "󰍶";                       // mdi-minus-circle (none/unknown)
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
