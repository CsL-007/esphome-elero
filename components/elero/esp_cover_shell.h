/// @file esp_cover_shell.h
/// @brief Thin ESPHome cover::Cover adapter — delegates all state to the Device model.
///
/// Owns NO cover state. Reads position/operation from the cover state machine.
/// Constructed by `NvsAdapter` at boot from a pre-restored DeviceRegistry slot.
/// Devices are no longer YAML-defined (RFC-002) — they live in NVS.

#pragma once

#ifdef USE_COVER
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/cover/cover.h"
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#include "device.h"
#include "device_registry.h"
#include "cover_sm.h"
#include "state_snapshot.h"  // state_change:: flags
#include "elero_strings.h"   // PERCENT_SCALE
#include "elero_packet.h"
#ifdef USE_BUTTON
#include "refresh_button.h"
#endif

namespace esphome {
namespace elero {

class EspCoverShell : public cover::Cover, public Component {
 public:
  float get_setup_priority() const override { return setup_priority::DATA - 1.0f; }

  // ── Setup (called by NvsAdapter when binding to a registry slot) ──
  void set_registry(DeviceRegistry *r) { registry_ = r; }
  void set_slot_index(int idx) { slot_index_ = idx; }
  // The following pre-seed get_traits() fallbacks for the brief window
  // between construction and setup() binding the registry slot. After
  // setup() runs, traits read directly from device_->config.
  void set_supports_tilt(bool v) { trait_hints_.supports_tilt = v ? 1 : 0; }
  void set_open_duration(uint32_t v) { trait_hints_.open_duration_ms = v; }
  void set_close_duration(uint32_t v) { trait_hints_.close_duration_ms = v; }

  // ── Sensor setters (all published from sync_and_publish_ via snapshot) ──
#ifdef USE_SENSOR
  void set_rssi_sensor(sensor::Sensor *s) { rssi_sensor_ = s; }
#endif
#ifdef USE_TEXT_SENSOR
  void set_status_sensor(text_sensor::TextSensor *s) { status_sensor_ = s; }
  void set_command_source_sensor(text_sensor::TextSensor *s) { command_source_sensor_ = s; }
  void set_problem_type_sensor(text_sensor::TextSensor *s) { problem_type_sensor_ = s; }
#endif
#ifdef USE_BINARY_SENSOR
  void set_problem_sensor(binary_sensor::BinarySensor *s) { problem_sensor_ = s; }
#endif
#ifdef USE_BUTTON
  void set_refresh_button(RefreshButton *b) { refresh_button_ = b; }
#endif

  // ── ESPHome Component lifecycle ────────────────────────────
  void setup() override {
    if (!registry_ || slot_index_ < 0) return;
    device_ = registry_->slot(static_cast<size_t>(slot_index_));
#ifdef USE_BUTTON
    if (refresh_button_ && device_) {
      refresh_button_->set_device(device_);
      refresh_button_->set_registry(registry_);
    }
#endif
  }

  void loop() override {
    if (!device_ || !device_->active) return;
    // Initial publish: ensure HA doesn't show "Unknown" after boot
    if (!initial_published_) {
      initial_published_ = true;
      sync_and_publish_(state_change::ALL);
    }
    // Publish when registry signals a state change (notify timestamp updated)
    if (device_->last_notify_ms > last_published_ms_) {
      sync_and_publish_(device_->last_changes);
      last_published_ms_ = device_->last_notify_ms;
    }
  }

  // ── ESPHome Cover interface ────────────────────────────────
  cover::CoverTraits get_traits() override {
    auto traits = cover::CoverTraits();
    // After setup() binds the slot, prefer the registry's authoritative config.
    // Before binding (very early in boot) fall back to the trait hints supplied
    // at construction time by NvsAdapter.
    const auto &cfg = (device_ && device_->active) ? device_->config : trait_hints_;
    auto ctx = cover_context(cfg);
    traits.set_supports_position(cover_sm::has_position_tracking(ctx));
    traits.set_supports_tilt(cfg.supports_tilt != 0);
    traits.set_supports_stop(true);
    traits.set_supports_toggle(true);
    traits.set_is_assumed_state(true);
    return traits;
  }

