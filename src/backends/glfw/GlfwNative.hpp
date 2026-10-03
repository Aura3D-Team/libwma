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

} // namespace wma::glfw

#endif // WMA_SRC_BACKENDS_GLFW_NATIVE_HPP
