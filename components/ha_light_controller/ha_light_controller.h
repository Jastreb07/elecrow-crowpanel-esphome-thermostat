#pragma once

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>

#include "esphome/components/api/api_server.h"
#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/core/string_ref.h"

namespace esphome::ha_light_controller {

enum LightControlMode : int { LIGHT_BRIGHTNESS = 0, LIGHT_TEMPERATURE = 1, LIGHT_COLOR = 2 };

struct HALightState {
  std::string entity_id;
  std::string name;
  std::string state{"unknown"};
  std::string supported_color_modes;
  float brightness{100.0f};
  float color_temp_kelvin{3000.0f};
  float min_color_temp_kelvin{2000.0f};
  float max_color_temp_kelvin{6500.0f};
  float hue{0.0f};
  float saturation{100.0f};
  int control_mode{LIGHT_BRIGHTNESS};
};

class HALightController : public Component {
 public:
  void add_light(const std::string &entity_id, const std::string &name) {
    this->lights_.push_back({entity_id, name});
  }

  void setup() override {
    for (size_t index = 0; index < this->lights_.size(); index++) {
      const std::string entity = this->lights_[index].entity_id;
      this->subscribe_(index, entity, nullptr, Attribute::STATE);
      this->subscribe_(index, entity, "supported_color_modes", Attribute::SUPPORTED_MODES);
      this->subscribe_(index, entity, "brightness", Attribute::BRIGHTNESS);
      this->subscribe_(index, entity, "color_temp_kelvin", Attribute::COLOR_TEMP);
      this->subscribe_(index, entity, "min_color_temp_kelvin", Attribute::MIN_COLOR_TEMP);
      this->subscribe_(index, entity, "max_color_temp_kelvin", Attribute::MAX_COLOR_TEMP);
      this->subscribe_(index, entity, "hs_color", Attribute::HS_COLOR);
    }
  }

  void dump_config() override {
    ESP_LOGCONFIG("ha_light_controller", "Home Assistant light controller:");
    for (const auto &light : this->lights_)
      ESP_LOGCONFIG("ha_light_controller", "  %s (%s)", light.name.c_str(), light.entity_id.c_str());
  }

  float get_setup_priority() const override { return setup_priority::AFTER_CONNECTION; }

  size_t size() const { return this->lights_.size(); }
  bool empty() const { return this->lights_.empty(); }

  const HALightState &get(size_t index) const {
    static const HALightState EMPTY{};
    return index < this->lights_.size() ? this->lights_[index] : EMPTY;
  }

  HALightState &get_mutable(size_t index) {
    static HALightState EMPTY{};
    return index < this->lights_.size() ? this->lights_[index] : EMPTY;
  }

  bool consume_dirty() {
    bool dirty = this->dirty_;
    this->dirty_ = false;
    return dirty;
  }

  bool supports_brightness(size_t index) const {
    const auto &light = this->get(index);
    return !contains_mode_(light.supported_color_modes, "onoff") || light.supported_color_modes.empty();
  }

  bool supports_temperature(size_t index) const {
    return contains_mode_(this->get(index).supported_color_modes, "color_temp");
  }

  bool supports_color(size_t index) const {
    const std::string &modes = this->get(index).supported_color_modes;
    return contains_mode_(modes, "hs") || contains_mode_(modes, "rgb") ||
           contains_mode_(modes, "rgbw") || contains_mode_(modes, "rgbww") ||
           contains_mode_(modes, "xy");
  }

  int ensure_mode(size_t index) {
    auto &light = this->get_mutable(index);
    if (this->mode_available_(index, light.control_mode)) return light.control_mode;
    for (int mode = LIGHT_BRIGHTNESS; mode <= LIGHT_COLOR; mode++) {
      if (this->mode_available_(index, mode)) {
        light.control_mode = mode;
        return mode;
      }
    }
    light.control_mode = LIGHT_BRIGHTNESS;
    return light.control_mode;
  }

  int cycle_mode(size_t index) {
    auto &light = this->get_mutable(index);
    int current = this->ensure_mode(index);
    for (int offset = 1; offset <= 3; offset++) {
      int candidate = (current + offset) % 3;
      if (this->mode_available_(index, candidate)) {
        light.control_mode = candidate;
        this->dirty_ = true;
        return candidate;
      }
    }
    return current;
  }

  void adjust(size_t index, int delta) {
    if (index >= this->lights_.size() || delta == 0) return;
    auto &light = this->lights_[index];
    int mode = this->ensure_mode(index);
    if (mode == LIGHT_BRIGHTNESS) {
      light.brightness = clamp_(light.brightness + delta * 5.0f, 1.0f, 100.0f);
    } else if (mode == LIGHT_TEMPERATURE) {
      float low = std::min(light.min_color_temp_kelvin, light.max_color_temp_kelvin);
      float high = std::max(light.min_color_temp_kelvin, light.max_color_temp_kelvin);
      light.color_temp_kelvin = clamp_(light.color_temp_kelvin + delta * 100.0f, low, high);
    } else {
      // The visible color selector is a 270° arc, not a full hue wheel.
      // Move along its actual magenta -> red -> green -> blue scale and stop
      // at both physical ends instead of wrapping around.
      float position = hue_to_arc_position_(light.hue);
      position = clamp_(position + delta * 0.02f, 0.0f, 1.0f);
      light.hue = arc_position_to_hue_(position);
      // Kelvin/white modes commonly report a very low saturation. Never
      // carry that value into color control or the selected color is washed
      // out. The color arc always represents fully saturated colors.
      light.saturation = 100.0f;
    }
    this->dirty_ = true;
  }

