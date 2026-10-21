#ifndef WMA_BACKENDS_WAYLAND_LOADER_HPP
#define WMA_BACKENDS_WAYLAND_LOADER_HPP

#include <wayland-client.h>
#include <wayland-cursor.h>
#ifdef WMA_WAYLAND_HAS_GL
#include <EGL/egl.h>
#include <wayland-egl.h>
#endif
#include <xkbcommon/xkbcommon-compose.h>
#include <xkbcommon/xkbcommon.h>

//! libwayland-client, wayland-cursor, wayland-egl, EGL and xkbcommon are bound at run time, so a machine without
//! one of them loses that feature instead of the program. Call sites include WaylandApi.hpp, not this header.
//!
//! Inside namespace wma the pointers below hide the C functions of the same name; a call outside it fails to link
//! instead of binding the library at load time.

namespace wma
{

inline namespace wayland_dyn
{
#define WMA_WL_CURSOR
#define WMA_WL_WLEGL
#define WMA_WL_EGL
#define WMA_WL_XKB
// NOLINTNEXTLINE(bugprone-macro-parentheses): the argument names a declarator
#define WMA_WL_SYM(name) extern decltype(&::name) name;
#include "backends/wayland/WaylandSymbols.inc"
#undef WMA_WL_SYM
#undef WMA_WL_XKB
#undef WMA_WL_EGL
#undef WMA_WL_WLEGL
#undef WMA_WL_CURSOR
} // namespace wayland_dyn

namespace wayland
{

//! Loads libwayland-client once per process. False when it is missing: there is no Wayland backend here.
[[nodiscard]] bool load() noexcept;

//! The other libraries. Each is loaded on first ask; the calls it provides must stay behind its answer.
[[nodiscard]] bool hasXkb() noexcept;
[[nodiscard]] bool hasCursor() noexcept;
[[nodiscard]] bool hasEgl() noexcept;

} // namespace wayland

} // namespace wma

#endif // WMA_BACKENDS_WAYLAND_LOADER_HPP
