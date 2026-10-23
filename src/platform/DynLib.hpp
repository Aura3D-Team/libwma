#ifndef WMA_PLATFORM_DYN_LIB_HPP
#define WMA_PLATFORM_DYN_LIB_HPP

#include <span>

//! Run-time binding of the system libraries the Linux backends talk to (Xlib, libwayland, ALSA, EGL, ...).
//! None of them is a link-time dependency: a missing library disables the backend that needs it instead of
//! stopping the program before main().

namespace wma::dynlib
{

//! One exported name and the pointer that receives it.
struct Binding
{
    const char *name;
    void **slot;
};

//! Opens the first of `sonames` the system can load, nullptr if none loads. The handle is never closed: GL and X
//! drivers register state that does not survive dlclose(), and the libraries live as long as the process anyway.
[[nodiscard]] void *open(std::span<const char *const> sonames) noexcept;

//! Fills every slot from `lib`. On any miss all slots are cleared and false is returned, so a half-bound table
//! never escapes.
[[nodiscard]] bool bind(void *lib, std::span<const Binding> bindings) noexcept;

[[nodiscard]] void *symbol(void *lib, const char *name) noexcept;

} // namespace wma::dynlib

#endif // WMA_PLATFORM_DYN_LIB_HPP
