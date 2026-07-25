// thermostat_helpers.h
//
// Helper functions for the ESPHome YAML files. They are included through
// `esphome: includes:` and are available inside lambdas. Keep them pure:
// no ESPHome ID access here.

#pragma once
#include <string>
#include <vector>
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
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
// timer_edit_value (the Timer add-page's preview end-time) after the knob
// adjusts it.
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

// Keep connection states as stable English keys internally (compared/set
// all over the file) and only translate at render time. Returns the i18n key
// for a known status, or "" if the status isn't a recognized message (the
// caller should then fall back to showing `status` itself, e.g. while empty
// before the first refresh_connection run).
inline std::string ha_connection_key(const std::string &status) {
  if (status == "WiFi connecting...") return "conn.wifi_connecting";
  if (status == "WiFi error") return "conn.wifi_error";
  if (status == "HA connecting...") return "conn.ha_connecting";
  if (status == "HA unreachable") return "conn.ha_unreachable";
  if (status == "Smart Knob config loading...") return "conn.config_loading";
  if (status == "Smart Knob config not found") return "conn.config_not_found";
  if (status == "Smart Knob config JSON error") return "conn.config_json_error";
  if (status == "No climate configured") return "conn.no_climate";
  if (status == "Climate entity loading...") return "conn.climate_loading";
  if (status == "Climate entity not found") return "conn.climate_not_found";
  return "";
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
// Design/Icons), 12 (formerly Mode), 32/33/35 (formerly the static Notify's
// LED Blink/Blink Color/Value Blink - now per-queue-entry instead of a
// global setting), and 36 (formerly Notify Show Screen) are permanently
// retired - removing a feature leaves a gap rather than renumbering
// everything after it, same as index 11 before it grew a use. Their
// icon/name slots stay as inert placeholders.
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
    "",   // 32 removed (Notify LED Blink)
    "",   // 33 removed (Notify Blink LED Color)
    "󰃟",  // 34 Notify blink LED brightness mdi-brightness-7 (reused)
    "",   // 35 removed (Notify Value Blink)
    "",   // 36 removed (Notify Show Screen)
};
constexpr int SETTINGS_MENU_COUNT = 37;

// Root groups for the hierarchical settings screen. The values below are the
// stable setting IDs used by the existing editor/apply logic.
constexpr int SETTINGS_GROUP_COUNT = 5;
constexpr int SETTINGS_ROOT_COUNT = SETTINGS_GROUP_COUNT + 1;  // + Back
static const char *const SETTINGS_GROUP_KEYS[SETTINGS_GROUP_COUNT] = {
    "settings.group.thermostat", "settings.group.timer", "settings.group.progress",
    "settings.group.notify", "settings.group.system"};
static const char *const SETTINGS_GROUP_ICONS[SETTINGS_GROUP_COUNT] = {
    MENU_ICONS[0], MENU_ICONS[8], MENU_ICONS[10], ICON_SUN, MENU_ICONS[14]};

static constexpr int SETTINGS_GROUP_THERMOSTAT[] = {0, 1, 7, 13};
static constexpr int SETTINGS_GROUP_TIMER[] = {11, 19, 25, 29, 23, 22, 26};
static constexpr int SETTINGS_GROUP_PROGRESS[] = {20, 30};
static constexpr int SETTINGS_GROUP_NOTIFY[] = {31, 34};
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
  return position == SETTINGS_GROUP_COUNT ? "common.back" : SETTINGS_GROUP_KEYS[position];
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
// Returns the i18n key for a known HVAC mode, or "" if `mode` isn't
// recognized (caller falls back to "-", same as the old default branch).
inline std::string ha_hvac_mode_key(const std::string &mode) {
  if (mode == "heat") return "hvac.heat";
  if (mode == "cool") return "hvac.cool";
  if (mode == "heat_cool") return "hvac.heat_cool";
  if (mode == "auto") return "hvac.auto";
  if (mode == "off") return "hvac.off";
  if (mode == "dry") return "hvac.dry";
  if (mode == "fan_only") return "hvac.fan_only";
  return "";
}

