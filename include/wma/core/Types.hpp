#ifndef WMA_CORE_TYPES_HPP
#define WMA_CORE_TYPES_HPP

#include <functional>
#include <ink/ink_base.hpp>

namespace wma
{

#if defined(__cpp_lib_move_only_function)
template <typename Sig> using move_only_function = std::move_only_function<Sig>;
#else
template <typename Sig> using move_only_function = std::function<Sig>;
#endif

//! Scoped enums are referenced qualified everywhere (GraphicsAPI::Vulkan, …).
//! `enum class` keeps the enumerators out of namespace wma; the WmaCode values
//! are deliberately `Ok`/`Error` (not `OK`/`ERROR`) because <windows.h> defines
//! an `ERROR` macro and <X11/X.h> defines a `Success` macro — those tokens would
//! be text-substituted even inside an enum declaration and break the build.
//! Metal is appended rather than slotted in alphabetically so the three
//! original values keep their numbers — they appear in serialized config and
//! in log output on the consumer side.
enum class GraphicsAPI : i32
{
    OpenGL,
    Vulkan,
    CPU,
    Metal
};

enum class WindowBackend : i32
{
    GLFW,
    SDL3,
    X11,
    WAYLAND
};

/**
 * @brief The platform API an IAudioDevice pushes samples through.
 *
 * Deliberately a separate axis from WindowBackend: GLFW, X11 and Wayland are
 * display protocols with no audio API of their own, so a window opened
 * through any of them can pair with any backend here. Only SDL3 appears in
 * both enums, and even there the two are independent — an X11 window
 * alongside an ALSA device is a normal configuration.
 *
 * Availability is a build-time property (WMA_HAS_ALSA / WMA_HAS_SDL); query
 * isAudioBackendAvailable() or take getDefaultAudioBackend()'s answer.
 */
enum class AudioBackend : i32
{
    /**
     * @brief Native Linux audio through libasound (snd_pcm_*).
     *
     * The lowest-latency route on desktop Linux: it talks to the kernel
     * interface directly rather than through a sound server. PulseAudio and
     * PipeWire both expose an ALSA-compatible "default" device, so this
     * still works on a desktop running either — it simply does not use
     * their client APIs.
     */
    Alsa,

    /**
     * @brief SDL3's cross-platform audio API.
     *
     * The universal path, and the *only* one on Android, WASM and Apple
     * platforms: SDL3 already resolves to AAudio/OpenSL ES, Web Audio and
     * CoreAudio respectively, so there is no separate native backend to
     * write for those targets.
     */
    Sdl3,

    /**
     * @brief Inert device: accepts a mix callback and never invokes it.
     *
     * Always compiled in, never fails to open. Exists so headless
     * environments (CI containers with no sound hardware, automated tests)
     * can exercise the full audio path without a device, and so
     * createAudioDevice() has something to degrade to rather than throwing.
     */
    Null
};

enum class WmaCode : i32
{
    Ok,
    Error
};

/**
 * @brief A window's drawable size, in pixels.
 *
 * Distinct from WindowDetails' width/height, which are the *logical* size the
 * window was requested at. The two differ by the display's backing scale
 * factor: a 1280x720 window on a Retina screen has a 2560x1440 backing store,
 * and a renderer that sizes its surface from the logical value draws a
 * quarter of the window stretched over the whole of it.
 *
 * @see IWindowManager::getFramebufferSize()
 */
struct FramebufferSize
{
    i32 width = 0;  ///< Width in pixels.
    i32 height = 0; ///< Height in pixels.

    [[nodiscard]] constexpr bool valid() const noexcept
    {
        return width > 0 && height > 0;
    }
};

/**
 * @brief A CPU-writable framebuffer for software rendering.
 *
 * Returned by IWindowManager::lockFramebuffer() when the window was created
 * with GraphicsAPI::CPU. Pixels are 32-bit; the exact channel order is
 * backend-defined (SDL follows the surface format; X11/Wayland use BGRA/XRGB8888).
 * `pixels == nullptr` means software rendering is unavailable for that backend
 * or the window is not in CPU mode.
 */
struct SoftwareFramebuffer
{
    void *pixels = nullptr; ///< Base address of the top-left pixel.
    i32 width = 0;          ///< Width in pixels.
    i32 height = 0;         ///< Height in pixels.
    i32 pitch = 0;          ///< Bytes per row (may exceed width * 4).

    [[nodiscard]] constexpr bool valid() const noexcept
    {
        return pixels != nullptr;
    }
};

} // namespace wma

#endif // WMA_CORE_TYPES_HPP
