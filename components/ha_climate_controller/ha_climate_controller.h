#pragma once

#include <cmath>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

#include "esphome/components/api/api_connection.h"
#include "esphome/components/api/api_server.h"
#include "esphome/components/json/json_util.h"
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

  void set_config_entity_id(const std::string &entity_id) {
    if (entity_id.empty() || entity_id == this->config_entity_id_) return;
    this->config_entity_id_ = entity_id;
    this->config_loaded_ = false;
    this->config_error_ = false;
    this->last_config_.clear();
    this->config_generation_++;
    if (this->setup_complete_) this->subscribe_config_();
  }

  void set_config_attribute(const std::string &attribute) { this->config_attribute_ = attribute; }

  void setup() override {
    this->setup_complete_ = true;
    if (!this->config_entity_id_.empty()) this->subscribe_config_();
    if (!this->climates_.empty()) this->subscribe_states_();
  }

  void loop() override {
    // Home Assistant state callbacks run while ESPHome iterates state_subs_.
    // Adding subscriptions from inside that callback may reallocate the vector
    // and invalidate ESPHome's active iterator. Defer it until the API handler
    // has returned to the normal component loop.
    if (!this->state_subscription_pending_) return;
    this->state_subscription_pending_ = false;
    this->subscribe_states_();
    this->announce_state_subscriptions_();
  }

  void subscribe_states_() {
    const uint32_t generation = ++this->state_generation_;
    for (size_t index = 0; index < this->climates_.size(); index++) {
      const std::string entity = this->climates_[index].entity_id;
      this->subscribe_(index, entity, nullptr, Attribute::STATE, generation);
      this->subscribe_(index, entity, "hvac_action", Attribute::HVAC_ACTION, generation);
      this->subscribe_(index, entity, "hvac_modes", Attribute::HVAC_MODES, generation);
      this->subscribe_(index, entity, "preset_modes", Attribute::PRESET_MODES, generation);
      this->subscribe_(index, entity, "preset_mode", Attribute::PRESET_MODE, generation);
      this->subscribe_(index, entity, "current_temperature", Attribute::CURRENT_TEMPERATURE, generation);
      this->subscribe_(index, entity, "temperature", Attribute::TARGET_TEMPERATURE, generation);
      this->subscribe_(index, entity, "target_temp_low", Attribute::TARGET_LOW, generation);
      this->subscribe_(index, entity, "target_temp_high", Attribute::TARGET_HIGH, generation);
      this->subscribe_(index, entity, "humidity", Attribute::HUMIDITY, generation);
      this->subscribe_(index, entity, "current_humidity", Attribute::HUMIDITY, generation);
    }
  }

  void dump_config() override {
    ESP_LOGCONFIG("ha_climate_controller", "Home Assistant climate controller:");
    ESP_LOGCONFIG("ha_climate_controller", "  Config: %s / %s", this->config_entity_id_.c_str(),
                  this->config_attribute_.c_str());
    for (const auto &climate : this->climates_)
      ESP_LOGCONFIG("ha_climate_controller", "  %s (%s)", climate.name.c_str(),
                    climate.entity_id.c_str());
  }

  float get_setup_priority() const override { return setup_priority::AFTER_CONNECTION; }

  size_t size() const { return this->climates_.size(); }
  bool empty() const { return this->climates_.empty(); }
  bool config_loaded() const { return this->config_loaded_; }
  bool config_error() const { return this->config_error_; }

  const HAClimateState &get(size_t index) const {
    static const HAClimateState EMPTY{};
    return index < this->climates_.size() ? this->climates_[index] : EMPTY;
  }

  HAClimateState &get_mutable(size_t index) {
    static HAClimateState EMPTY{};
    return index < this->climates_.size() ? this->climates_[index] : EMPTY;
  }

  bool consume_dirty(size_t index) {
    if (this->config_dirty_) {
      this->config_dirty_ = false;
      return true;
    }
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

  void subscribe_config_() {
    const uint32_t generation = this->config_generation_;
    api::global_api_server->subscribe_home_assistant_state(
        this->config_entity_id_, optional<std::string>(this->config_attribute_),
        [this, generation](StringRef state) {
          if (generation == this->config_generation_) this->apply_config_(state.str());
        });
  }

  void apply_config_(const std::string &value) {
    if (value == this->last_config_) return;
    std::string payload = value;
    if (payload.rfind("json:", 0) == 0) payload.erase(0, 5);
    auto doc = json::parse_json(payload);
    if (!doc.is<JsonArray>()) {
      this->config_error_ = true;
      ESP_LOGW("ha_climate_controller", "Attribute '%s' is not a valid JSON array",
               this->config_attribute_.c_str());
      return;
    }

    std::vector<HAClimateState> climates;
    for (JsonObject item : doc.as<JsonArray>()) {
      if (climates.size() >= 16) {
        ESP_LOGW("ha_climate_controller", "Only the first 16 climate entries are loaded");
        break;
      }
      const char *entity_value = item["entity_id"] | "";
      const char *name_value = item["name"] | "";
      const std::string entity_id{entity_value};
      const std::string name{name_value};
      if (entity_id.rfind("climate.", 0) != 0 || entity_id.size() <= 8) {
        ESP_LOGW("ha_climate_controller", "Ignoring invalid climate entity '%s'", entity_id.c_str());
        continue;
      }
      HAClimateState climate;
      climate.entity_id = entity_id;
      climate.name = name;
      climates.push_back(std::move(climate));
    }

    this->climates_ = std::move(climates);
    this->last_config_ = value;
    this->config_loaded_ = true;
    this->config_error_ = false;
    this->config_dirty_ = true;
    this->state_subscription_pending_ = true;
    ESP_LOGI("ha_climate_controller", "Loaded %u climate(s) from %s", (unsigned) this->climates_.size(),
             this->config_entity_id_.c_str());
  }

  void subscribe_(size_t index, const std::string &entity, const char *attribute, Attribute type,
                  uint32_t generation) {
    optional<std::string> dynamic_attribute;
    if (attribute != nullptr) dynamic_attribute = std::string(attribute);
    api::global_api_server->subscribe_home_assistant_state(
        entity, dynamic_attribute,
        [this, index, type, generation](StringRef state) {
          if (generation == this->state_generation_) this->receive_(index, type, state.str());
        });
  }

  void announce_state_subscriptions_() {
    // Home Assistant requests the subscription list once when the API connects.
    // Dynamic entries loaded afterwards must be announced to that same client.
    for (auto &client : api::global_api_server->active_clients()) {
      if (client != nullptr && client->is_authenticated() && !client->is_marked_for_removal())
        client->on_subscribe_home_assistant_states_request();
    }
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
  std::string config_entity_id_{"sensor.smart_knob_config"};
  std::string config_attribute_{"climates"};
  std::string last_config_;
  uint32_t config_generation_{0};
  uint32_t state_generation_{0};
  bool setup_complete_{false};
  bool state_subscription_pending_{false};
  bool config_dirty_{true};
  bool config_loaded_{false};
  bool config_error_{false};
};

}  // namespace esphome::ha_climate_controller
