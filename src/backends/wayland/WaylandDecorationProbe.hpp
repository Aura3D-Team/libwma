#ifndef WMA_SRC_BACKENDS_WAYLAND_DECORATION_PROBE_HPP
#define WMA_SRC_BACKENDS_WAYLAND_DECORATION_PROBE_HPP

struct wl_display;

namespace wma::wayland
{

//! Whether the compositor offers xdg-decoration, i.e. can draw the frame itself.
//! Dispatches on a private queue, so a display SDL or GLFW owns is safe to probe.
[[nodiscard]] bool offersServerDecorations(wl_display *display) noexcept;

} // namespace wma::wayland

#endif // WMA_SRC_BACKENDS_WAYLAND_DECORATION_PROBE_HPP
