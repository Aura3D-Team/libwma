#ifndef WMA_SRC_BACKENDS_WAYLAND_DECORATION_PROBE_HPP
#define WMA_SRC_BACKENDS_WAYLAND_DECORATION_PROBE_HPP

struct wl_display;

namespace wma::wayland
{

//! Whether the compositor offers xdg-decoration, i.e. can draw the frame itself.
//! Dispatches on a private queue, so a display SDL or GLFW owns is safe to probe.
[[nodiscard]] bool offersServerDecorations(wl_display *display) noexcept;

//! Whether libdecor has an actual plugin to draw with. libdecor itself is nearly
//! always present as a shared library, but distros split its GTK/cairo plugin
//! into a separate package; without one libdecor_new() still succeeds but draws
//! nothing. Checks $LIBDECOR_PLUGIN_DIR, falling back to the directory libdecor
//! was built to use (see WMA_LIBDECOR_PLUGIN_DIR).
[[nodiscard]] bool libdecorHasPlugin() noexcept;

} // namespace wma::wayland

#endif // WMA_SRC_BACKENDS_WAYLAND_DECORATION_PROBE_HPP
