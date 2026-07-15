#pragma once

#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>

#include "esphome/components/api/api_connection.h"
#include "esphome/components/api/api_server.h"
#include "esphome/components/json/json_util.h"
#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/core/string_ref.h"

namespace esphome::ha_cover_controller {

// Home Assistant CoverEntityFeature bitmask (only the bit this controller
// checks is named; the rest of open/close/stop is assumed always available).
constexpr int COVER_FEATURE_SET_POSITION = 4;

struct HACoverState {
  std::string entity_id;
  std::string name;
  std::string state{"unknown"};
  float position{0.0f};
  int supported_features{0};
};

class HACoverController : public Component {
 public:
  void add_cover(const std::string &entity_id, const std::string &name) {
    this->covers_.push_back({entity_id, name});
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
    if (!this->covers_.empty()) this->subscribe_states_();
  }

  void loop() override {
    // Do not mutate APIServer::state_subs_ from a callback that is currently
    // iterating that vector. Register the newly loaded entities afterwards.
    if (!this->state_subscription_pending_) return;
    this->state_subscription_pending_ = false;
    this->subscribe_states_();
    this->announce_state_subscriptions_();
  }

  void subscribe_states_() {
    const uint32_t generation = ++this->state_generation_;
    for (size_t index = 0; index < this->covers_.size(); index++) {
      const std::string entity = this->covers_[index].entity_id;
      this->subscribe_(index, entity, nullptr, Attribute::STATE, generation);
      this->subscribe_(index, entity, "current_position", Attribute::POSITION, generation);
      this->subscribe_(index, entity, "supported_features", Attribute::SUPPORTED_FEATURES, generation);
    }
  }

  void dump_config() override {
    ESP_LOGCONFIG("ha_cover_controller", "Home Assistant cover controller:");
    ESP_LOGCONFIG("ha_cover_controller", "  Config: %s / %s", this->config_entity_id_.c_str(),
                  this->config_attribute_.c_str());
    for (const auto &cover : this->covers_)
      ESP_LOGCONFIG("ha_cover_controller", "  %s (%s)", cover.name.c_str(), cover.entity_id.c_str());
  }

  float get_setup_priority() const override { return setup_priority::AFTER_CONNECTION; }

  size_t size() const { return this->covers_.size(); }
  bool empty() const { return this->covers_.empty(); }
  bool config_loaded() const { return this->config_loaded_; }
  bool config_error() const { return this->config_error_; }

  const HACoverState &get(size_t index) const {
    static const HACoverState EMPTY{};
    return index < this->covers_.size() ? this->covers_[index] : EMPTY;
  }

  HACoverState &get_mutable(size_t index) {
    static HACoverState EMPTY{};
    return index < this->covers_.size() ? this->covers_[index] : EMPTY;
  }

  bool consume_dirty() {
    bool dirty = this->dirty_;
    this->dirty_ = false;
    return dirty;
  }

  bool supports_position(size_t index) const {
    return (this->get(index).supported_features & COVER_FEATURE_SET_POSITION) != 0;
  }

  // Moves the locally cached position by delta * 5 (clamped 0-100). Covers
  // without SET_POSITION support still track this locally so the pill/knob
  // has something to show; send_cover_update decides which HA action the
  // resulting value turns into.
  void adjust(size_t index, int delta) {
    if (index >= this->covers_.size() || delta == 0) return;
    auto &cover = this->covers_[index];
    cover.position = clamp_(cover.position + delta * 5.0f, 0.0f, 100.0f);
    this->dirty_ = true;
  }

  int normalized_value(size_t index) const {
    if (index >= this->covers_.size()) return 0;
    return (int) std::lround(this->covers_[index].position);
  }

 protected:
  enum class Attribute { STATE, POSITION, SUPPORTED_FEATURES };

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
      ESP_LOGW("ha_cover_controller", "Attribute '%s' is not a valid JSON array",
               this->config_attribute_.c_str());
      return;
    }

    std::vector<HACoverState> covers;
    for (JsonObject item : doc.as<JsonArray>()) {
      if (covers.size() >= 16) {
        ESP_LOGW("ha_cover_controller", "Only the first 16 cover entries are loaded");
        break;
      }
      const char *entity_value = item["entity_id"] | "";
      const char *name_value = item["name"] | "";
      const std::string entity_id{entity_value};
      const std::string name{name_value};
      if (entity_id.rfind("cover.", 0) != 0 || entity_id.size() <= 6) {
        ESP_LOGW("ha_cover_controller", "Ignoring invalid cover entity '%s'", entity_id.c_str());
        continue;
      }
      HACoverState cover;
      cover.entity_id = entity_id;
      cover.name = name;
      covers.push_back(std::move(cover));
    }

    this->covers_ = std::move(covers);
    this->last_config_ = value;
    this->config_loaded_ = true;
    this->config_error_ = false;
    this->dirty_ = true;
    this->state_subscription_pending_ = true;
    ESP_LOGI("ha_cover_controller", "Loaded %u cover(s) from %s", (unsigned) this->covers_.size(),
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
    // Re-send the complete subscription list after loading dynamic entities.
    // If multiple controllers load together, the last reset wins and HA
    // receives one list containing every controller's subscriptions.
    for (auto &client : api::global_api_server->active_clients()) {
      if (client != nullptr && client->is_authenticated() && !client->is_marked_for_removal())
        client->on_subscribe_home_assistant_states_request();
    }
  }

  void receive_(size_t index, Attribute type, const std::string &value) {
    if (index >= this->covers_.size()) return;
    auto &cover = this->covers_[index];
    switch (type) {
      case Attribute::STATE:
        cover.state = value;
        break;
      case Attribute::POSITION: {
        float raw = parse_number_(value, NAN);
        if (!std::isnan(raw)) cover.position = clamp_(raw, 0.0f, 100.0f);
        break;
      }
      case Attribute::SUPPORTED_FEATURES: {
        float raw = parse_number_(value, 0.0f);
        cover.supported_features = (int) raw;
        break;
      }
    }
    this->dirty_ = true;
  }

  static float clamp_(float value, float low, float high) {
    return value < low ? low : (value > high ? high : value);
  }

  static float parse_number_(const std::string &value, float fallback) {
    char *end = nullptr;
    float parsed = std::strtof(value.c_str(), &end);
    return end != value.c_str() ? parsed : fallback;
  }

  std::vector<HACoverState> covers_;
  bool dirty_{true};
  std::string config_entity_id_{"sensor.smart_knob_config"};
  std::string config_attribute_{"covers"};
  std::string last_config_;
  uint32_t config_generation_{0};
  uint32_t state_generation_{0};
  bool setup_complete_{false};
  bool state_subscription_pending_{false};
  bool config_loaded_{false};
  bool config_error_{false};
};

}  // namespace esphome::ha_cover_controller
