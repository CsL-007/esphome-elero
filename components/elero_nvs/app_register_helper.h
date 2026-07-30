/// @file app_register_helper.h
/// @brief Workaround for ESPHome 2026.x protected Application::register_component_().
///
/// Uses the canonical "litb" access trick: access control is NOT applied to
/// template arguments of explicit instantiations ([temp.spec] / [class.access]),
/// so the explicit instantiation below may name the protected member. The
/// injected friend function is then found via ADL on the tag type — the
/// protected member is never named outside the exempt context.

#pragma once
#include "esphome/core/application.h"
#include "esphome/core/component.h"
#include <type_traits>

namespace esphome {
namespace elero {
namespace detail {

// Tag describing the stolen member; friend is injected by Rob's instantiation.
struct RegImplTag {
  typedef void (Application::*type)(Component *, bool);
  friend type get_stolen_fn(RegImplTag);  // defined by friend injection below
};

template<typename Tag, typename Tag::type M>
struct Rob {
  friend typename Tag::type get_stolen_fn(Tag) { return M; }
};

// Explicit instantiation — the ONLY place the protected member is named.
// Access checking does not apply here per the C++ standard.
template struct Rob<RegImplTag, &Application::register_component_impl_>;

}  // namespace detail

/// Register a Component with the global App.
/// Works around ESPHome 2026.x making register_component_() protected.
template<typename T>
inline void app_register_component(T *comp) {
  constexpr bool has_loop =
      !std::is_same<decltype(&T::loop), decltype(&Component::loop)>::value;
  (App.*get_stolen_fn(detail::RegImplTag{}))(comp, has_loop);
}

}  // namespace elero
}  // namespace esphome
