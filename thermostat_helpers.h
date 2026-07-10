// thermostat_helpers.h
//
// Hilfsfunktionen für thermostat.yaml (über `esphome: includes:`
// eingebunden, in allen Lambdas verfügbar). Reine Funktionen/Konstanten
// ohne Zugriff auf ESPHome-IDs.

#pragma once
#include <string>
#include <vector>
#include <cctype>

// ------------------------------------------------------------------
// Bootstrap-Icons (assets/fonts/bootstrap-icons.woff). Codepoints aus
// bootstrap-icons.json (twbs/icons, main) extrahiert.
// ------------------------------------------------------------------
static const char *const ICON_FIRE = "";          // bi-fire
static const char *const ICON_SNOW = "";          // bi-snow
static const char *const ICON_TOGGLE_OFF = "";    // bi-toggle-off
static const char *const ICON_POWER = "";   // bi-power
static const char *const ICON_THERMO_HIGH = "";   // bi-thermometer-high
static const char *const ICON_DROPLET_HALF = "";  // bi-droplet-half

// Menü-Icons, Index = Menüpunkt (siehe SETTINGS-Menü in thermostat.yaml)
static const char *const MENU_ICONS[] = {
    "",  //  0 HVAC-Modus     bi-thermometer-half
    "",  //  1 Preset         bi-sliders
    "",  //  2 Helligkeit     bi-brightness-high
    "",  //  3 Uhrzeit        bi-clock
    "",  //  4 Sprache        bi-translate
    "",  //  5 Design         bi-palette
    "",  //  6 Icons          bi-eye
    "",  //  7 Einheit        bi-rulers
    "",  //  8 Idle-Zeit      bi-hourglass-split
    "",  //  9 Dimmen nach    bi-brightness-low
    "",  // 10 Dimm-Stufe     bi-percent
    "",  // 11 3s-Taste       bi-stopwatch
    "",  // 12 Knob-Modi      bi-toggles
    "\uf56b",  // 13 Knob-Schritt   bi-sliders (Glyph bereits im Font)
    "",  // 14 WLAN           bi-wifi
    "",  // 15 Firmware       bi-info-circle
    "",  // 16 Zurücksetzen   bi-arrow-counterclockwise
    "",  // 17 LED             bi-lightbulb
    "",  // 18 LED-Helligkeit  bi-brightness-high (wie Index 2 Helligkeit)
};
constexpr int SETTINGS_MENU_COUNT = 19;

// Icon je better_thermostat-Preset
inline const char *ha_preset_icon(const std::string &p) {
  if (p == "eco") return "";       // bi-tree
  if (p == "comfort") return "";   // bi-cup-hot
  if (p == "home") return "";      // bi-house
  if (p == "away") return "";      // bi-person-walking
  if (p == "sleep") return "";     // bi-moon-stars
  if (p == "boost") return "";     // bi-lightning-charge
  if (p == "activity") return "";  // bi-activity
  return "";                       // bi-dash-circle (none/unbekannt)
}

// ------------------------------------------------------------------
// Lokalisierung (de = true -> Deutsch, sonst Englisch)
// ------------------------------------------------------------------
inline std::string ha_hvac_mode_label(std::string mode, bool de) {
  if (mode == "heat") return de ? "Heizen" : "Heat";
  if (mode == "cool") return de ? "Kühlen" : "Cool";
  if (mode == "heat_cool") return de ? "Heizen/Kühlen" : "Heat/Cool";
  if (mode == "auto") return "Auto";
  if (mode == "off") return de ? "Aus" : "Off";
  if (mode == "dry") return de ? "Entfeuchten" : "Dry";
  if (mode == "fan_only") return de ? "Lüften" : "Fan";
  return "-";
}

inline std::string ha_preset_label(const std::string &p, bool de) {
  if (p == "none") return de ? "Kein Preset" : "No preset";
  if (p == "eco") return "Eco";
  if (p == "comfort") return de ? "Komfort" : "Comfort";
  if (p == "home") return de ? "Zuhause" : "Home";
  if (p == "away") return de ? "Abwesend" : "Away";
  if (p == "sleep") return de ? "Schlafen" : "Sleep";
  if (p == "boost") return "Boost";
  if (p == "activity") return de ? "Aktivität" : "Activity";
  if (p.empty()) return "-";
  return p;
}

inline const char *settings_menu_name(int i, bool de) {
  static const char *const EN[] = {
      "HVAC Mode",  "Preset", "Brightness", "Clock",     "Language",   "Design",
      "Icons",      "Unit",   "Idle Time",  "Dim After", "Dim Level",  "3s Button",
      "Knob Modes", "Knob Step", "WiFi",    "Firmware",  "Reset",      "LED",
      "LED Brightness"};
  static const char *const DE[] = {
      "HVAC-Modus", "Preset", "Helligkeit", "Uhrzeit",     "Sprache",    "Design",
      "Icons",      "Einheit", "Idle-Zeit", "Dimmen nach", "Dimm-Stufe", "3s-Taste",
      "Knob-Modi",  "Knob-Schritt", "WLAN", "Firmware",    "Zurücksetzen", "LED",
      "LED-Helligkeit"};
  if (i < 0 || i >= SETTINGS_MENU_COUNT) return "-";
  return de ? DE[i] : EN[i];
}

// ------------------------------------------------------------------
// Status-LED-Farbe je HVAC-Zustand (240er): off -> weiß, Kühlen ->
// weiß-blau, Heizen -> orange. Bei Dual-Setpoint zeigt adjust_target,
// welcher Sollwert gerade aktiv ist (0 = TRV/Heizen, 1 = AC/Kühlen).
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
// Temperatur-Einheit: intern IMMER °C, Umrechnung nur für die Anzeige
// ------------------------------------------------------------------
inline float display_temp(float celsius, bool fahrenheit) {
  return fahrenheit ? celsius * 1.8f + 32.0f : celsius;
}

// ------------------------------------------------------------------
// HA-Listen-Attribute parsen ("['heat', 'off']" -> {"heat","off"})
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
