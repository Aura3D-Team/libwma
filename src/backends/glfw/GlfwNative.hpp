#ifndef WMA_SRC_BACKENDS_GLFW_NATIVE_HPP
#define WMA_SRC_BACKENDS_GLFW_NATIVE_HPP

#include "wma/core/Types.hpp"

struct GLFWwindow;

//! Kept out of GlfwWindowManager.cpp: glfw3native.h drags in Xlib and its macros.
namespace wma::glfw
{

//! GLFW exposes the handles a native move/resize needs only on X11; on Wayland it
//! hides the xdg_toplevel and the press serial.
[[nodiscard]] bool supportsMoveResize() noexcept;

//! Hands the held button to the window manager to move (Caption) or resize (an edge).
bool startMoveResize(GLFWwindow *window, WindowHit hit) noexcept;

//! Before glfwInit(): where the compositor draws no frame, GLFW draws its own
//! rather than libdecor's, which draws nothing without a plugin installed.
void selectWaylandFrame() noexcept;

//! glfwInit(), preferring X11 when compiled in: GLFW's own default auto-selection
//! can pick Wayland even on a system where X11 works fine, and wma's GLFW backend
//! only has native move/resize (hence ClientSide decoration) on X11 -- see
//! supportsMoveResize(). Falls back to GLFW's normal auto-selection if X11 is not
//! actually available, so a Wayland-only system is unaffected.
[[nodiscard]] bool initPreferringX11() noexcept;

} // namespace wma::glfw

#endif // WMA_SRC_BACKENDS_GLFW_NATIVE_HPP
