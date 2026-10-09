#ifndef WMA_BACKENDS_WAYLAND_WINDOW_MANAGER_HPP
#define WMA_BACKENDS_WAYLAND_WINDOW_MANAGER_HPP

#include "WaylandKeyboardListener.hpp"
#include "WaylandMouseListener.hpp"
#include "wma/WaylandSurfaceRole.hpp"
#include "wma/backends/WindowHit.hpp"
#include "wma/backends/wayland/protocols/xdg-decoration-unstable-v1-client-protocol.h"
#include "wma/backends/wayland/protocols/xdg-shell-client-protocol.h"
#include "wma/managers/IWindowManager.hpp"
#include <array>
#include <memory>
#include <wayland-client.h>

namespace wma
{

class WaylandWindowManager : public IWindowManager
{
  public:
    explicit WaylandWindowManager(const WindowDetails &windowDetails, GraphicsAPI graphicsAPI = GraphicsAPI::Vulkan,
                                  std::unique_ptr<WaylandSurfaceRole> role = {});
    ~WaylandWindowManager() override;

    WaylandWindowManager(const WaylandWindowManager &) = delete;
    WaylandWindowManager &operator=(const WaylandWindowManager &) = delete;
    WaylandWindowManager(WaylandWindowManager &&) noexcept;
    WaylandWindowManager &operator=(WaylandWindowManager &&) noexcept;

    void createWindow(const char *windowName) override;
    [[nodiscard]] bool isToplevel() const noexcept override
    {
        return xdgToplevel_ != nullptr;
    }
    bool transparentFramebuffer() const noexcept override
    {
        return role_ && role_->transparentFramebuffer();
    }
    void pollEvents() override;
    void waitEvents(int timeoutMs) override;
    void swapBuffers() override;
    void *getWindowInstance() override;
    void *getNativeDisplayHandle() const noexcept override;
    void *getGLProcAddress(const char *name) const override;
    SoftwareFramebuffer lockFramebuffer() override;
    FramebufferSize getFramebufferSize() noexcept override;
    void presentFramebuffer() override;
    WindowFlags *getWindowFlags() noexcept override;
    const WindowDetails *getWindowDetails() noexcept override;
    const std::vector<const char *> getVulkanExtensions() const override;
    KeyboardListener &getKeyboardListener() noexcept override;
    void setTextInputEnabled(bool enabled) noexcept override;
    [[nodiscard]] bool isTextInputEnabled() const noexcept override;
    MouseListener &getMouseListener() noexcept override;
    bool minimize() noexcept override;
    bool maximize() noexcept override;
    bool restore() noexcept override;
    [[nodiscard]] bool isMaximized() const noexcept override;
    void close() noexcept override;
    bool setHitTest(HitTest hitTest) override;
    bool setTitle(const char *title) noexcept override;
    [[nodiscard]] DecorationMode getDecorationMode() const noexcept override;
    bool shouldClose() const override;
    WindowBackend getBackendType() const override;
    GraphicsAPI getGraphicsAPI() const override;
    WmaCode destroy() override;

    wl_display *getDisplay() const
    {
        return display_;
    }
    wl_surface *getSurface() const
    {
        return surface_;
    }

  private:
    friend class WaylandMouseListener;

    std::unique_ptr<WaylandSurfaceRole> role_;
    void rebindListeners() noexcept;
    //! Takes a press the hit test assigns to the frame: resizes at once, and holds a
    //! title-bar press until dragTo() turns it into a move or the release into a click.
    [[nodiscard]] bool claimPress(u32 serial, f64 x, f64 y);
    void dragTo(f64 x, f64 y);
    void releaseClaim() noexcept;
    void cancelClaim() noexcept;
    [[nodiscard]] SystemCursor hoverCursor(f64 x, f64 y) const;
    //! pollEvents() and waitEvents() differ only by the poll timeout.
    void dispatch(int timeoutMs);
    wl_display *display_;
    wl_registry *registry_;
    wl_compositor *compositor_;
    wl_surface *surface_;
    wl_seat *seat_;
    u32 seatGlobalName_ = 0;
    wl_shm *shm_;

    xdg_wm_base *xdgWmBase_;
    xdg_surface *xdgSurface_;
    xdg_toplevel *xdgToplevel_;

    //! The compositor confirms the requested decoration mode asynchronously.
    zxdg_decoration_manager_v1 *xdgDecorationManager_;
    zxdg_toplevel_decoration_v1 *xdgToplevelDecoration_;

    wl_keyboard *keyboard_;
    wl_pointer *pointer_;

    //! Software rendering (GraphicsAPI::CPU) via wl_shm, double-buffered: writing into
    //! one slot while the compositor may still be reading the other is what a single
    //! reused buffer cannot do safely, and is also the backpressure that keeps a fast
    //! rasterizer from outrunning the compositor -- lockFramebuffer() simply has no free
    //! slot to hand out once both are in flight. Vulkan gets the same thing from its
    //! swapchain, OpenGL from eglSwapInterval; raw wl_shm has nothing built in.
    static constexpr i32 kShmBufferCount = 2;