  int normalized_value(size_t index) {
    if (index >= this->lights_.size()) return 0;
    const auto &light = this->lights_[index];
    // Never display stale brightness/Kelvin/color positions while the light
    // is off or unavailable. Keep the cached values only for the next HA sync.
    if (light.state != "on") return 0;
    int mode = this->ensure_mode(index);
    if (mode == LIGHT_BRIGHTNESS) return (int) std::lround(light.brightness);
    if (mode == LIGHT_TEMPERATURE) {
      float low = std::min(light.min_color_temp_kelvin, light.max_color_temp_kelvin);
      float high = std::max(light.min_color_temp_kelvin, light.max_color_temp_kelvin);
      if (high <= low) return 0;
      return (int) std::lround((light.color_temp_kelvin - low) / (high - low) * 100.0f);
    }
    return (int) std::lround(hue_to_arc_position_(light.hue) * 100.0f);
  }

 protected:
  enum class Attribute { STATE, SUPPORTED_MODES, BRIGHTNESS, COLOR_TEMP, MIN_COLOR_TEMP, MAX_COLOR_TEMP, HS_COLOR };

  void subscribe_(size_t index, const std::string &entity, const char *attribute, Attribute type) {
    optional<std::string> dynamic_attribute;
    if (attribute != nullptr) dynamic_attribute = std::string(attribute);
    api::global_api_server->subscribe_home_assistant_state(
        entity, dynamic_attribute,
        [this, index, type](StringRef state) { this->receive_(index, type, state.str()); });
  }

  void receive_(size_t index, Attribute type, const std::string &value) {
    if (index >= this->lights_.size()) return;
    auto &light = this->lights_[index];
    switch (type) {
      case Attribute::STATE:
        light.state = value;
        break;
      case Attribute::SUPPORTED_MODES:
        light.supported_color_modes = value;
        this->ensure_mode(index);
        break;
      case Attribute::BRIGHTNESS: {
        float raw = parse_number_(value, NAN);
        if (!std::isnan(raw)) light.brightness = clamp_(raw / 255.0f * 100.0f, 1.0f, 100.0f);
        break;
      }
      case Attribute::COLOR_TEMP:
        light.color_temp_kelvin = parse_number_(value, light.color_temp_kelvin);
        break;
      case Attribute::MIN_COLOR_TEMP:
        light.min_color_temp_kelvin = parse_number_(value, light.min_color_temp_kelvin);
        break;
      case Attribute::MAX_COLOR_TEMP:
        light.max_color_temp_kelvin = parse_number_(value, light.max_color_temp_kelvin);
        break;
      case Attribute::HS_COLOR: {
        auto values = parse_numbers_(value);
        if (!values.empty()) light.hue = clamp_(values[0], 0.0f, 360.0f);
        if (values.size() > 1) light.saturation = clamp_(values[1], 0.0f, 100.0f);
        break;
      }
    }
    this->dirty_ = true;
  }

  bool mode_available_(size_t index, int mode) const {
    if (mode == LIGHT_BRIGHTNESS) return this->supports_brightness(index);
    if (mode == LIGHT_TEMPERATURE) return this->supports_temperature(index);
    if (mode == LIGHT_COLOR) return this->supports_color(index);
    return false;
  }

  static bool contains_mode_(const std::string &modes, const char *mode) {
    std::string quoted = std::string("'") + mode + "'";
    std::string double_quoted = std::string("\"") + mode + "\"";
    return modes.find(quoted) != std::string::npos || modes.find(double_quoted) != std::string::npos;
  }

  static float clamp_(float value, float low, float high) {
    return value < low ? low : (value > high ? high : value);
  }

  // Map the visible 270° color scale to Home Assistant hue. The reference UI
  // begins with magenta (315°) and walks forward through red, yellow, green
  // and cyan to blue (240°). The omitted violet range is snapped to the
  // nearest physical end when an external HA update selects it.
  static float hue_to_arc_position_(float hue) {
    hue = std::fmod(hue, 360.0f);
    if (hue < 0.0f) hue += 360.0f;
    if (hue >= 315.0f) return (hue - 315.0f) / 285.0f;
    if (hue <= 240.0f) return (hue + 45.0f) / 285.0f;
    return hue < 277.5f ? 1.0f : 0.0f;
  }

  static float arc_position_to_hue_(float position) {
    float hue = 315.0f + clamp_(position, 0.0f, 1.0f) * 285.0f;
    return std::fmod(hue, 360.0f);
  }

  static float parse_number_(const std::string &value, float fallback) {
    char *end = nullptr;
    float parsed = std::strtof(value.c_str(), &end);
    return end != value.c_str() ? parsed : fallback;
  }

  static std::vector<float> parse_numbers_(const std::string &value) {
    std::vector<float> values;
    const char *cursor = value.c_str();
    while (*cursor != '\0') {
      char *end = nullptr;
      float parsed = std::strtof(cursor, &end);
      if (end != cursor) {
        values.push_back(parsed);
        cursor = end;
      } else {
        cursor++;
      }
    }
    return values;
  }

  std::vector<HALightState> lights_;
  bool dirty_{true};
};

}  // namespace esphome::ha_light_controller
