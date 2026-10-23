#ifdef WMA_ENABLE_X11

#include "backends/x11/X11Api.hpp"

#include <span>

#include "platform/DynLib.hpp"

namespace wma
{

inline namespace x11_dyn
{
#define WMA_X11_ALL
// NOLINTNEXTLINE(bugprone-macro-parentheses): the argument names a declarator
#define WMA_X11_SYM(name) decltype(&::name) name = nullptr;
#include "X11Symbols.inc"
#undef WMA_X11_SYM
#undef WMA_X11_ALL
} // namespace x11_dyn

namespace x11
{

namespace
{
bool bindLibrary(std::span<const char *const> sonames, std::span<const dynlib::Binding> bindings) noexcept
{
    return dynlib::bind(dynlib::open(sonames), bindings);
}
} // namespace

bool load() noexcept
{
    static const bool loaded = []
    {
        static constexpr const char *kSonames[] = {"libX11.so.6"};
        static const dynlib::Binding kBindings[] = {
#define WMA_X11_CORE
#define WMA_X11_SYM(name) {#name, reinterpret_cast<void **>(&x11_dyn::name)},
#include "X11Symbols.inc"
#undef WMA_X11_SYM
#undef WMA_X11_CORE
        };
        return bindLibrary(kSonames, kBindings);
    }();
    return loaded;
}

bool hasShm() noexcept
{
    static const bool loaded = []
    {
        static constexpr const char *kSonames[] = {"libXext.so.6"};
        static const dynlib::Binding kBindings[] = {
#define WMA_X11_SHM
#define WMA_X11_SYM(name) {#name, reinterpret_cast<void **>(&x11_dyn::name)},
#include "X11Symbols.inc"
#undef WMA_X11_SYM
#undef WMA_X11_SHM
        };
        return bindLibrary(kSonames, kBindings);
    }();
    return loaded;
}

bool hasXInput2() noexcept
{
    static const bool loaded = []
    {
        static constexpr const char *kSonames[] = {"libXi.so.6"};
        static const dynlib::Binding kBindings[] = {
#define WMA_X11_INPUT2
#define WMA_X11_SYM(name) {#name, reinterpret_cast<void **>(&x11_dyn::name)},
#include "X11Symbols.inc"
#undef WMA_X11_SYM
#undef WMA_X11_INPUT2
        };
        return bindLibrary(kSonames, kBindings);
    }();
    return loaded;
}

bool hasRandr() noexcept
{
    static const bool loaded = []
    {
        static constexpr const char *kSonames[] = {"libXrandr.so.2"};
        static const dynlib::Binding kBindings[] = {
#define WMA_X11_RANDR
#define WMA_X11_SYM(name) {#name, reinterpret_cast<void **>(&x11_dyn::name)},
#include "X11Symbols.inc"
#undef WMA_X11_SYM
#undef WMA_X11_RANDR
        };
        return bindLibrary(kSonames, kBindings);
    }();
    return loaded;
}

bool hasPresent() noexcept
{
    static const bool loaded = []
    {
        static constexpr const char *kSonames[] = {"libXpresent.so.1"};
        static const dynlib::Binding kBindings[] = {
#define WMA_X11_PRESENT
#define WMA_X11_SYM(name) {#name, reinterpret_cast<void **>(&x11_dyn::name)},
#include "X11Symbols.inc"
#undef WMA_X11_SYM
#undef WMA_X11_PRESENT
        };
        return bindLibrary(kSonames, kBindings);
    }();
    return loaded;
}

bool hasGlx() noexcept
{
#ifdef WMA_X11_HAS_GL
    static const bool loaded = []
    {
        //! libGLX is the glvnd dispatcher; libGL.so.1 is the same entry points on a system without glvnd.
        static constexpr const char *kSonames[] = {"libGLX.so.0", "libGL.so.1"};
        static const dynlib::Binding kBindings[] = {
#define WMA_X11_GLX
#define WMA_X11_SYM(name) {#name, reinterpret_cast<void **>(&x11_dyn::name)},
#include "X11Symbols.inc"
#undef WMA_X11_SYM
#undef WMA_X11_GLX
        };
        return bindLibrary(kSonames, kBindings);
    }();
    return loaded;
#else
    return false;
#endif
}

} // namespace x11

} // namespace wma

#endif // WMA_ENABLE_X11
