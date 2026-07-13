#pragma once

#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>

#include "esphome/components/api/api_server.h"
#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/core/string_ref.h"

namespace esphome::ha_climate_controller {

struct HAClimateState {
  std::string entity_id;
  std::string name;
  std::string state{"unknown"};
  std::string last_hvac_mode{"heat"};
  std::string hvac_action;
  std::string hvac_modes;
  std::string preset_modes;
  std::string preset_mode;
  float current_temperature{NAN};
  float target_temperature{NAN};
  float target_low{NAN};
  float target_high{NAN};
  float humidity{NAN};
  int adjust_target{0};
  bool dirty{true};
};

class HAClimateController : public Component {
 public:
  void add_climate(const std::string &entity_id, const std::string &name) {
    HAClimateState climate;
    climate.entity_id = entity_id;
    climate.name = name;
    this->climates_.push_back(climate);
  }

  void setup() override {
    for (size_t index = 0; index < this->climates_.size(); index++) {
      const std::string entity = this->climates_[index].entity_id;
      this->subscribe_(index, entity, nullptr, Attribute::STATE);
      this->subscribe_(index, entity, "hvac_action", Attribute::HVAC_ACTION);
      this->subscribe_(index, entity, "hvac_modes", Attribute::HVAC_MODES);
      this->subscribe_(index, entity, "preset_modes", Attribute::PRESET_MODES);
      this->subscribe_(index, entity, "preset_mode", Attribute::PRESET_MODE);
      this->subscribe_(index, entity, "current_temperature", Attribute::CURRENT_TEMPERATURE);
      this->subscribe_(index, entity, "temperature", Attribute::TARGET_TEMPERATURE);
      this->subscribe_(index, entity, "target_temp_low", Attribute::TARGET_LOW);
      this->subscribe_(index, entity, "target_temp_high", Attribute::TARGET_HIGH);
      this->subscribe_(index, entity, "humidity", Attribute::HUMIDITY);
      this->subscribe_(index, entity, "current_humidity", Attribute::HUMIDITY);
    }
  }

  void dump_config() override {
    ESP_LOGCONFIG("ha_climate_controller", "Home Assistant climate controller:");
    for (const auto &climate : this->climates_)
      ESP_LOGCONFIG("ha_climate_controller", "  %s (%s)", climate.name.c_str(),
                    climate.entity_id.c_str());
  }

  float get_setup_priority() const override { return setup_priority::AFTER_CONNECTION; }

  size_t size() const { return this->climates_.size(); }
  bool empty() const { return this->climates_.empty(); }

  const HAClimateState &get(size_t index) const {
    static const HAClimateState EMPTY{};
    return index < this->climates_.size() ? this->climates_[index] : EMPTY;
  }

  HAClimateState &get_mutable(size_t index) {
    static HAClimateState EMPTY{};
    return index < this->climates_.size() ? this->climates_[index] : EMPTY;
  }

  bool consume_dirty(size_t index) {
    if (index >= this->climates_.size()) return false;
    bool dirty = this->climates_[index].dirty;
    this->climates_[index].dirty = false;
    return dirty;
  }

 protected:
  enum class Attribute {
    STATE,
    HVAC_ACTION,
    HVAC_MODES,
    PRESET_MODES,
    PRESET_MODE,
    CURRENT_TEMPERATURE,
    TARGET_TEMPERATURE,
    TARGET_LOW,
    TARGET_HIGH,
    HUMIDITY
  };

  void subscribe_(size_t index, const std::string &entity, const char *attribute, Attribute type) {
    optional<std::string> dynamic_attribute;
    if (attribute != nullptr) dynamic_attribute = std::string(attribute);
    api::global_api_server->subscribe_home_assistant_state(
        entity, dynamic_attribute,
        [this, index, type](StringRef state) { this->receive_(index, type, state.str()); });
  }

  void receive_(size_t index, Attribute type, const std::string &value) {
    if (index >= this->climates_.size()) return;
    auto &climate = this->climates_[index];
    switch (type) {
      case Attribute::STATE:
        climate.state = value;
        if (!value.empty() && value != "off" && value != "unknown" && value != "unavailable")
          climate.last_hvac_mode = value;
        break;
      case Attribute::HVAC_ACTION: climate.hvac_action = value; break;
      case Attribute::HVAC_MODES: climate.hvac_modes = value; break;
      case Attribute::PRESET_MODES: climate.preset_modes = value; break;
      case Attribute::PRESET_MODE: climate.preset_mode = value; break;
      case Attribute::CURRENT_TEMPERATURE:
        climate.current_temperature = parse_number_(value, climate.current_temperature);
        break;
      case Attribute::TARGET_TEMPERATURE:
        climate.target_temperature = parse_number_(value, climate.target_temperature);
        break;
      case Attribute::TARGET_LOW: climate.target_low = parse_number_(value, NAN); break;
      case Attribute::TARGET_HIGH: climate.target_high = parse_number_(value, NAN); break;
      case Attribute::HUMIDITY:
        climate.humidity = parse_number_(value, climate.humidity);
        break;
    }
    climate.dirty = true;
  }

  static float parse_number_(const std::string &value, float fallback) {
    if (value.empty() || value == "unknown" || value == "unavailable" || value == "None")
      return fallback;
    char *end = nullptr;
    float parsed = std::strtof(value.c_str(), &end);
    return end != value.c_str() ? parsed : fallback;
  }

  std::vector<HAClimateState> climates_;
};

}  // namespace esphome::ha_climate_controller
