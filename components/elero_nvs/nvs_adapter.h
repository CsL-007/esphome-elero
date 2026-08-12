/// @file nvs_adapter.h
/// @brief Creates ESPHome cover/light entities and native API services from NVS-restored devices at boot.

#pragma once

#include "esphome/components/api/custom_api_device.h"
#include "esphome/components/light/light_state.h"
#include "esphome/core/application.h"
#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "../elero/device_registry.h"
#include "../elero/device_type.h"
#include "../elero/esp_cover_shell.h"
#include "../elero/esp_light_shell.h"
#include "../elero/output_adapter.h"
#include "app_register_helper.h"
#include <array>

namespace esphome {
namespace elero {

class NvsAdapter : public Component, public OutputAdapter, public api::CustomAPIDevice {
 public:
  float get_setup_priority() const override {
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
      if (!dev || !dev->active || !dev->config.is_enabled()) continue;

      if (dev->is_cover()) {
        create_cover_(dev, i);
        ++covers;
      } else if (dev->is_light()) {
        create_light_(dev, i);
        ++lights;
      }
    }

#ifdef USE_API_CUSTOM_SERVICES
    register_service(&NvsAdapter::on_long_up_, "elero_long_up", {"address"});
    register_service(&NvsAdapter::on_long_down_, "elero_long_down", {"address"});
    register_service(&NvsAdapter::on_intermediate_, "elero_intermediate", {"address"});
    register_service(&NvsAdapter::on_tilt_, "elero_tilt", {"address"});
    register_service(&NvsAdapter::on_check_, "elero_check", {"address"});
    ESP_LOGI("nvs_adapter", "Registered Elero native API services");
#else
    ESP_LOGW("nvs_adapter", "Elero native API services disabled; set api.custom_services: true");
#endif

    ESP_LOGI("nvs_adapter", "Created %zu cover(s) and %zu light(s) from NVS", covers, lights);
  }

 private:
  static constexpr const char *TAG = "nvs_adapter";

  Device *find_enabled_cover_(int address, const char *service) {
    if (!registry_ || address < 0) {
      ESP_LOGW(TAG, "%s: invalid address", service);
      return nullptr;
    }
    auto *dev = registry_->find(static_cast<uint32_t>(address));
    if (!dev || !dev->is_cover() || !dev->config.is_enabled()) {
      ESP_LOGW(TAG, "%s: enabled cover 0x%06x not found", service,
               static_cast<uint32_t>(address));
      return nullptr;
    }
    return dev;
  }

  void send_special_cover_command_(int address, uint8_t command, const char *service) {
    auto *dev = find_enabled_cover_(address, service);
    if (!dev) return;
    if (!registry_->command_raw_button(static_cast<uint32_t>(address), command)) {
      ESP_LOGW(TAG, "%s: failed to queue command 0x%02x", service, command);
      return;
    }
    registry_->request_check(*dev);
  }

  void on_long_up_(int address) { send_special_cover_command_(address, 0x21, "elero_long_up"); }
  void on_long_down_(int address) { send_special_cover_command_(address, 0x41, "elero_long_down"); }
  void on_intermediate_(int address) { send_special_cover_command_(address, 0x44, "elero_intermediate"); }

  void on_tilt_(int address) {
    auto *dev = find_enabled_cover_(address, "elero_tilt");
    if (dev) registry_->command_cover_tilt(*dev);
  }

  void on_check_(int address) {
    auto *dev = find_enabled_cover_(address, "elero_check");
    if (dev) registry_->request_check(*dev);
  }

  void create_cover_(Device *dev, size_t slot_index) {
    auto *shell = new EspCoverShell();
    shell->set_registry(registry_);
    shell->set_device(dev);

    cover_shells_[slot_index] = shell;
    App.register_cover(shell, dev->config.name, 0, 0);
    app_register_component(shell);
  }

  void create_light_(Device *dev, size_t slot_index) {
    auto *output = new EspLightShell();
    output->set_registry(registry_);
    output->set_device(dev);

    auto *state = new light::LightState(output);
    state->set_restore_mode(light::LIGHT_RESTORE_DEFAULT_OFF);

    output->set_light_state(state);

    light_shells_[slot_index] = output;
    App.register_light(state, dev->config.name, 0, 0);
    app_register_component(output);
    app_register_component(state);
  }

  DeviceRegistry *registry_{nullptr};
  std::array<EspCoverShell *, DeviceRegistry::MAX_DEVICES> cover_shells_{};
  std::array<EspLightShell *, DeviceRegistry::MAX_DEVICES> light_shells_{};
};

}  // namespace elero
}  // namespace esphome
