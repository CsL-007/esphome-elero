/// @file nvs_adapter.h
/// @brief Creates ESPHome cover/light entities from NVS-restored devices at boot.
///
/// In native_nvs mode (API + NVS), devices are managed via the web UI and
/// persisted in NVS. This adapter creates EspCoverShell / EspLightShell
/// instances from the restored registry slots during setup(), before the
/// API server enumerates entities.

#pragma once

#include "esphome/core/application.h"
#include "esphome/core/component.h"
#include "../elero/device_registry.h"
#include "../elero/device_type.h"
#include "../elero/esp_cover_shell.h"
#include "../elero/esp_light_shell.h"
#include "../elero/output_adapter.h"
#include "esphome/components/light/light_state.h"
#include <array>

namespace esphome {
namespace elero {

class NvsAdapter : public Component, public OutputAdapter {
 public:
  float get_setup_priority() const override {
    // After hub (DATA=600) which restores NVS devices,
    // before API server (AFTER_WIFI=200) which enumerates entities.
    return setup_priority::DATA - 2.0f;
  }

  void set_registry(DeviceRegistry *r) { registry_ = r; }

  void setup(DeviceRegistry &registry) override { registry_ = &registry; }
  void loop() override {}
  void on_device_added(const Device &) override {}
  void on_device_removed(const Device &) override {}
  void on_state_changed(const Device &dev, uint16_t changes) override {
    size_t idx = registry_->slot_index(dev);
    if (dev.is_cover() && cover_shells_[idx] != nullptr) {
      cover_shells_[idx]->sync_and_publish(changes);
    } else if (dev.is_light() && light_shells_[idx] != nullptr) {
      light_shells_[idx]->sync_and_publish(changes);
    }
  }

  void setup() override {
    if (!registry_) return;

    size_t covers = 0, lights = 0;
    for (size_t i = 0; i < DeviceRegistry::max_devices(); ++i) {
      auto *dev = registry_->slot(i);
      if (!dev || !dev->active) continue;
      if (!dev->config.is_enabled()) continue;

      if (dev->is_cover()) {
        create_cover_(dev, i);
        ++covers;
      } else if (dev->is_light()) {
        create_light_(dev, i);
        ++lights;
      }
    }

    ESP_LOGI("nvs_adapter", "Created %zu cover(s) and %zu light(s) from NVS", covers, lights);
  }

 private:
  void create_cover_(Device *dev, size_t slot_index) {
    auto *shell = new EspCoverShell();  // NOLINT — owned by App
    shell->set_name(dev->config.name);
    shell->set_registry(registry_);
    shell->set_device(dev);

    cover_shells_[slot_index] = shell;
    App.register_cover(shell);
    App.register_component(shell);
  }

  void create_light_(Device *dev, size_t slot_index) {
    auto *output = new EspLightShell();  // NOLINT — owned by App
    output->set_registry(registry_);
    output->set_device(dev);

    auto *state = new light::LightState(output);  // NOLINT — owned by App
    state->set_name(dev->config.name);
    state->set_restore_mode(light::LIGHT_RESTORE_DEFAULT_OFF);

    output->set_light_state(state);

    light_shells_[slot_index] = output;
    App.register_light(state);
    App.register_component(state);
    App.register_component(output);
  }

  DeviceRegistry *registry_{nullptr};
  std::array<EspCoverShell *, DeviceRegistry::MAX_DEVICES> cover_shells_{};
  std::array<EspLightShell *, DeviceRegistry::MAX_DEVICES> light_shells_{};
};

}  // namespace elero
}  // namespace esphome
