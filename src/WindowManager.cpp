#include "wma/wma.hpp"

#include <array>
#include <cstdlib>
#include <filesystem>
#include <system_error>

//! This TU is compiled INTO the library, where the private WMA_ENABLE_* backend
//! macros and the backend SDK headers are visible. Keeping the factory here (rather
//! than inline in wma.hpp) is what lets a downstream consumer select a backend
//! without having any backend SDK on its own include path.

#ifdef WMA_ENABLE_GLFW
#include "wma/backends/glfw/GlfwWindowManager.hpp"
#endif
#ifdef WMA_ENABLE_SDL
#include "wma/backends/sdl/SdlWindowManager.hpp"
#endif
#ifdef WMA_ENABLE_WAYLAND
#include "backends/wayland/WaylandApi.hpp"
#include "wma/backends/wayland/WaylandWindowManager.hpp"
#endif
#ifdef WMA_ENABLE_X11
#include "backends/x11/X11Api.hpp"
#include "wma/backends/x11/X11WindowManager.hpp"
#endif

namespace wma
{

std::unique_ptr<IWindowManager> createWindowManager(WindowBackend backend, const WindowDetails &windowDetails,
                                                    GraphicsAPI graphicsAPI)
{
    switch (backend)
    {
#ifdef WMA_ENABLE_GLFW
    case WindowBackend::GLFW:
        return std::make_unique<GlfwWindowManager>(windowDetails, graphicsAPI);
#endif
#ifdef WMA_ENABLE_SDL
    case WindowBackend::SDL3:
        return std::make_unique<SdlWindowManager>(windowDetails, graphicsAPI);
#endif
#ifdef WMA_ENABLE_WAYLAND
    case WindowBackend::WAYLAND:
        return std::make_unique<WaylandWindowManager>(windowDetails, graphicsAPI);
#endif
#ifdef WMA_ENABLE_X11
    case WindowBackend::X11:
        return std::make_unique<X11WindowManager>(windowDetails, graphicsAPI);
#endif
    default:
        break;
    }
    throw WMAException("Requested window backend is not available or not compiled in");
}

WindowBackend getDefaultBackend()
{
#if defined(WMA_ENABLE_SDL)
    //! SDL3 is the most portable backend and the only one available on WASM/Android.
    return WindowBackend::SDL3;
#elif defined(WMA_ENABLE_WAYLAND)
    return WindowBackend::WAYLAND;
#elif defined(WMA_ENABLE_GLFW)
    return WindowBackend::GLFW;
#elif defined(WMA_ENABLE_X11)
    return WindowBackend::X11;
#else
    throw WMAException("No window backend is enabled in this build");
#endif
}

bool isBackendAvailable(WindowBackend backend) noexcept
{
    if ((backend == WindowBackend::GLFW && WMA_HAS_GLFW) || (backend == WindowBackend::SDL3 && WMA_HAS_SDL))
        return true;
    //! The native backends bind their system libraries at run time, so "compiled in" is not enough.
#ifdef WMA_ENABLE_WAYLAND
    if (backend == WindowBackend::WAYLAND)
        return wayland::load();
#endif
#ifdef WMA_ENABLE_X11
    if (backend == WindowBackend::X11)
        return x11::load();
#endif
    return false;
}

namespace
{

#if defined(__linux__) && !defined(__ANDROID__) && !defined(__EMSCRIPTEN__)
bool isSet(const char *name) noexcept
{
    const char *value = std::getenv(name);
    return value && *value;
}

//! libwayland's own order: an inherited socket, $WAYLAND_DISPLAY, then wayland-0 in the runtime directory.
bool hasWaylandDisplay() noexcept
{
    if (isSet("WAYLAND_SOCKET") || isSet("WAYLAND_DISPLAY"))
        return true;
    const char *runtimeDir = std::getenv("XDG_RUNTIME_DIR");
    std::error_code error;
    return runtimeDir && *runtimeDir && std::filesystem::exists(std::filesystem::path(runtimeDir) / "wayland-0", error);
}
#endif

//! Whether the session has a display server this backend can talk to.
bool hasDisplayServer(WindowBackend backend) noexcept
{
#if defined(__linux__) && !defined(__ANDROID__) && !defined(__EMSCRIPTEN__)
    if (backend == WindowBackend::WAYLAND)
        return hasWaylandDisplay();
    if (backend == WindowBackend::X11)
        return isSet("DISPLAY");
    //! GLFW has no headless platform it falls back to. SDL3 is not checked: it also runs on a bare console (KMSDRM).
    if (backend == WindowBackend::GLFW)
        return isSet("DISPLAY") || hasWaylandDisplay();
#endif
    (void)backend;
    return true;
}

} // namespace

WindowBackend selectUsableBackend(WindowBackend preferred)
{
    if (isBackendAvailable(preferred) && hasDisplayServer(preferred))
        return preferred;

    //! SDL3 first: it is the one that binds its own libraries and works on the most machines.
    constexpr std::array<WindowBackend, 4> kFallbacks{WindowBackend::SDL3, WindowBackend::WAYLAND, WindowBackend::X11,
                                                      WindowBackend::GLFW};
    for (const WindowBackend candidate : kFallbacks)
    {
        if (isBackendAvailable(candidate) && hasDisplayServer(candidate))
            return candidate;
    }
    throw WMAException("No window backend can run here: no display server, and SDL3 is not compiled in");
}

const char *getLibraryInfo() noexcept
{
    static const char *info = "WMA Window Management & Input Abstraction Library v" WMA_VERSION_STRING_FULL "\n"
                              "Backends:"
#ifdef WMA_ENABLE_GLFW
                              " GLFW"
#endif
#ifdef WMA_ENABLE_SDL
                              " SDL3"
#endif
#ifdef WMA_ENABLE_WAYLAND
                              " Wayland"
#endif
#ifdef WMA_ENABLE_X11
                              " X11"
#endif
                              //! Listed on their own line because audio is a separate axis from
                              //! windowing: any window backend pairs with any audio backend. Null
                              //! leads because it is the one that is always compiled in.
                              "\nAudio backends: Null"
#ifdef WMA_ENABLE_ALSA
                              " ALSA"
#endif
#ifdef WMA_ENABLE_SDL
                              " SDL3"
#endif
                              "\nGraphics APIs: OpenGL Vulkan CPU"
#if WMA_HAS_METAL
                              //! Reported from WMA_HAS_METAL rather than unconditionally: it is the only
                              //! graphics API whose availability depends on the target platform, not
                              //! just on which backends were compiled in.
                              " Metal"
#endif
                              "\nBuilt with C++23";
    return info;
}

bool IWindowManager::isClipboardSupported() const noexcept
{
    return false;
}

std::string IWindowManager::getClipboardText()
{
    return {};
}

bool IWindowManager::setClipboardText(std::string_view)
{
    return false;
}

} // namespace wma
