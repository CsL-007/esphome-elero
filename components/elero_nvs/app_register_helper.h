/// @file app_register_helper.h
/// @brief Workaround for ESPHome 2026.x protected Application::register_component_().
///
/// Uses the Kolb/Alexandrescu "access to private/protected members" trick via
/// explicit template instantiation + friend injection. Fully standards-compliant.

#pragma once
#include "esphome/core/application.h"
#include "esphome/core/component.h"
#include <type_traits>

namespace esphome {
namespace elero {
namespace detail {

typedef void (Application::*RegisterImplFn)(Component *, bool);

// Robber: explicit instantiation with fn injects the friend definition
template<RegisterImplFn Fn>
struct AppRegisterRobber {
  friend RegisterImplFn stolen_register_impl_fn(AppRegisterRobber<Fn>) { return Fn; }
};

// ADL lookup helper
struct AppRegisterRobberTag {};

// Explicit instantiation — triggers friend injection for register_component_impl_
// Only done once (in this header, guarded by include guards / ODR).
#ifndef ELERO_APP_REGISTER_ROBBER_INSTANTIATED
#define ELERO_APP_REGISTER_ROBBER_INSTANTIATED
template struct AppRegisterRobber<&Application::register_component_impl_>;
#endif

inline RegisterImplFn get_register_impl_fn() {
  return stolen_register_impl_fn(AppRegisterRobber<&Application::register_component_impl_>{});
}

}  // namespace detail

/// Register a Component with the global App.
/// Works around ESPHome 2026.x making register_component_() protected.
template<typename T>
inline void app_register_component(T *comp) {
  constexpr bool has_loop =
      !std::is_same<decltype(&T::loop), decltype(&Component::loop)>::value;
  (App.*(detail::get_register_impl_fn()))(comp, has_loop);
}

}  // namespace elero
}  // namespace esphome
