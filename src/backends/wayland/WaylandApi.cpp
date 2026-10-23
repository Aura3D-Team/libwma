#ifdef WMA_ENABLE_WAYLAND

#include "backends/wayland/WaylandLoader.hpp"

#include <span>

#include "platform/DynLib.hpp"

namespace wma
{

inline namespace wayland_dyn
{
#define WMA_WL_ALL
// NOLINTNEXTLINE(bugprone-macro-parentheses): the argument names a declarator
#define WMA_WL_SYM(name) decltype(&::name) name = nullptr;
#include "backends/wayland/WaylandSymbols.inc"
#undef WMA_WL_SYM
#undef WMA_WL_ALL
} // namespace wayland_dyn

namespace wayland
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
        static constexpr const char *kSonames[] = {"libwayland-client.so.0"};
        static const dynlib::Binding kBindings[] = {
#define WMA_WL_CLIENT
#define WMA_WL_SYM(name) {#name, reinterpret_cast<void **>(&wayland_dyn::name)},
#include "backends/wayland/WaylandSymbols.inc"
#undef WMA_WL_SYM
#undef WMA_WL_CLIENT
        };
        return bindLibrary(kSonames, kBindings);
    }();
    return loaded;
}

bool hasXkb() noexcept
{
    static const bool loaded = []
    {
        static constexpr const char *kSonames[] = {"libxkbcommon.so.0"};
        static const dynlib::Binding kBindings[] = {
#define WMA_WL_XKB
#define WMA_WL_SYM(name) {#name, reinterpret_cast<void **>(&wayland_dyn::name)},
#include "backends/wayland/WaylandSymbols.inc"
#undef WMA_WL_SYM
#undef WMA_WL_XKB
        };
        return bindLibrary(kSonames, kBindings);
    }();
    return loaded;
}

bool hasCursor() noexcept
{
    static const bool loaded = []
    {
        static constexpr const char *kSonames[] = {"libwayland-cursor.so.0"};
        static const dynlib::Binding kBindings[] = {
#define WMA_WL_CURSOR
#define WMA_WL_SYM(name) {#name, reinterpret_cast<void **>(&wayland_dyn::name)},
#include "backends/wayland/WaylandSymbols.inc"
#undef WMA_WL_SYM
#undef WMA_WL_CURSOR
        };
        return bindLibrary(kSonames, kBindings);
    }();
    return loaded;
}

bool hasEgl() noexcept
{
#ifdef WMA_WAYLAND_HAS_GL
    static const bool loaded = []
    {
        static constexpr const char *kEglSonames[] = {"libEGL.so.1"};
        static constexpr const char *kWindowSonames[] = {"libwayland-egl.so.1"};
        static const dynlib::Binding kEglBindings[] = {
#define WMA_WL_EGL
#define WMA_WL_SYM(name) {#name, reinterpret_cast<void **>(&wayland_dyn::name)},
#include "backends/wayland/WaylandSymbols.inc"
#undef WMA_WL_SYM
#undef WMA_WL_EGL
        };
        static const dynlib::Binding kWindowBindings[] = {
#define WMA_WL_WLEGL
#define WMA_WL_SYM(name) {#name, reinterpret_cast<void **>(&wayland_dyn::name)},
#include "backends/wayland/WaylandSymbols.inc"
#undef WMA_WL_SYM
#undef WMA_WL_WLEGL
        };
        return bindLibrary(kEglSonames, kEglBindings) && bindLibrary(kWindowSonames, kWindowBindings);
    }();
    return loaded;
#else
    return false;
#endif
}

} // namespace wayland

} // namespace wma

#endif // WMA_ENABLE_WAYLAND
