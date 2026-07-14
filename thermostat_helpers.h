// thermostat_helpers.h
//
// Helper functions for the ESPHome YAML files. They are included through
// `esphome: includes:` and are available inside lambdas. Keep them pure:
// no ESPHome ID access here.

#pragma once
#include <string>
#include <vector>
#include <cctype>

// Return live Home Assistant text once the configuration and entity state are
// ready. Until then, callers choose the exact placeholder width per label.
inline std::string ha_text_or_placeholder(bool ready, const std::string &text,
                                          size_t placeholder_count) {
  return ready ? text : std::string(placeholder_count, '-');
}

// Keep connection states as stable English keys internally and translate only
// their presentation. Error comparisons and actions therefore stay language
// independent.
inline std::string ha_connection_label(const std::string &status, bool de) {
  if (!de) return status;
  if (status == "WiFi connecting...") return "WLAN wird verbunden...";
  if (status == "WiFi error") return "WLAN-Fehler";
  if (status == "HA connecting...") return "Home Assistant wird verbunden...";
  if (status == "HA unreachable") return "Home Assistant nicht erreichbar";
  if (status == "Smart Knob config loading...") return "Smart-Knob-Konfiguration wird geladen...";
  if (status == "Smart Knob config not found") return "Smart-Knob-Konfiguration nicht gefunden";
  if (status == "Smart Knob config JSON error") return "Smart-Knob-Konfiguration: JSON-Fehler";
  if (status == "No climate configured") return "Kein Thermostat konfiguriert";
  if (status == "Climate entity loading...") return "Thermostat-Entität wird geladen...";
  if (status == "Climate entity not found") return "Thermostat-Entität nicht gefunden";
  return status;
}

inline std::string reboot_hint_label(bool de) {
  return de ? "Knob drücken oder Display berühren zum Neustart"
            : "Press knob or touch display to reboot";
}

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
static const char *const ICON_BACK = "󰁍";          // mdi-arrow-left

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
    "",   // 11 removed/reserved
    "󰔡",  // 12 Knob modes     mdi-toggle-switch
    "󰑧",  // 13 Knob step      mdi-rotate-right
    "󰖩",  // 14 WiFi           mdi-wifi
    "󰋽",  // 15 Firmware       mdi-information-outline
    "󰜉",  // 16 Reset          mdi-restart
    "󰌵",  // 17 LED            mdi-lightbulb
    "󰃟",  // 18 LED brightness mdi-brightness-7
};
constexpr int SETTINGS_MENU_COUNT = 19;

// Root groups for the hierarchical settings screen. The values below are the
// stable setting IDs used by the existing editor/apply logic.
constexpr int SETTINGS_GROUP_COUNT = 3;
constexpr int SETTINGS_ROOT_COUNT = SETTINGS_GROUP_COUNT + 1;  // + Back
static const char *const SETTINGS_GROUP_NAMES[SETTINGS_GROUP_COUNT] = {
    "Thermostat", "Smart Knob", "System"};
static const char *const SETTINGS_GROUP_ICONS[SETTINGS_GROUP_COUNT] = {
    MENU_ICONS[0], MENU_ICONS[12], MENU_ICONS[14]};

static constexpr int SETTINGS_GROUP_THERMOSTAT[] = {0, 1, 7};
static constexpr int SETTINGS_GROUP_SMART_KNOB[] = {
    12, 13, 5, 6, 2, 3, 4, 8, 9, 10, 17, 18};
static constexpr int SETTINGS_GROUP_SYSTEM[] = {14, 15, 16};

inline int settings_group_count(int group) {
  switch (group) {
    case 0: return sizeof(SETTINGS_GROUP_THERMOSTAT) / sizeof(SETTINGS_GROUP_THERMOSTAT[0]);
    case 1: return sizeof(SETTINGS_GROUP_SMART_KNOB) / sizeof(SETTINGS_GROUP_SMART_KNOB[0]);
    case 2: return sizeof(SETTINGS_GROUP_SYSTEM) / sizeof(SETTINGS_GROUP_SYSTEM[0]);
    default: return 0;
  }
}

inline int settings_group_type_at(int group, int position) {
  int count = settings_group_count(group);
  if (count <= 0) return 0;
  position = ((position % count) + count) % count;
  switch (group) {
    case 0: return SETTINGS_GROUP_THERMOSTAT[position];
    case 1: return SETTINGS_GROUP_SMART_KNOB[position];
    case 2: return SETTINGS_GROUP_SYSTEM[position];
    default: return 0;
  }
}

inline int settings_group_wrap(int group) {
  return ((group % SETTINGS_GROUP_COUNT) + SETTINGS_GROUP_COUNT) % SETTINGS_GROUP_COUNT;
}

inline int settings_root_wrap(int position) {
  return ((position % SETTINGS_ROOT_COUNT) + SETTINGS_ROOT_COUNT) % SETTINGS_ROOT_COUNT;
}

inline const char *settings_root_name(int position) {
  position = settings_root_wrap(position);
  return position == SETTINGS_GROUP_COUNT ? "Back" : SETTINGS_GROUP_NAMES[position];
}

inline const char *settings_root_icon(int position) {
  position = settings_root_wrap(position);
  return position == SETTINGS_GROUP_COUNT ? ICON_BACK : SETTINGS_GROUP_ICONS[position];
}

inline int settings_group_entry_count(int group) {
  return settings_group_count(group) + 1;  // Last entry is Back.
}

inline int settings_group_entry_type_at(int group, int position) {
  int settings_count = settings_group_count(group);
  int entry_count = settings_count + 1;
  position = ((position % entry_count) + entry_count) % entry_count;
  return position == settings_count ? -1 : settings_group_type_at(group, position);
}

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
      "Icons",      "Unit",   "Idle Time",  "Dim After", "Dim Level",  "-",
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