 protected:
  void control(const cover::CoverCall &call) override {
    if (!device_ || !device_->is_cover() || !registry_) return;

    if (call.get_stop()) {
      registry_->command_cover(*device_, packet::command::STOP);
      sync_and_publish_(device_->last_changes);
      return;
    }

    if (call.get_position().has_value()) {
      float target = *call.get_position();
      auto ctx = cover_context(device_->config);
      if (cover_sm::has_position_tracking(ctx) &&
          target > cover_sm::POSITION_CLOSED && target < cover_sm::POSITION_OPEN) {
        registry_->set_cover_position(*device_, target);
      } else {
        uint8_t cmd = (target >= cover_sm::POSITION_OPEN) ? packet::command::UP : packet::command::DOWN;
        registry_->command_cover(*device_, cmd);
      }
      sync_and_publish_(device_->last_changes);
      return;
    }

    if (call.get_tilt().has_value()) {
      registry_->command_cover_tilt(*device_);
      sync_and_publish_(device_->last_changes);
      return;
    }

    if (call.get_toggle().has_value()) {
      auto &cover = std::get<CoverDevice>(device_->logic);
      uint8_t cmd;
      if (cover_sm::is_moving(cover.state)) {
        cmd = packet::command::STOP;
      } else {
        auto ctx = cover_context(device_->config);
        float pos = cover_sm::position(cover.state, millis(), ctx);
        bool was_closing = (cover.last_direction == cover_sm::Operation::CLOSING);
        cmd = (pos <= cover_sm::POSITION_CLOSED || was_closing) ? packet::command::UP : packet::command::DOWN;
      }
      registry_->command_cover(*device_, cmd);
      sync_and_publish_(device_->last_changes);
    }
  }

 private:
  void sync_and_publish_(uint16_t changes) {
    if (!device_ || !device_->is_cover()) return;
    const auto &pub = std::get<CoverDevice>(device_->logic).published;

    if (changes & (state_change::POSITION | state_change::HA_STATE |
                   state_change::OPERATION | state_change::TILT)) {
      this->position = static_cast<float>(pub.position_pct) / PERCENT_SCALE;
      if (device_->config.supports_tilt != 0) {
        this->tilt = pub.tilted ? cover_sm::POSITION_OPEN : cover_sm::POSITION_CLOSED;
      }
      switch (pub.operation) {
        case cover_sm::Operation::IDLE:
          this->current_operation = cover::COVER_OPERATION_IDLE; break;
        case cover_sm::Operation::OPENING:
          this->current_operation = cover::COVER_OPERATION_OPENING; break;
        case cover_sm::Operation::CLOSING:
          this->current_operation = cover::COVER_OPERATION_CLOSING; break;
      }
      this->publish_state();
    }

#ifdef USE_SENSOR
    if ((changes & state_change::RSSI) && rssi_sensor_ != nullptr)
      rssi_sensor_->publish_state(static_cast<float>(pub.rssi_rounded));
#endif
#ifdef USE_TEXT_SENSOR
    if ((changes & state_change::STATE_STRING) && status_sensor_ != nullptr)
      status_sensor_->publish_state(pub.state_string);
    if ((changes & state_change::COMMAND_SOURCE) && command_source_sensor_ != nullptr)
      command_source_sensor_->publish_state(pub.command_source);
    if ((changes & state_change::PROBLEM) && problem_type_sensor_ != nullptr)
      problem_type_sensor_->publish_state(pub.problem_type);
#endif
#ifdef USE_BINARY_SENSOR
    if ((changes & state_change::PROBLEM) && problem_sensor_ != nullptr)
      problem_sensor_->publish_state(pub.is_problem);
#endif
  }

  NvsDeviceConfig trait_hints_{};       ///< Pre-binding fallback for get_traits()
  DeviceRegistry *registry_{nullptr};
  Device *device_{nullptr};
  uint32_t last_published_ms_{0};
  int slot_index_{-1};                  ///< Required: NVS slot bound in setup()
  bool initial_published_{false};

#ifdef USE_SENSOR
  sensor::Sensor *rssi_sensor_{nullptr};
#endif
#ifdef USE_TEXT_SENSOR
  text_sensor::TextSensor *status_sensor_{nullptr};
  text_sensor::TextSensor *command_source_sensor_{nullptr};
  text_sensor::TextSensor *problem_type_sensor_{nullptr};
#endif
#ifdef USE_BINARY_SENSOR
  binary_sensor::BinarySensor *problem_sensor_{nullptr};
#endif
#ifdef USE_BUTTON
  RefreshButton *refresh_button_{nullptr};
#endif
};

}  // namespace elero
}  // namespace esphome

#endif  // USE_COVER
