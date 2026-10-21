#if defined(WMA_ENABLE_X11) || defined(WMA_ENABLE_WAYLAND) || defined(WMA_ENABLE_ALSA)

#include "platform/DynLib.hpp"

#include <dlfcn.h>

namespace wma::dynlib
{

void *open(std::span<const char *const> sonames) noexcept
{
    for (const char *soname : sonames)
    {
        //! RTLD_LOCAL: the libraries' symbols must not leak into the global scope and interpose on a copy the
        //! application or a driver loaded itself.
        if (void *lib = dlopen(soname, RTLD_NOW | RTLD_LOCAL))
            return lib;
    }
    return nullptr;
}

void *symbol(void *lib, const char *name) noexcept
{
    return lib ? dlsym(lib, name) : nullptr;
}

bool bind(void *lib, std::span<const Binding> bindings) noexcept
{
    bool complete = lib != nullptr;
    for (const Binding &binding : bindings)
    {
        *binding.slot = complete ? symbol(lib, binding.name) : nullptr;
        complete = complete && *binding.slot != nullptr;
    }

    if (!complete)
    {
        for (const Binding &binding : bindings)
            *binding.slot = nullptr;
    }
    return complete;
}

} // namespace wma::dynlib

#endif
