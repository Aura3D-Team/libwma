#ifndef WMA_SRC_BACKENDS_X11_MOVE_RESIZE_HPP
#define WMA_SRC_BACKENDS_X11_MOVE_RESIZE_HPP

#include "wma/core/Types.hpp"

//! Xlib's tag for Display, so callers need not include Xlib and its macros.
struct _XDisplay;

namespace wma::x11
{

//! Hands a held button to the window manager (EWMH _NET_WM_MOVERESIZE) to move
//! (Caption) or resize (an edge) @p window. Coordinates are root-relative.
bool startMoveResize(_XDisplay *display, unsigned long window, WindowHit hit, int rootX, int rootY,
                     unsigned int button) noexcept;

//! For callers without the press event (GLFW): reads the pointer, and fails
//! unless a button is still held.
bool startMoveResize(_XDisplay *display, unsigned long window, WindowHit hit) noexcept;

} // namespace wma::x11

#endif // WMA_SRC_BACKENDS_X11_MOVE_RESIZE_HPP