// Returns the i18n key for a known preset, or "" for both an empty preset
// and a custom/unrecognized one - the caller distinguishes those (empty ->
// "-", unrecognized -> show the raw preset name as-is), same as before.
inline std::string ha_preset_key(const std::string &p) {
  if (p == "none") return "preset.none";
  if (p == "eco") return "preset.eco";
  if (p == "comfort") return "preset.comfort";
  if (p == "home") return "preset.home";
  if (p == "away") return "preset.away";
  if (p == "sleep") return "preset.sleep";
  if (p == "boost") return "preset.boost";
  if (p == "activity") return "preset.activity";
  return "";
}

// Returns the i18n key for a settings-menu item, or "" for the retired
// placeholder slots (5, 6, 12, 32, 33, 35, 36) and out-of-range indices -
// callers translate non-empty keys and fall back to "-" for empty ones.
inline const char *settings_menu_name(int i) {
  static const char *const KEYS[] = {
      "settings.item.hvac_mode", "settings.item.preset", "settings.item.brightness",
      "settings.item.clock", "settings.item.language", "",
      "", "settings.item.unit", "settings.item.idle_time",
      "settings.item.dim_after", "settings.item.dim_level", "settings.item.timer_auto_home",
      "", "settings.item.step", "settings.item.wifi",
      "settings.item.firmware", "settings.item.reset", "settings.item.led",
      "settings.item.led_brightness", "settings.item.timer_led_blink", "settings.item.progress_auto_home",
      "", "settings.item.timer_knob_step", "settings.item.timer_value_blink",
      "", "settings.item.timer_blink_color", "settings.item.timer_show_screen",
      "", "", "settings.item.timer_blink_brightness",
      "settings.item.progress_blink_brightness", "settings.item.notify_auto_home", "",
      "", "settings.item.notify_blink_brightness", "",
      ""};
  if (i < 0 || i >= SETTINGS_MENU_COUNT) return "";
  return KEYS[i];
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

// i18n key for a color's on-screen display name. blink_led_color_name() above
// stays the source of truth for the entity's actual (always-English) stored
// option value - this is only for translating what the editor screen renders
// while scrolling through the color list.
inline const char *blink_led_color_key(int i) {
  static const char *const KEYS[BLINK_LED_COLOR_COUNT] = {
      "color.green", "color.red", "color.blue", "color.yellow", "color.orange",
      "color.purple", "color.cyan", "color.magenta", "color.white", "color.pink"};
  i = ((i % BLINK_LED_COLOR_COUNT) + BLINK_LED_COLOR_COUNT) % BLINK_LED_COLOR_COUNT;
  return KEYS[i];
}

// Same as blink_led_color_key(), but looked up by the color's stored
// (always-English) name instead of its index - for translating the group-
// list preview value next to "Timer/Progress/Notify Blink LED Color".
inline const char *blink_led_color_key_for_name(const std::string &name) {
  for (int i = 0; i < BLINK_LED_COLOR_COUNT; i++)
    if (name == BLINK_LED_COLORS[i].name) return blink_led_color_key(i);
  return "color.green";
}

inline LedColor blink_led_color_rgb(const std::string &name) {
  for (int i = 0; i < BLINK_LED_COLOR_COUNT; i++)
    if (name == BLINK_LED_COLORS[i].name) return {BLINK_LED_COLORS[i].r, BLINK_LED_COLORS[i].g, BLINK_LED_COLORS[i].b};
  return {BLINK_LED_COLORS[0].r, BLINK_LED_COLORS[0].g, BLINK_LED_COLORS[0].b};  // default Green
}

// One-letter codes for BLINK_LED_COLORS, used by export_settings/
// import_settings to keep the exported string well under Home Assistant's
// 255-char text-entity limit - K stands in for Pink since P is Purple.
inline char blink_led_color_letter(const std::string &name) {
  static const char letters[BLINK_LED_COLOR_COUNT] = {'G', 'R', 'B', 'Y', 'O', 'P', 'C', 'M', 'W', 'K'};
  for (int i = 0; i < BLINK_LED_COLOR_COUNT; i++)
    if (name == BLINK_LED_COLORS[i].name) return letters[i];
  return 'G';
}

inline const char *blink_led_color_from_letter(char c) {
  static const char letters[BLINK_LED_COLOR_COUNT] = {'G', 'R', 'B', 'Y', 'O', 'P', 'C', 'M', 'W', 'K'};
  for (int i = 0; i < BLINK_LED_COLOR_COUNT; i++)
    if (c == letters[i]) return BLINK_LED_COLORS[i].name;
  return "Green";
}

// Converts a BLINK_LED_COLORS name into the same 6-digit hex format used by
// the Timer/Progress/Notify queues' `color` field - lets an on-device-
// created Timer entry (see timer_click's add-page branch) carry forward
// whatever the "Timer Blink LED Color" setting currently is, in the same
// format HA would have supplied for a queue entry it added itself.
inline std::string blink_led_color_hex(const std::string &name) {
  LedColor c = blink_led_color_rgb(name);
  char buf[7];
  snprintf(buf, sizeof(buf), "%02X%02X%02X", (int) roundf(c.r * 255.0f), (int) roundf(c.g * 255.0f),
           (int) roundf(c.b * 255.0f));
  return std::string(buf);
}

// ------------------------------------------------------------------
// Notify queue: internal storage (id(notify_queue), a plain global, not an
// HA entity - no 255-char limit to worry about). Home Assistant never
// writes this directly; it writes one notification's fields to
// notify_add_txt as a human-readable "key=value;key=value;..." string and
// presses "Add to Queue", which appends an ESP-assigned "seq=<n>" (an
// insertion-order counter, not something HA provides) and stores the whole
// entry as its own line in id(notify_queue). Removing one works the other
// way: HA writes the entry's `id` to notify_remove_id_txt and presses
// "Remove from Queue", which parses, drops the matching entry, and
// re-serializes the rest.
//
// Recognized keys: id, title, subtitle, color (a 6-digit hex RGB string,
// with or without a leading '#'), led_mode ("0"=off/"1"=on/"2"=blink),
// priority ("0"=low/"1"=medium/"2"=high), value_blink ("1"/"0", whether the
// title text itself also blinks), seq (insertion order, ESP-assigned), end
// (optional - a "YYYY-MM-DD HH:MM:SS" or "HH:MM:SS" date-time, same format
// as a QueuedTimer's end_time; when set the entry is self-deleting: the Notify
// screen shows a Timer-style countdown arc for it, and it's automatically
// dropped from the queue once that time passes - see check_finish_effects),
// added (ESP-assigned Unix timestamp captured when an `end` was set, used
// as the countdown arc's start point - not meaningful without `end`),
// arc_color (optional 6-digit hex RGB for the countdown arc itself - empty
// keeps the default purple, see ARC_DEFAULT_COLOR/arc_color_or_default()).
// Unknown keys are ignored; missing keys fall back to sane defaults.
// Example HA input: "id=door1;title=Door open;subtitle=Garage;color=FF0000;
// led_mode=2;priority=2;value_blink=1;end=2026-07-25 18:30:00;arc_color=00AAFF"
//
// parse_notify_queue() always returns entries sorted highest-priority
// first, and within the same priority, most-recently-added first.
// ------------------------------------------------------------------
struct QueuedNotify {
  std::string entry_id;
  std::string title, subtitle;
  std::string color;  // 6-digit hex RGB, no '#'
  int led_mode;
  int priority;
  bool value_blink;
  uint32_t seq;
  std::string end_time;   // empty = no self-delete/countdown
  uint32_t added_epoch;   // Unix timestamp, only meaningful if end_time set
  std::string arc_color;  // empty = default arc color
};

// Parses one "key=value;key=value;..." line into a QueuedNotify, applying
// defaults for anything missing. Shared by parse_notify_queue() (splitting
// the multi-line queue first) and notify_add_to_queue (validating/
// normalizing a single freshly-entered entry before it's stored).
inline QueuedNotify parse_notify_entry(const std::string &line) {
  QueuedNotify qn;
  qn.color = "00FF00";
  qn.led_mode = 2;
  qn.priority = 1;
  qn.value_blink = false;
  qn.seq = 0;
  qn.added_epoch = 0;
  size_t pos = 0;
  while (pos < line.size()) {
    size_t semi = line.find(';', pos);
    std::string token = (semi == std::string::npos) ? line.substr(pos) : line.substr(pos, semi - pos);
    pos = (semi == std::string::npos) ? line.size() : semi + 1;
    size_t eq = token.find('=');
    if (eq == std::string::npos) continue;
    std::string key = token.substr(0, eq);
    std::string value = token.substr(eq + 1);
    if (key == "id") qn.entry_id = value;
    else if (key == "title") qn.title = value;
    else if (key == "subtitle") qn.subtitle = value;
    else if (key == "color") {
      if (!value.empty() && value[0] == '#') value = value.substr(1);
      qn.color = value.empty() ? "00FF00" : value;
    } else if (key == "led_mode") {
      qn.led_mode = value.empty() ? 2 : (value[0] - '0');
      if (qn.led_mode < 0 || qn.led_mode > 2) qn.led_mode = 2;
    } else if (key == "priority") {
      qn.priority = value.empty() ? 1 : (value[0] - '0');
      if (qn.priority < 0 || qn.priority > 2) qn.priority = 1;
    } else if (key == "value_blink") {
      qn.value_blink = value == "1";
    } else if (key == "seq") {
      qn.seq = (uint32_t) strtoul(value.c_str(), nullptr, 10);
    } else if (key == "end") {
      qn.end_time = value;
    } else if (key == "added") {
      qn.added_epoch = (uint32_t) strtoul(value.c_str(), nullptr, 10);
    } else if (key == "arc_color") {
      if (!value.empty() && value[0] == '#') value = value.substr(1);
      qn.arc_color = value;
    }
  }
  return qn;
}

inline std::vector<QueuedNotify> parse_notify_queue(const std::string &raw) {
  std::vector<QueuedNotify> out;
  size_t pos = 0;
  while (pos < raw.size()) {
    size_t nl = raw.find('\n', pos);
    std::string line = (nl == std::string::npos) ? raw.substr(pos) : raw.substr(pos, nl - pos);
    pos = (nl == std::string::npos) ? raw.size() : nl + 1;
    if (line.empty()) continue;
    QueuedNotify qn = parse_notify_entry(line);
    if (qn.entry_id.empty() || qn.title.empty()) continue;  // need at least id + title
    out.push_back(qn);
  }
  std::sort(out.begin(), out.end(), [](const QueuedNotify &a, const QueuedNotify &b) {
    if (a.priority != b.priority) return a.priority > b.priority;
    return a.seq > b.seq;
  });
  return out;
}

// Inverse of parse_notify_entry() - rebuilds the "key=value;..." line, used
// to write the queue back out after removing one entry from it.
inline std::string serialize_notify_entry(const QueuedNotify &q) {
  char num[16];
  std::string out = "id=" + q.entry_id + ";title=" + q.title + ";subtitle=" + q.subtitle + ";color=" + q.color +
                     ";led_mode=";
  snprintf(num, sizeof(num), "%d", q.led_mode);
  out += num;
  out += ";priority=";
  snprintf(num, sizeof(num), "%d", q.priority);
  out += num;
  out += ";value_blink=";
  out += q.value_blink ? "1" : "0";
  out += ";seq=";
  snprintf(num, sizeof(num), "%lu", (unsigned long) q.seq);
  out += num;
  if (!q.end_time.empty()) {
    out += ";end=" + q.end_time + ";added=";
    snprintf(num, sizeof(num), "%lu", (unsigned long) q.added_epoch);
    out += num;
  }
  if (!q.arc_color.empty()) out += ";arc_color=" + q.arc_color;
  return out;
}

// Default arc indicator color (the purple used everywhere before per-entry
// arc colors existed). arc_color_or_default() below falls back to this
// whenever an entry didn't specify its own arc_color.
constexpr uint32_t ARC_DEFAULT_COLOR = 0x6E4FBD;

// Parses an entry's optional arc_color (6-digit hex, no '#') into a 24-bit
// 0xRRGGBB value for lv_color_hex(), falling back to ARC_DEFAULT_COLOR if
// empty or malformed.
inline uint32_t arc_color_or_default(const std::string &hex) {
  if (hex.size() != 6) return ARC_DEFAULT_COLOR;
  char *end = nullptr;
  uint32_t v = (uint32_t) strtoul(hex.c_str(), &end, 16);
  return (end == hex.c_str() + 6) ? v : ARC_DEFAULT_COLOR;
}

// Parses a 6-digit hex RGB string (as stored in QueuedNotify::color, no '#')
// into 0..1 float channels for light.turn_on. Invalid/short input falls
// back to green, matching parse_notify_entry()'s default.
inline LedColor hex_to_led_color(const std::string &hex) {
  if (hex.size() != 6) return {0.0f, 1.0f, 0.0f};
  auto nibble = [](char c) -> int {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0;
  };
  int r = (nibble(hex[0]) << 4) | nibble(hex[1]);
  int g = (nibble(hex[2]) << 4) | nibble(hex[3]);
  int b = (nibble(hex[4]) << 4) | nibble(hex[5]);
  return {r / 255.0f, g / 255.0f, b / 255.0f};
}

// ------------------------------------------------------------------
// Timer queue: same internal-storage/add-button/remove-button-by-id design
// as the Notify queue above (id(timer_queue), a plain global). Unlike
// Notify, entries are plain FIFO (sorted by seq only, no priority) and
// don't self-delete when they finish - they stay in the queue, blinking/
// counting as "finished", until removed (on-device cancel-confirm click or
// the HA remove button). `end` is required (not optional like Notify's).
//
// Entries can come from HA (id=...;title=...;end=...;color=RRGGBB;
// led_mode=0..2;value_blink=0|1;arc_color=RRGGBB, same key=value format as
// Notify - arc_color is optional, empty keeps the default purple arc) or be
// created directly on the device via the Timer screen's last swipe page
// (see timer_click) - those get an ESP-generated id and their color/
// led_mode/value_blink/arc_color come from the Timer settings-menu entries
// instead of being specified explicitly.
// ------------------------------------------------------------------
struct QueuedTimer {
  std::string entry_id, title;
  std::string end_time;  // "YYYY-MM-DD HH:MM:SS" or "HH:MM:SS", required
  std::string color;     // 6-digit hex RGB, no '#'
  int led_mode;
  bool value_blink;
  uint32_t seq;
  uint32_t added_epoch;  // Unix timestamp when added - the countdown arc's start point
  bool notified;          // one-shot flag: has the auto-show already fired for this entry?
  std::string arc_color;  // empty = default arc color
};

inline QueuedTimer parse_timer_entry(const std::string &line) {
  QueuedTimer qt;
  qt.color = "00FF00";
  qt.led_mode = 2;
  qt.value_blink = false;
  qt.seq = 0;
  qt.added_epoch = 0;
  qt.notified = false;
  size_t pos = 0;
  while (pos < line.size()) {
    size_t semi = line.find(';', pos);
    std::string token = (semi == std::string::npos) ? line.substr(pos) : line.substr(pos, semi - pos);
    pos = (semi == std::string::npos) ? line.size() : semi + 1;
    size_t eq = token.find('=');
    if (eq == std::string::npos) continue;
    std::string key = token.substr(0, eq);
    std::string value = token.substr(eq + 1);
    if (key == "id") qt.entry_id = value;
    else if (key == "title") qt.title = value;
    else if (key == "end") qt.end_time = value;
    else if (key == "color") {
      if (!value.empty() && value[0] == '#') value = value.substr(1);
      qt.color = value.empty() ? "00FF00" : value;
    } else if (key == "led_mode") {
      qt.led_mode = value.empty() ? 2 : (value[0] - '0');
      if (qt.led_mode < 0 || qt.led_mode > 2) qt.led_mode = 2;
    } else if (key == "value_blink") {
      qt.value_blink = value == "1";
    } else if (key == "seq") {
      qt.seq = (uint32_t) strtoul(value.c_str(), nullptr, 10);
    } else if (key == "added") {
      qt.added_epoch = (uint32_t) strtoul(value.c_str(), nullptr, 10);
    } else if (key == "notified") {
      qt.notified = value == "1";
    } else if (key == "arc_color") {
      if (!value.empty() && value[0] == '#') value = value.substr(1);
      qt.arc_color = value;
    }
  }
  return qt;
}

inline std::vector<QueuedTimer> parse_timer_queue(const std::string &raw) {
  std::vector<QueuedTimer> out;
  size_t pos = 0;
  while (pos < raw.size()) {
    size_t nl = raw.find('\n', pos);
    std::string line = (nl == std::string::npos) ? raw.substr(pos) : raw.substr(pos, nl - pos);
    pos = (nl == std::string::npos) ? raw.size() : nl + 1;
    if (line.empty()) continue;
    QueuedTimer qt = parse_timer_entry(line);
    if (qt.entry_id.empty() || qt.title.empty() || qt.end_time.empty()) continue;
    out.push_back(qt);
  }
  std::sort(out.begin(), out.end(),
            [](const QueuedTimer &a, const QueuedTimer &b) { return a.seq < b.seq; });
  return out;
}

inline std::string serialize_timer_entry(const QueuedTimer &q) {
  char num[16];
  std::string out = "id=" + q.entry_id + ";title=" + q.title + ";end=" + q.end_time + ";color=" + q.color +
                     ";led_mode=";
  snprintf(num, sizeof(num), "%d", q.led_mode);
  out += num;
  out += ";value_blink=";
  out += q.value_blink ? "1" : "0";
  out += ";seq=";
  snprintf(num, sizeof(num), "%lu", (unsigned long) q.seq);
  out += num;
  out += ";added=";
  snprintf(num, sizeof(num), "%lu", (unsigned long) q.added_epoch);
  out += num;
  out += ";notified=";
  out += q.notified ? "1" : "0";
  if (!q.arc_color.empty()) out += ";arc_color=" + q.arc_color;
  return out;
}

// ------------------------------------------------------------------
// Progress queue: same design again, but for a running "N%" bar instead of
// a countdown. `value` (0..100) is the one field HA is expected to keep
// re-sending as work progresses - adding with an `id` that's already in
// the queue replaces that entry in place (upsert) rather than appending a
// duplicate, so "id=dl1;title=...;value=42;..." sent repeatedly just
// updates dl1's percentage. No on-device creation exists for Progress (no
// physical input maps to "how far along is this"), so every field always
// comes from HA - same key=value format as Notify/Timer.
// ------------------------------------------------------------------
struct QueuedProgress {
  std::string entry_id, title;
  float value;  // 0..100
  std::string color;
  int led_mode;
  bool value_blink;
  uint32_t seq;
  bool notified;
  uint32_t finished_at_ms;  // millis() when notified first flipped true - anchors Auto-Home
  std::string arc_color;    // empty = default arc color
};

inline QueuedProgress parse_progress_entry(const std::string &line) {
  QueuedProgress qp;
  qp.value = 0.0f;
  qp.color = "00FF00";
  qp.led_mode = 2;
  qp.value_blink = false;
  qp.seq = 0;
  qp.notified = false;
  qp.finished_at_ms = 0;
  size_t pos = 0;
  while (pos < line.size()) {
    size_t semi = line.find(';', pos);
    std::string token = (semi == std::string::npos) ? line.substr(pos) : line.substr(pos, semi - pos);
    pos = (semi == std::string::npos) ? line.size() : semi + 1;
    size_t eq = token.find('=');
    if (eq == std::string::npos) continue;
    std::string key = token.substr(0, eq);
    std::string value = token.substr(eq + 1);
    if (key == "id") qp.entry_id = value;
    else if (key == "title") qp.title = value;
    else if (key == "value") {
      float v = strtof(value.c_str(), nullptr);
      qp.value = v < 0.0f ? 0.0f : (v > 100.0f ? 100.0f : v);
    }
    else if (key == "color") {
      if (!value.empty() && value[0] == '#') value = value.substr(1);
      qp.color = value.empty() ? "00FF00" : value;
    } else if (key == "led_mode") {
      qp.led_mode = value.empty() ? 2 : (value[0] - '0');
      if (qp.led_mode < 0 || qp.led_mode > 2) qp.led_mode = 2;
    } else if (key == "value_blink") {
      qp.value_blink = value == "1";
    } else if (key == "seq") {
      qp.seq = (uint32_t) strtoul(value.c_str(), nullptr, 10);
    } else if (key == "notified") {
      qp.notified = value == "1";
    } else if (key == "finished_at_ms") {
      qp.finished_at_ms = (uint32_t) strtoul(value.c_str(), nullptr, 10);
    } else if (key == "arc_color") {
      if (!value.empty() && value[0] == '#') value = value.substr(1);
      qp.arc_color = value;
    }
  }
  return qp;
}

inline std::vector<QueuedProgress> parse_progress_queue(const std::string &raw) {
  std::vector<QueuedProgress> out;
  size_t pos = 0;
  while (pos < raw.size()) {
    size_t nl = raw.find('\n', pos);
    std::string line = (nl == std::string::npos) ? raw.substr(pos) : raw.substr(pos, nl - pos);
    pos = (nl == std::string::npos) ? raw.size() : nl + 1;
    if (line.empty()) continue;
    QueuedProgress qp = parse_progress_entry(line);
    if (qp.entry_id.empty() || qp.title.empty()) continue;
    out.push_back(qp);
  }
  std::sort(out.begin(), out.end(),
            [](const QueuedProgress &a, const QueuedProgress &b) { return a.seq < b.seq; });
  return out;
}

inline std::string serialize_progress_entry(const QueuedProgress &q) {
  char num[16];
  std::string out = "id=" + q.entry_id + ";title=" + q.title + ";value=";
  snprintf(num, sizeof(num), "%.1f", q.value);
  out += num;
  out += ";color=" + q.color + ";led_mode=";
  snprintf(num, sizeof(num), "%d", q.led_mode);
  out += num;
  out += ";value_blink=";
  out += q.value_blink ? "1" : "0";
  out += ";seq=";
  snprintf(num, sizeof(num), "%lu", (unsigned long) q.seq);
  out += num;
  out += ";notified=";
  out += q.notified ? "1" : "0";
  out += ";finished_at_ms=";
  snprintf(num, sizeof(num), "%lu", (unsigned long) q.finished_at_ms);
  out += num;
  if (!q.arc_color.empty()) out += ";arc_color=" + q.arc_color;
  return out;
}

// ------------------------------------------------------------------
// Shared status-LED lookup for Timer (ui_context 8) / Progress (9) /
// Notify (10): all three now carry their own led_mode/color per queue
// entry, so update_status_led (thermostat_240.yaml) just needs to know
// which entry is "current" for whichever screen is active. Timer/Progress
// only report a mode once their entry is actually finished (countdown at
// 0 / value at 100) - Notify has no such gate, its LED reflects led_mode
// as soon as it's queued.
// ------------------------------------------------------------------
inline int current_screen_led_mode(int ui_context, const std::string &timer_queue, int timer_index,
                                    const std::string &progress_queue, int progress_index,
                                    const std::string &notify_queue, int notify_index, bool notify_finished,
                                    const esphome::ESPTime &now) {
  if (ui_context == 8) {
    auto items = parse_timer_queue(timer_queue);
    int total = (int) items.size();
    if (timer_index >= total) return 0;
    auto &qt = items[timer_index];
    if (timer_remaining_seconds(qt.end_time, now) > 0) return 0;
    return qt.led_mode;
  }
  if (ui_context == 9) {
    auto items = parse_progress_queue(progress_queue);
    int total = (int) items.size();
    if (progress_index >= total) return 0;
    auto &qp = items[progress_index];
    if (qp.value < 100.0f) return 0;
    return qp.led_mode;
  }
  if (ui_context == 10 && notify_finished) {
    auto items = parse_notify_queue(notify_queue);
    int total = (int) items.size();
    if (total <= 0) return 0;
    int idx = notify_index < 0 ? 0 : (notify_index >= total ? total - 1 : notify_index);
    return items[idx].led_mode;
  }
  return 0;
}

inline LedColor current_screen_led_color(int ui_context, const std::string &timer_queue, int timer_index,
                                          const std::string &progress_queue, int progress_index,
                                          const std::string &notify_queue, int notify_index) {
  if (ui_context == 8) {
    auto items = parse_timer_queue(timer_queue);
    int total = (int) items.size();
    if (timer_index < total) return hex_to_led_color(items[timer_index].color);
  } else if (ui_context == 9) {
    auto items = parse_progress_queue(progress_queue);
    int total = (int) items.size();
    if (progress_index < total) return hex_to_led_color(items[progress_index].color);
  } else if (ui_context == 10) {
    auto items = parse_notify_queue(notify_queue);
    int total = (int) items.size();
    if (total > 0) {
      int idx = notify_index < 0 ? 0 : (notify_index >= total ? total - 1 : notify_index);
      return hex_to_led_color(items[idx].color);
    }
  }
  return {0.0f, 1.0f, 0.0f};
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
// an actual HVACMode value, same set ha_hvac_mode_key() recognizes.
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
