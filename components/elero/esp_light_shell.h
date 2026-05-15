/// @file esp_light_shell.h
/// @brief Thin ESPHome light::LightOutput adapter — delegates all state to the Device model.

#pragma once

#ifdef USE_LIGHT
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/light/light_output.h"
#include "esphome/components/light/light_state.h"
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif
#include "device.h"
#include "device_registry.h"
#include "light_sm.h"
#include "state_snapshot.h"  // state_change:: flags
#include "elero_strings.h"   // PERCENT_SCALE
#include "elero_packet.h"
#ifdef USE_BUTTON
#include "refresh_button.h"
#endif

namespace esphome {
namespace elero {

class EspLightShell : public light::LightOutput, public Component {
 public:
  float get_setup_priority() const override { return setup_priority::DATA - 1.0f; }

  // ── Binding (called by NvsAdapter at construction time) ──
  void set_registry(DeviceRegistry *r) { registry_ = r; }
  void set_device(Device *d) { device_ = d; }
  void set_light_state(light::LightState *s) { light_state_ = s; }

  // ── Sensor setters (published from sync_and_publish via snapshot) ──
#ifdef USE_SENSOR
  void set_rssi_sensor(sensor::Sensor *s) { rssi_sensor_ = s; }
#endif
#ifdef USE_TEXT_SENSOR
  void set_status_sensor(text_sensor::TextSensor *s) { status_sensor_ = s; }
#endif
#ifdef USE_BUTTON
  void set_refresh_button(RefreshButton *b) { refresh_button_ = b; }
#endif

  // ── ESPHome Component lifecycle ────────────────────────────
  void setup() override {
#ifdef USE_BUTTON
    if (refresh_button_ && device_) {
      refresh_button_->set_device(device_);
      refresh_button_->set_registry(registry_);
    }
#endif
    if (device_ && device_->active) {
      sync_and_publish(state_change::ALL);
    }
  }

  // ── ESPHome Light interface ────────────────────────────────
  light::LightTraits get_traits() override {
    auto traits = light::LightTraits();
    const auto &cfg = device_->config;
    auto ctx = light_context(cfg);
    traits.set_supported_color_modes({
        light_sm::supports_brightness(ctx)
            ? light::ColorMode::BRIGHTNESS
            : light::ColorMode::ON_OFF
    });
    return traits;
  }

  void write_state(light::LightState *state) override {
    if (!device_ || !device_->is_light() || !registry_ || ignore_write_state_) return;

    float brightness;
    state->current_values_as_brightness(&brightness);
    bool is_on = state->current_values.is_on();

    if (!is_on) {
      registry_->command_light(*device_, packet::command::DOWN);
    } else {
      registry_->set_light_brightness(*device_, brightness);
    }
  }

  void sync_and_publish(uint16_t changes) {
    if (!device_ || !device_->is_light() || !light_state_) return;
    const auto &pub = std::get<LightDevice>(device_->logic).published;

    if (changes & state_change::BRIGHTNESS) {
      ignore_write_state_ = true;
      auto call = light_state_->make_call();
      call.set_state(pub.is_on);
      auto ctx = light_context(device_->config);
      if (pub.is_on && light_sm::supports_brightness(ctx)) {
        call.set_brightness(static_cast<float>(pub.brightness_pct) / PERCENT_SCALE);
      }
      call.perform();
      ignore_write_state_ = false;
    }

#ifdef USE_SENSOR
    if ((changes & state_change::RSSI) && rssi_sensor_ != nullptr)
      rssi_sensor_->publish_state(static_cast<float>(pub.rssi_rounded));
#endif
#ifdef USE_TEXT_SENSOR
    if ((changes & state_change::STATE_STRING) && status_sensor_ != nullptr)
      status_sensor_->publish_state(pub.state_string);
#endif
  }

  DeviceRegistry *registry_{nullptr};
  Device *device_{nullptr};
  light::LightState *light_state_{nullptr};
  bool ignore_write_state_{false};

#ifdef USE_SENSOR
  sensor::Sensor *rssi_sensor_{nullptr};
#endif
#ifdef USE_TEXT_SENSOR
  text_sensor::TextSensor *status_sensor_{nullptr};
#endif
#ifdef USE_BUTTON
  RefreshButton *refresh_button_{nullptr};
#endif
};

}  // namespace elero
}  // namespace esphome

#endif  // USE_LIGHT
