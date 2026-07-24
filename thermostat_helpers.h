// thermostat_helpers.h
//
// Helper functions for the ESPHome YAML files. They are included through
// `esphome: includes:` and are available inside lambdas. Keep them pure:
// no ESPHome ID access here.

#pragma once
#include <string>
#include <vector>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cmath>
#include <functional>
#include "esphome/core/time.h"

// Days since 1970-01-01 for a proleptic Gregorian civil date (Howard
// Hinnant's well-known constant-time algorithm). Used instead of mktime()/
// strptime() so the timer never depends on the libc timezone/DST state -
// both sides of a comparison are always computed the same deterministic way.
inline long civil_days_from_ymd(int y, int m, int d) {
  y -= m <= 2;
  long era = (y >= 0 ? y : y - 399) / 400;
  unsigned yoe = (unsigned) (y - era * 400);
  unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + (long) doe - 719468;
}

inline long civil_seconds(int y, int m, int d, int hh, int mm, int ss) {
  return civil_days_from_ymd(y, m, d) * 86400L + hh * 3600L + mm * 60L + ss;
}

// Seconds remaining until end_time_str, or a negative value if the string is
// empty, unparseable, or `now` isn't valid yet. Accepts either a full
// "YYYY-MM-DD HH:MM:SS" datetime or a plain "HH:MM:SS" time (combined with
// today's date from `now`).
inline long timer_remaining_seconds(const std::string &end_time_str,
                                     const esphome::ESPTime &now) {
  if (end_time_str.empty() || !now.is_valid()) return -1;
  int y, mo, d, h, mi, s;
  if (sscanf(end_time_str.c_str(), "%d-%d-%d %d:%d:%d", &y, &mo, &d, &h, &mi, &s) == 6) {
    // Full datetime, nothing more to fill in.
  } else if (sscanf(end_time_str.c_str(), "%d:%d:%d", &h, &mi, &s) == 3) {
    y = now.year;
    mo = now.month;
    d = now.day_of_month;
  } else {
    return -1;
  }
  long end_secs = civil_seconds(y, mo, d, h, mi, s);
  long now_secs = civil_seconds(now.year, now.month, now.day_of_month, now.hour, now.minute, now.second);
  return end_secs - now_secs;
}

// Inverse of civil_days_from_ymd (Howard Hinnant's algorithm): turns a day
// count since 1970-01-01 back into a proleptic Gregorian y/m/d.
inline void civil_ymd_from_days(long z, int &y, int &m, int &d) {
  z += 719468;
  long era = (z >= 0 ? z : z - 146096) / 146097;
  unsigned doe = (unsigned) (z - era * 146097);
  unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  long yy = (long) yoe + era * 400;
  unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  unsigned mp = (5 * doy + 2) / 153;
  d = (int) (doy - (153 * mp + 2) / 5 + 1);
  m = (int) (mp + (mp < 10 ? 3 : -9));
  y = (int) (yy + (m <= 2 ? 1 : 0));
}

// Inverse of civil_seconds(): formats a civil-epoch second count (same
// convention as civil_seconds/timer_remaining_seconds - not a Unix
// timestamp) back into "YYYY-MM-DD HH:MM:SS", for writing a new value into
// timer_end_time_txt after the knob adjusts it.
inline std::string format_civil_datetime(long total_seconds) {
  long days = total_seconds >= 0 ? total_seconds / 86400 : -((-total_seconds + 86399) / 86400);
  long secs_of_day = total_seconds - days * 86400;
  int y, mo, d;
  civil_ymd_from_days(days, y, mo, d);
  int h = (int) (secs_of_day / 3600);
  int mi = (int) ((secs_of_day % 3600) / 60);
  int s = (int) (secs_of_day % 60);
  char buf[24];
  snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d", y, mo, d, h, mi, s);
  return std::string(buf);
}

inline bool progress_is_running(float value) {
  return value > 0.0f && value < 100.0f;
}

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
static const char *const ICON_SUN = "\U000F05A8";           // mdi-white-balance-sunny
static const char *const ICON_EYEDROPPER = "\U000F020A";    // mdi-eyedropper
static const char *const ICON_DROPLET_HALF = "󰖎";  // mdi-water-percent
static const char *const ICON_BACK = "󰁍";          // mdi-arrow-left
static const char *const ICON_BRIGHTNESS_6 = "\U000F00DF";           // mdi-brightness-6
static const char *const ICON_SUN_THERMOMETER_OUTLINE = "\U000F18D7";  // mdi-sun-thermometer-outline
static const char *const ICON_PALETTE = "\U000F03D8";                // mdi-palette
static const char *const ICON_WINDOW_SHUTTER = "\U000F111C";         // mdi-window-shutter

