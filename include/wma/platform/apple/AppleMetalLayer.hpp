#ifndef WMA_PLATFORM_APPLE_METAL_LAYER_HPP
#define WMA_PLATFORM_APPLE_METAL_LAYER_HPP

namespace wma
{
namespace apple
{

/**
 * @brief Creates a CAMetalLayer and makes it the backing layer of an NSWindow's
 *        content view.
 *
 * The AppKit half of the GLFW Metal path, and the only Objective-C in this
 * library. It exists because GLFW — unlike SDL3, which ships
 * SDL_Metal_CreateView — hands back a bare NSWindow and leaves hosting a
 * Metal layer entirely to the caller.
 *
 * Both parameters and the return value are void* so this header stays
 * includable from plain C++: AppleMetalLayer.mm is the only translation unit
 * that needs AppKit/QuartzCore on its include path. That mirrors how
 * IWindowManager::getMetalLayer() keeps Objective-C out of consumer TUs.
 *
 * The layer's contentsScale is set from the window's backingScaleFactor, so a
 * Retina window renders at full resolution rather than half. Its device and
 * pixel format are deliberately left unset — those belong to the renderer.
 *
 * @param[in] nsWindow An NSWindow*, as returned by glfwGetCocoaWindow().
 * @return The CAMetalLayer*, owned by the view and therefore by the window;
 *         nullptr if @p nsWindow is null or has no content view. The caller
 *         must not release it.
 *
 * @note macOS only. iOS has no NSWindow, and GLFW has no iOS port, so this is
 *       compiled only when both conditions hold (see src/CMakeLists.txt).
 */
[[nodiscard]] void *attachMetalLayerToNSWindow(void *nsWindow) noexcept;

} // namespace apple
} // namespace wma

#endif // WMA_PLATFORM_APPLE_METAL_LAYER_HPP
