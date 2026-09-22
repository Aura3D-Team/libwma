#ifndef WMA_BACKENDS_WAYLAND_WINDOW_MANAGER_HPP
#define WMA_BACKENDS_WAYLAND_WINDOW_MANAGER_HPP

#include <wayland-client.h>
#include <memory>
#include "wma/WaylandSurfaceRole.hpp"
#include "WaylandMouseListener.hpp"
#include "WaylandKeyboardListener.hpp"
#include "wma/backends/wayland/protocols/xdg-shell-client-protocol.h"
#include "wma/backends/wayland/protocols/xdg-decoration-unstable-v1-client-protocol.h"
#include "wma/managers/IWindowManager.hpp"

namespace wma {

class WaylandWindowManager : public IWindowManager {
public:
    explicit WaylandWindowManager(const WindowDetails& windowDetails,
                                  GraphicsAPI graphicsAPI = GraphicsAPI::Vulkan,
                                  std::unique_ptr<WaylandSurfaceRole> role = {});
    ~WaylandWindowManager() override;

    WaylandWindowManager(const WaylandWindowManager&) = delete;
    WaylandWindowManager& operator=(const WaylandWindowManager&) = delete;
    WaylandWindowManager(WaylandWindowManager&&) noexcept;
    WaylandWindowManager& operator=(WaylandWindowManager&&) noexcept;

    void createWindow(const char* windowName) override;
    bool transparentFramebuffer() const noexcept override { return role_ && role_->transparentFramebuffer(); }
    void pollEvents() override;
    void waitEvents(int timeoutMs) override;
    void swapBuffers() override;
    void* getWindowInstance() override;
    void* getNativeDisplayHandle() const noexcept override;
    void* getGLProcAddress(const char* name) const override;
    SoftwareFramebuffer lockFramebuffer() override;
    FramebufferSize getFramebufferSize() noexcept override;
    void presentFramebuffer() override;
    WindowFlags* getWindowFlags() noexcept override;
    const WindowDetails* getWindowDetails() noexcept override;
    const std::vector<const char*> getVulkanExtensions() const override;
    KeyboardListener& getKeyboardListener() noexcept override;
    void setTextInputEnabled(bool enabled) noexcept override;
    [[nodiscard]] bool isTextInputEnabled() const noexcept override;
    MouseListener& getMouseListener() noexcept override;
    bool shouldClose() const override;
    WindowBackend getBackendType() const override;
    GraphicsAPI getGraphicsAPI() const override;
    WmaCode destroy() override;

    wl_display* getDisplay() const { return display_; }
    wl_surface* getSurface() const { return surface_; }

private:
    std::unique_ptr<WaylandSurfaceRole> role_;
    void rebindListeners() noexcept;
    //! pollEvents() and waitEvents() differ only by the poll timeout.
    void dispatch(int timeoutMs);
    wl_display* display_;
    wl_registry* registry_;
    wl_compositor* compositor_;
    wl_surface* surface_;
    wl_seat* seat_;
    wl_shm* shm_;

    xdg_wm_base* xdgWmBase_;
    xdg_surface* xdgSurface_;
    xdg_toplevel* xdgToplevel_;

    //! xdg-decoration: requests server-side decorations when the compositor
    //! advertises the global; silently absent under GNOME/Mutter.
    zxdg_decoration_manager_v1* xdgDecorationManager_;
    zxdg_toplevel_decoration_v1* xdgToplevelDecoration_;

    wl_keyboard* keyboard_;
    wl_pointer* pointer_;

    //! Software rendering (GraphicsAPI::CPU) via wl_shm.
    wl_buffer* shmBuffer_;
    void* shmData_;
    i32 shmSize_;
    i32 shmWidth_;
    i32 shmHeight_;

    //! OpenGL via EGL (opaque so this header needs no EGL includes).
    void* eglWindow_; //!< wl_egl_window*
    void* eglDisplay_; //!< EGLDisplay
    void* eglContext_; //!< EGLContext
    void* eglSurface_; //!< EGLSurface

    WindowDetails windowDetails_;
    WindowFlags windowFlags_;
    GraphicsAPI graphicsAPI_;
    bool windowShouldClose_;
    bool configured_;

    std::unique_ptr<WaylandKeyboardListener> keyboardListener_;
    std::unique_ptr<WaylandMouseListener> mouseListener_;

    static const wl_registry_listener registryListener_;
    static void handleRegistryGlobal(void* data, wl_registry* registry,
                                     u32 name, const char* interface, u32 version);
    static void handleRegistryGlobalRemove(void* data, wl_registry* registry, u32 name);

    static const wl_seat_listener seatListener_;
    static void handleSeatCapabilities(void* data, wl_seat* seat, u32 capabilities);
    static void handleSeatName(void* data, wl_seat* seat, const char* name);

    static const xdg_wm_base_listener xdgWmBaseListener_;
    static void handleXdgWmBasePing(void* data, xdg_wm_base* xdg_wm_base, u32 serial);

    static const xdg_surface_listener xdgSurfaceListener_;
    static void handleXdgSurfaceConfigure(void* data, xdg_surface* xdg_surface, u32 serial);

    static const xdg_toplevel_listener xdgToplevelListener_;
    static void handleXdgToplevelConfigure(void* data, xdg_toplevel* xdg_toplevel,
                                           i32 width, i32 height, wl_array* states);
    static void handleXdgToplevelClose(void* data, xdg_toplevel* xdg_toplevel);
    static void handleXdgToplevelConfigureBounds(void* data, xdg_toplevel* xdg_toplevel,
                                                 i32 width, i32 height);

    void setupInputDevices();
    void initEGL();
    void allocateShmBuffer(i32 width, i32 height);
    void destroyShmBuffer();
};

} // namespace wma

#endif // WMA_BACKENDS_WAYLAND_WINDOW_MANAGER_HPP