    struct ShmBuffer
    {
        wl_buffer *buffer = nullptr;
        void *data = nullptr;
        //! Set on commit, cleared by wl_buffer.release: true while the compositor may
        //! still be reading this slot's memory.
        bool busy = false;
    };

    //! One memfd/mmap split into kShmBufferCount slots. A resize retires the whole pool
    //! (see retiringPool_) rather than tearing it down in place, since any of its slots
    //! can still be the surface's displayed content.
    struct ShmPool
    {
        ~ShmPool();
        void *mapped = nullptr;
        i32 mappedSize = 0;
        i32 width = 0;
        i32 height = 0;
        i32 stride = 0;
        std::array<ShmBuffer, kShmBufferCount> buffers;
    };

    std::unique_ptr<ShmPool> shmPool_;
    //! The pool a resize just replaced. Freed once every one of its slots reports
    //! released, checked opportunistically in lockFramebuffer().
    std::unique_ptr<ShmPool> retiringPool_;
    //! The slot lockFramebuffer() last handed out, pending the matching presentFramebuffer().
    ShmBuffer *lockedSlot_ = nullptr;
    //! A second, orthogonal gate on lockFramebuffer(): slot availability alone only
    //! keeps writes safe, it does not stop the rasterizer from producing far more
    //! frames than the compositor ever displays (measured ~20x at a 60 Hz output with
    //! two reused buffers and no pacing). Non-null while the last commit is
    //! unacknowledged.
    wl_callback *frameCallback_ = nullptr;

    //! OpenGL via EGL (opaque so this header needs no EGL includes).
    void *eglWindow_;  //!< wl_egl_window*
    void *eglDisplay_; //!< EGLDisplay
    void *eglContext_; //!< EGLContext
    void *eglSurface_; //!< EGLSurface

    WindowDetails windowDetails_;
    WindowFlags windowFlags_;
    GraphicsAPI graphicsAPI_;
    bool windowShouldClose_;
    bool configured_;
    bool maximized_ = false;
    bool pendingMaximized_ = false;
    bool pendingFloating_ = true;
    i32 pendingWidth_ = 0;
    i32 pendingHeight_ = 0;
    //! Last size while neither maximized nor fullscreen; see handleXdgSurfaceConfigure.
    i32 floatingWidth_ = 0;
    i32 floatingHeight_ = 0;
    DecorationMode decorationMode_ = DecorationMode::ClientSide;
    DecorationMode pendingDecorationMode_ = DecorationMode::ClientSide;
    HitTest hitTest_;
    detail::CaptionGesture caption_;
    u32 captionSerial_ = 0;

    std::unique_ptr<WaylandKeyboardListener> keyboardListener_;
    std::unique_ptr<WaylandMouseListener> mouseListener_;

    static const wl_registry_listener registryListener_;
    static void handleRegistryGlobal(void *data, wl_registry *registry, u32 name, const char *interface, u32 version);
    static void handleRegistryGlobalRemove(void *data, wl_registry *registry, u32 name);

    static const wl_seat_listener seatListener_;
    static void handleSeatCapabilities(void *data, wl_seat *seat, u32 capabilities);
    static void handleSeatName(void *data, wl_seat *seat, const char *name);

    static const xdg_wm_base_listener xdgWmBaseListener_;
    static void handleXdgWmBasePing(void *data, xdg_wm_base *xdg_wm_base, u32 serial);

    static const xdg_surface_listener xdgSurfaceListener_;
    static void handleXdgSurfaceConfigure(void *data, xdg_surface *xdg_surface, u32 serial);

    static const xdg_toplevel_listener xdgToplevelListener_;
    static void handleXdgToplevelConfigure(void *data, xdg_toplevel *xdg_toplevel, i32 width, i32 height,
                                           wl_array *states);
    static void handleXdgToplevelClose(void *data, xdg_toplevel *xdg_toplevel);
    static void handleXdgToplevelConfigureBounds(void *data, xdg_toplevel *xdg_toplevel, i32 width, i32 height);

    static const zxdg_toplevel_decoration_v1_listener xdgToplevelDecorationListener_;
    static void handleXdgToplevelDecorationConfigure(void *data, zxdg_toplevel_decoration_v1 *decoration, u32 mode);

    static const wl_buffer_listener bufferListener_;
    static void handleBufferRelease(void *data, wl_buffer *buffer);

    static const wl_callback_listener frameCallbackListener_;
    static void handleFrameDone(void *data, wl_callback *callback, u32 time);

    void setupInputDevices();
    void initEGL();
    void allocateShmBuffer(i32 width, i32 height);
    //! Frees retiringPool_ once none of its slots are still with the compositor.
    void releaseRetiringPoolIfIdle() noexcept;
};

} // namespace wma

#endif // WMA_BACKENDS_WAYLAND_WINDOW_MANAGER_HPP