// Menu icons. Index matches the settings menu item. Indices 5/6 (formerly
// Design/Icons) and 12 (formerly Mode) are permanently retired - removing a
// feature leaves a gap rather than renumbering everything after it, same as
// index 11 before it grew a use. Their icon/name slots stay as inert
// placeholders.
static const char *const MENU_ICONS[] = {
    "󰎓",  //  0 HVAC mode      mdi-thermostat
    "󰘮",  //  1 Preset         mdi-tune
    "󰃞",  //  2 Brightness     mdi-brightness-6
    "󰅐",  //  3 Clock          mdi-clock-outline
    "󰗊",  //  4 Language       mdi-translate
    "",   //  5 removed (Design)
    "",   //  6 removed (Icons)
    "󰔄",  //  7 Unit           mdi-temperature-celsius
    "󰔟",  //  8 Idle time      mdi-timer-sand
    "󰃜",  //  9 Dim after      mdi-brightness-4
    "󰏰",  // 10 Dim level      mdi-percent
    "󰔟",  // 11 Timer auto-home  mdi-timer-sand (reused)
    "",   // 12 removed (Mode)
    "󰑧",  // 13 Step           mdi-rotate-right
    "󰖩",  // 14 WiFi           mdi-wifi
    "󰋽",  // 15 Firmware       mdi-information-outline
    "󰜉",  // 16 Reset          mdi-restart
    "󰌵",  // 17 LED            mdi-lightbulb
    "󰃟",  // 18 LED brightness mdi-brightness-7
    "󰌵",  // 19 Timer LED blink       mdi-lightbulb (reused)
    "󰔟",  // 20 Progress auto-home    mdi-timer-sand (reused)
    "󰌵",  // 21 Progress LED blink    mdi-lightbulb (reused)
    "󰑧",  // 22 Timer knob step       mdi-rotate-right (reused)
    "󰌵",  // 23 Timer value blink     mdi-lightbulb (reused)
    "󰌵",  // 24 Progress value blink  mdi-lightbulb (reused)
    "\U000F020A",  // 25 Timer blink LED color    mdi-eyedropper (reused)
    "\U000F05A8",  // 26 Timer show screen        mdi-white-balance-sunny (reused)
    "\U000F05A8",  // 27 Progress show screen     mdi-white-balance-sunny (reused)
    "\U000F020A",  // 28 Progress blink LED color mdi-eyedropper (reused)
    "󰃟",  // 29 Timer blink LED brightness    mdi-brightness-7 (reused)
    "󰃟",  // 30 Progress blink LED brightness mdi-brightness-7 (reused)
    "󰔟",  // 31 Notify auto-home       mdi-timer-sand (reused)
    "󰌵",  // 32 Notify LED blink       mdi-lightbulb (reused)
    "\U000F020A",  // 33 Notify blink LED color      mdi-eyedropper (reused)
    "󰃟",  // 34 Notify blink LED brightness mdi-brightness-7 (reused)
    "󰌵",  // 35 Notify value blink     mdi-lightbulb (reused)
    "\U000F05A8",  // 36 Notify show screen    mdi-white-balance-sunny (reused)
};
constexpr int SETTINGS_MENU_COUNT = 37;

// Root groups for the hierarchical settings screen. The values below are the
// stable setting IDs used by the existing editor/apply logic.
constexpr int SETTINGS_GROUP_COUNT = 5;
constexpr int SETTINGS_ROOT_COUNT = SETTINGS_GROUP_COUNT + 1;  // + Back
static const char *const SETTINGS_GROUP_NAMES[SETTINGS_GROUP_COUNT] = {
    "Thermostat", "Timer", "Progress", "Notify", "System"};
static const char *const SETTINGS_GROUP_ICONS[SETTINGS_GROUP_COUNT] = {
    MENU_ICONS[0], MENU_ICONS[8], MENU_ICONS[10], ICON_SUN, MENU_ICONS[14]};

static constexpr int SETTINGS_GROUP_THERMOSTAT[] = {0, 1, 7, 13};
static constexpr int SETTINGS_GROUP_TIMER[] = {11, 19, 25, 29, 23, 22, 26};
static constexpr int SETTINGS_GROUP_PROGRESS[] = {20, 21, 28, 30, 24, 27};
static constexpr int SETTINGS_GROUP_NOTIFY[] = {31, 32, 33, 34, 35, 36};
static constexpr int SETTINGS_GROUP_SYSTEM[] = {
    2, 4, 3, 18, 17, 10, 9, 8, 14, 15, 16};

