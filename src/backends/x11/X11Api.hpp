#ifndef WMA_BACKENDS_X11_API_HPP
#define WMA_BACKENDS_X11_API_HPP

#include <X11/XKBlib.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XInput2.h>
#include <X11/extensions/XShm.h>
#include <X11/extensions/Xpresent.h>
#include <X11/extensions/Xrandr.h>
#ifdef WMA_X11_HAS_GL
#include <GL/glx.h>
#endif

//! Xlib, its extensions and GLX are bound at run time. Inside namespace wma these pointers hide the C functions of
//! the same name, so call sites stay as they are; one outside the namespace fails to link instead of binding the
//! library at load time.

namespace wma
{

inline namespace x11_dyn
{
#define WMA_X11_ALL
// NOLINTNEXTLINE(bugprone-macro-parentheses): the argument names a declarator
#define WMA_X11_SYM(name) extern decltype(&::name) name;
#include "X11Symbols.inc"
#undef WMA_X11_SYM
#undef WMA_X11_ALL
} // namespace x11_dyn

namespace x11
{

//! Loads libX11 once per process. False when it is missing: there is no X11 backend on this machine.
[[nodiscard]] bool load() noexcept;

//! The optional libraries. Each is loaded on first ask; the calls it provides must stay behind its answer.
[[nodiscard]] bool hasShm() noexcept;
[[nodiscard]] bool hasXInput2() noexcept;
[[nodiscard]] bool hasRandr() noexcept;
[[nodiscard]] bool hasPresent() noexcept;
[[nodiscard]] bool hasGlx() noexcept;

} // namespace x11

} // namespace wma

#endif // WMA_BACKENDS_X11_API_HPP