inline int settings_group_count(int group) {
  switch (group) {
    case 0: return sizeof(SETTINGS_GROUP_THERMOSTAT) / sizeof(SETTINGS_GROUP_THERMOSTAT[0]);
    case 1: return sizeof(SETTINGS_GROUP_TIMER) / sizeof(SETTINGS_GROUP_TIMER[0]);
    case 2: return sizeof(SETTINGS_GROUP_PROGRESS) / sizeof(SETTINGS_GROUP_PROGRESS[0]);
    case 3: return sizeof(SETTINGS_GROUP_NOTIFY) / sizeof(SETTINGS_GROUP_NOTIFY[0]);
    case 4: return sizeof(SETTINGS_GROUP_SYSTEM) / sizeof(SETTINGS_GROUP_SYSTEM[0]);
    default: return 0;
  }
}

inline int settings_group_type_at(int group, int position) {
  int count = settings_group_count(group);
  if (count <= 0) return 0;
  position = ((position % count) + count) % count;
  switch (group) {
    case 0: return SETTINGS_GROUP_THERMOSTAT[position];
    case 1: return SETTINGS_GROUP_TIMER[position];
    case 2: return SETTINGS_GROUP_PROGRESS[position];
    case 3: return SETTINGS_GROUP_NOTIFY[position];
    case 4: return SETTINGS_GROUP_SYSTEM[position];
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

// Settings-row icon in the entity overview list. Still the toggle-switch
// glyph formerly shown via MENU_ICONS[12] (the retired "Mode" setting) - the
// glyph is still loaded in the font, only that array slot was blanked out.
static const char *const ICON_SETTINGS_ROW = "󰔡";  // mdi-toggle-switch (reused)

inline const char *entity_menu_icon(int position, int climate_count, int light_count,
                                     int cover_count, int timer_count, int progress_count,
                                     int notify_count) {
  int before_timer = climate_count + light_count + cover_count;
  int before_progress = before_timer + timer_count;
  int before_notify = before_progress + progress_count;
  int before_settings = before_notify + notify_count;
  int count = before_settings + 2;  // Settings + Back
  if (count <= 0) return ICON_BACK;
  position = ((position % count) + count) % count;
  if (position < climate_count) return MENU_ICONS[0];
  if (position < climate_count + light_count) return MENU_ICONS[17];
  if (position < before_timer) return ICON_WINDOW_SHUTTER;
  if (position < before_progress) return MENU_ICONS[3];  // Timer: clock
  if (position < before_notify) return MENU_ICONS[10];   // Progress: percent
  if (position < before_settings) return ICON_SUN;        // Notify: sun (reused)
  return position == before_settings ? ICON_SETTINGS_ROW : ICON_BACK;
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

// ------------------------------------------------------------------
// Shared list-menu behavior. Every scrollable menu (settings root, group
// items, entity overview, and in-editor option lists) advances and renders
// through these two functions, so the knob and the row layout behave
// identically no matter which list is on screen.
// ------------------------------------------------------------------

// Clockwise (positive encoder delta) moves the selection down (toward
// index + 1). Only the sign of `delta` matters. With `loop` false (the
// default) the index clamps at 0/count-1 instead of wrapping; pass `loop`
// true for callers that want it to wrap around (e.g. swipe-driven cycling).
inline int menu_scroll_index(int index, int delta, int count, bool loop = false) {
  if (count <= 0) return 0;
  int d = delta > 0 ? 1 : -1;
  if (loop) return ((index + d) % count + count) % count;
  int next = index + d;
  if (next < 0) return 0;
  if (next >= count) return count - 1;
  return next;
}

struct MenuWindow {
  std::string prev2, prev, cur, next, next2, value;
};

// Builds the prev2/prev/cur/next/next2 row texts and the "i / n" value
// string around `index`, calling `name(position)` for each visible row.
// `extra_rows` enables the outer prev2/next2 rows (only shown once the list
// has at least 5 entries). With `loop` false (the default, matching
// menu_scroll_index) a row past the first/last entry is left empty instead
// of wrapping around to show a menu item that scrolling can't reach.
inline MenuWindow menu_window(int index, int count, bool extra_rows,
                               const std::function<std::string(int)> &name,
                               bool loop = false) {
  MenuWindow m;
  if (count <= 0 || !name) return m;
  int i = ((index % count) + count) % count;
  bool prev2_ok = extra_rows && count >= 5 && (loop || i - 2 >= 0);
  bool prev_ok = count > 1 && (loop || i - 1 >= 0);
  bool next_ok = count > 1 && (loop || i + 1 < count);
  bool next2_ok = extra_rows && count >= 5 && (loop || i + 2 < count);
  m.prev2 = prev2_ok ? name(i - 2) : "";
  m.prev = prev_ok ? name(i - 1) : "";
  m.cur = name(i);
  m.next = next_ok ? name(i + 1) : "";
  m.next2 = next2_ok ? name(i + 2) : "";
  char buf[32];
  snprintf(buf, sizeof(buf), "%d / %d", i + 1, count);
  m.value = buf;
  return m;
}

// Same prev/cur/next/"i / n" pattern as menu_window, but for a number
// entity's own value range instead of a list of names - lets every settings
// editor (booleans, numeric steppers, select lists) use one consistent
// scroll display. `value` need not fall exactly on a step multiple (e.g.
// right after boot); it's clamped into range and snapped to the nearest step.
inline MenuWindow numeric_menu_window(int value, int minv, int maxv, int step,
                                       const std::function<std::string(int)> &format) {
  if (step <= 0) step = 1;
  int count = (maxv - minv) / step + 1;
  int clamped = value < minv ? minv : (value > maxv ? maxv : value);
  int idx = (clamped - minv + step / 2) / step;
  return menu_window(idx, count, false,
                      [minv, step, &format](int p) -> std::string { return format(minv + p * step); });
}

// Same pattern for a plain two-option boolean setting (Off/On, 12h/24h, ...).
inline MenuWindow bool_menu_window(int index, const char *off_label, const char *on_label) {
  const char *opts[] = {off_label, on_label};
  return menu_window(index, 2, false, [&opts](int p) -> std::string { return opts[((p % 2) + 2) % 2]; });
}

// ------------------------------------------------------------------
// Shared bidirectional, looping swipe-cycling between a fixed set of
// screens (e.g. the light control pages). Both gesture directions on an
// axis are wired to the matching helper so swiping either way always
// wraps around, instead of every direction cycling the same way forward.
// ------------------------------------------------------------------

// Step for on_swipe_up / on_swipe_down. `down` is true for the swipe-down
// gesture (moves to the next screen), false for swipe-up (previous screen).
inline int swipe_step_vertical(int current, bool down, int count) {
  return menu_scroll_index(current, down ? 1 : -1, count, true);
}

// Step for on_swipe_left / on_swipe_right. `right` is true for the
// swipe-right gesture (moves to the next screen), false for swipe-left.
inline int swipe_step_horizontal(int current, bool right, int count) {
  return menu_scroll_index(current, right ? 1 : -1, count, true);
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
      "HVAC Mode",  "Preset", "Brightness", "Clock",     "Language",   "-",
      "-",          "Unit",   "Idle Time",  "Dim After", "Dim Level",  "Timer Auto-Home",
      "-",          "Step",      "WiFi",    "Firmware",  "Reset",      "LED",
      "LED Brightness", "Timer LED Blink", "Progress Auto-Home", "Progress LED Blink",
      "Timer Knob Step", "Timer Value Blink", "Progress Value Blink", "Timer Blink LED Color",
      "Timer Show Screen", "Progress Show Screen", "Progress Blink LED Color",
      "Timer Blink Brightness", "Progress Blink Brightness", "Notify Auto-Home",
      "Notify LED Blink", "Notify Blink LED Color", "Notify Blink Brightness",
      "Notify Value Blink", "Notify Show Screen"};
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
// Fixed preset colors for the Timer/Progress finish blink on the status LED
// (see check_finish_effects / update_status_led). A small named set, not a
// hue wheel, matches how the setting is picked (knob-scroll through a short
// list), same reasoning as LIGHT_COLOR_PRESETS.
// ------------------------------------------------------------------
struct BlinkLedColor { const char *name; float r, g, b; };
constexpr int BLINK_LED_COLOR_COUNT = 10;
static constexpr BlinkLedColor BLINK_LED_COLORS[BLINK_LED_COLOR_COUNT] = {
    {"Green", 0.0f, 1.0f, 0.0f},
    {"Red", 1.0f, 0.0f, 0.0f},
    {"Blue", 0.0f, 0.4f, 1.0f},
    {"Yellow", 1.0f, 1.0f, 0.0f},
    {"Orange", 1.0f, 0.5f, 0.0f},
    {"Purple", 0.6f, 0.0f, 1.0f},
    {"Cyan", 0.0f, 1.0f, 1.0f},
    {"Magenta", 1.0f, 0.0f, 1.0f},
    {"White", 1.0f, 1.0f, 1.0f},
    {"Pink", 1.0f, 0.4f, 0.7f},
};

inline const char *blink_led_color_name(int i) {
  i = ((i % BLINK_LED_COLOR_COUNT) + BLINK_LED_COLOR_COUNT) % BLINK_LED_COLOR_COUNT;
  return BLINK_LED_COLORS[i].name;
}

inline LedColor blink_led_color_rgb(const std::string &name) {
  for (int i = 0; i < BLINK_LED_COLOR_COUNT; i++)
    if (name == BLINK_LED_COLORS[i].name) return {BLINK_LED_COLORS[i].r, BLINK_LED_COLORS[i].g, BLINK_LED_COLORS[i].b};
  return {BLINK_LED_COLORS[0].r, BLINK_LED_COLORS[0].g, BLINK_LED_COLORS[0].b};  // default Green
}

// ------------------------------------------------------------------
// Light color mode presets. A full continuous hue ring needed 96
// individually recolored arc segments per refresh, which was too slow on
// this panel; picking from a small fixed set of colors is both fast and
// matches how the knob is actually used (favorite lamp colors, not a
// precise color wheel). `rgb` is precomputed offline (see
// tools/lvgl_preview.py's hsv_int) so refresh_light never has to run
// lv_color_hsv_to_rgb for these swatches, only look up a constant.
// ------------------------------------------------------------------
struct ColorPreset { float hue; float saturation; uint32_t rgb; };
constexpr int LIGHT_COLOR_PRESET_COUNT = 18;
static constexpr ColorPreset LIGHT_COLOR_PRESETS[LIGHT_COLOR_PRESET_COUNT] = {
    {0.0f, 70.0f, 0xFF4D4D},
    {20.0f, 70.0f, 0xFF884D},
    {40.0f, 70.0f, 0xFFC34D},
    {60.0f, 70.0f, 0xFFFF4D},
    {80.0f, 70.0f, 0xC4FF4D},
    {100.0f, 70.0f, 0x88FF4D},
    {120.0f, 70.0f, 0x4DFF4D},
    {140.0f, 70.0f, 0x4DFF88},
    {160.0f, 70.0f, 0x4DFFC3},
    {180.0f, 70.0f, 0x4DFFFF},
    {200.0f, 70.0f, 0x4DC3FF},
    {220.0f, 70.0f, 0x4D88FF},
    {240.0f, 70.0f, 0x4D4DFF},
    {260.0f, 70.0f, 0x884DFF},
    {280.0f, 70.0f, 0xC44DFF},
    {300.0f, 70.0f, 0xFF4DFF},
    {320.0f, 70.0f, 0xFF4DC4},
    {340.0f, 70.0f, 0xFF4D88},
};

// Closest preset to a given HA hue, e.g. to highlight the right swatch when
// a light's color was last set from Home Assistant instead of the knob.
inline int nearest_color_preset(float hue) {
  int best = 0;
  float best_diff = 361.0f;
  for (int i = 0; i < LIGHT_COLOR_PRESET_COUNT; i++) {
    float diff = std::fabs(std::fmod(hue - LIGHT_COLOR_PRESETS[i].hue + 540.0f, 360.0f) - 180.0f);
    if (diff < best_diff) {
      best_diff = diff;
      best = i;
    }
  }
  return best;
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

// The "hvac_modes" attribute HA sends over the API is a stringified list
// (e.g. "['off', 'heat', 'cool']"); ha_parse_list splits on every non-alnum
// character, so anything other than a plain mode word in there (stray enum
// class names, punctuation runs, ...) would otherwise turn into its own
// bogus entry and show up as an unlabeled "-" row. Keep only tokens that are
// an actual HVACMode value, same set ha_hvac_mode_label() recognizes.
inline bool ha_is_known_hvac_mode(const std::string &m) {
  return m == "off" || m == "heat" || m == "cool" || m == "heat_cool" ||
         m == "auto" || m == "dry" || m == "fan_only";
}

inline std::vector<std::string> ha_filter_modes(const std::string &raw) {
  std::vector<std::string> v;
  for (auto &m : ha_parse_list(raw))
    if (ha_is_known_hvac_mode(m)) v.push_back(m);
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
