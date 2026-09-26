#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "wma/backends/wayland/WaylandWindowManager.hpp"
#include "wma/exceptions/WMAException.hpp"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <poll.h>
#include <sys/mman.h>
#include <unistd.h>
#include <utility>

#ifdef WMA_WAYLAND_HAS_GL
#include <EGL/egl.h>
#include <wayland-egl.h>
#endif

namespace wma
{

namespace
{

//! Create an anonymous, sized, CLOEXEC file suitable for wl_shm.
int createAnonymousFile(off_t size)
{
    int fd = memfd_create("wma-shm", MFD_CLOEXEC);
    if (fd < 0)
        return -1;
    if (ftruncate(fd, size) < 0)
    {
        close(fd);
        return -1;
    }
    return fd;
}

} // namespace

const wl_registry_listener WaylandWindowManager::registryListener_ = {handleRegistryGlobal, handleRegistryGlobalRemove};

const wl_seat_listener WaylandWindowManager::seatListener_ = {handleSeatCapabilities, handleSeatName};

const xdg_wm_base_listener WaylandWindowManager::xdgWmBaseListener_ = {handleXdgWmBasePing};

const xdg_surface_listener WaylandWindowManager::xdgSurfaceListener_ = {handleXdgSurfaceConfigure};

const xdg_toplevel_listener WaylandWindowManager::xdgToplevelListener_ = {
    handleXdgToplevelConfigure, handleXdgToplevelClose, handleXdgToplevelConfigureBounds};

WaylandWindowManager::WaylandWindowManager(const WindowDetails &windowDetails, GraphicsAPI graphicsAPI,
                                           std::unique_ptr<WaylandSurfaceRole> role)
    : role_(std::move(role)), display_(nullptr), registry_(nullptr), compositor_(nullptr), surface_(nullptr),
      seat_(nullptr), shm_(nullptr), xdgWmBase_(nullptr), xdgSurface_(nullptr), xdgToplevel_(nullptr),
      xdgDecorationManager_(nullptr), xdgToplevelDecoration_(nullptr), keyboard_(nullptr), pointer_(nullptr),
      shmBuffer_(nullptr), shmData_(nullptr), shmSize_(0), shmWidth_(0), shmHeight_(0), eglWindow_(nullptr),
      eglDisplay_(nullptr), eglContext_(nullptr), eglSurface_(nullptr), windowDetails_(windowDetails), windowFlags_{},
      graphicsAPI_(graphicsAPI), windowShouldClose_(false), configured_(false),
      keyboardListener_(std::make_unique<WaylandKeyboardListener>(&windowFlags_)),
      mouseListener_(std::make_unique<WaylandMouseListener>())
{
}

WaylandWindowManager::~WaylandWindowManager()
{
    WaylandWindowManager::destroy();
}

WaylandWindowManager::WaylandWindowManager(WaylandWindowManager &&other) noexcept
    : role_(std::move(other.role_)), display_(std::exchange(other.display_, nullptr)),
      registry_(std::exchange(other.registry_, nullptr)), compositor_(std::exchange(other.compositor_, nullptr)),
      surface_(std::exchange(other.surface_, nullptr)), seat_(std::exchange(other.seat_, nullptr)),
      shm_(std::exchange(other.shm_, nullptr)), xdgWmBase_(std::exchange(other.xdgWmBase_, nullptr)),
      xdgSurface_(std::exchange(other.xdgSurface_, nullptr)), xdgToplevel_(std::exchange(other.xdgToplevel_, nullptr)),
      xdgDecorationManager_(std::exchange(other.xdgDecorationManager_, nullptr)),
      xdgToplevelDecoration_(std::exchange(other.xdgToplevelDecoration_, nullptr)),
      keyboard_(std::exchange(other.keyboard_, nullptr)), pointer_(std::exchange(other.pointer_, nullptr)),
      shmBuffer_(std::exchange(other.shmBuffer_, nullptr)), shmData_(std::exchange(other.shmData_, nullptr)),
      shmSize_(other.shmSize_), shmWidth_(other.shmWidth_), shmHeight_(other.shmHeight_),
      eglWindow_(std::exchange(other.eglWindow_, nullptr)), eglDisplay_(std::exchange(other.eglDisplay_, nullptr)),
      eglContext_(std::exchange(other.eglContext_, nullptr)), eglSurface_(std::exchange(other.eglSurface_, nullptr)),
      windowDetails_(other.windowDetails_), windowFlags_(other.windowFlags_), graphicsAPI_(other.graphicsAPI_),
      windowShouldClose_(other.windowShouldClose_), configured_(other.configured_),
      keyboardListener_(std::move(other.keyboardListener_)), mouseListener_(std::move(other.mouseListener_))
{
    rebindListeners();
}

WaylandWindowManager &WaylandWindowManager::operator=(WaylandWindowManager &&other) noexcept
{
    if (this != &other)
    {
        destroy();

        role_ = std::move(other.role_);
        display_ = std::exchange(other.display_, nullptr);
        registry_ = std::exchange(other.registry_, nullptr);
        compositor_ = std::exchange(other.compositor_, nullptr);
        surface_ = std::exchange(other.surface_, nullptr);
        seat_ = std::exchange(other.seat_, nullptr);
        shm_ = std::exchange(other.shm_, nullptr);
        xdgWmBase_ = std::exchange(other.xdgWmBase_, nullptr);
        xdgSurface_ = std::exchange(other.xdgSurface_, nullptr);
        xdgToplevel_ = std::exchange(other.xdgToplevel_, nullptr);
        xdgDecorationManager_ = std::exchange(other.xdgDecorationManager_, nullptr);
        xdgToplevelDecoration_ = std::exchange(other.xdgToplevelDecoration_, nullptr);
        keyboard_ = std::exchange(other.keyboard_, nullptr);
        pointer_ = std::exchange(other.pointer_, nullptr);
        shmBuffer_ = std::exchange(other.shmBuffer_, nullptr);
        shmData_ = std::exchange(other.shmData_, nullptr);
        shmSize_ = other.shmSize_;
        shmWidth_ = other.shmWidth_;
        shmHeight_ = other.shmHeight_;
        eglWindow_ = std::exchange(other.eglWindow_, nullptr);
        eglDisplay_ = std::exchange(other.eglDisplay_, nullptr);
        eglContext_ = std::exchange(other.eglContext_, nullptr);
        eglSurface_ = std::exchange(other.eglSurface_, nullptr);
        windowDetails_ = other.windowDetails_;
        windowFlags_ = other.windowFlags_;
        graphicsAPI_ = other.graphicsAPI_;
        windowShouldClose_ = other.windowShouldClose_;
        configured_ = other.configured_;
        keyboardListener_ = std::move(other.keyboardListener_);
        mouseListener_ = std::move(other.mouseListener_);
        rebindListeners();
    }
    return *this;
}

void WaylandWindowManager::rebindListeners() noexcept
{
    if (role_)
        role_->rebind(&windowDetails_, &windowFlags_);
    if (keyboardListener_)
        keyboardListener_->setWindowFlags(&windowFlags_);
    const auto rebind = [this](auto *proxy)
    {
        if (proxy)
            wl_proxy_set_user_data(reinterpret_cast<wl_proxy *>(proxy), this);
    };
    rebind(registry_);
    rebind(seat_);
    rebind(xdgWmBase_);
    rebind(xdgSurface_);
    rebind(xdgToplevel_);
}

void WaylandWindowManager::createWindow(const char *windowName)
{
    if (role_ && graphicsAPI_ != GraphicsAPI::Vulkan)
        throw GraphicsException("Custom Wayland surface roles currently require Vulkan");
    display_ = wl_display_connect(nullptr);
    if (!display_)
        throw WindowException("Failed to connect to Wayland display (is WAYLAND_DISPLAY set?)");

    registry_ = wl_display_get_registry(display_);
    wl_registry_add_listener(registry_, &registryListener_, this);

    wl_display_roundtrip(display_);

    if (!compositor_)
        throw WindowException("Wayland compositor global not available");

    if (!role_ && !xdgWmBase_)
        throw WindowException("xdg_wm_base not available (compositor must support xdg-shell)");

    surface_ = wl_compositor_create_surface(compositor_);

    if (role_)
    {
        role_->attach(display_, surface_, &windowDetails_, &windowFlags_);
    }
    else
    {
        xdgSurface_ = xdg_wm_base_get_xdg_surface(xdgWmBase_, surface_);
        xdg_surface_add_listener(xdgSurface_, &xdgSurfaceListener_, this);

        xdgToplevel_ = xdg_surface_get_toplevel(xdgSurface_);
        xdg_toplevel_add_listener(xdgToplevel_, &xdgToplevelListener_, this);

        xdg_toplevel_set_title(xdgToplevel_, windowName);
        xdg_toplevel_set_app_id(xdgToplevel_, "wma_app");

        //! Server-side decorations only: if the compositor doesn't advertise
        //! zxdg_decoration_manager_v1 (e.g. GNOME/Mutter), the surface stays
        //! borderless — this library does not implement client-side decorations.
        if (xdgDecorationManager_)
        {
            xdgToplevelDecoration_ =
                zxdg_decoration_manager_v1_get_toplevel_decoration(xdgDecorationManager_, xdgToplevel_);
            zxdg_toplevel_decoration_v1_set_mode(xdgToplevelDecoration_, ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
        }
    }

    //! Initial commit, then block until the compositor configures the surface.
    wl_surface_commit(surface_);
    wl_display_roundtrip(display_);
    while (!(role_ ? role_->configured() : configured_))
    {
        if (shouldClose() || wl_display_dispatch(display_) < 0)
            throw WindowException("Wayland surface closed before initial configure");
    }

    if (seat_)
        setupInputDevices();

    switch (graphicsAPI_)
    {
    case GraphicsAPI::OpenGL:
        initEGL();
        break;
    case GraphicsAPI::CPU:
        if (!shm_)
        {
            throw GraphicsException("wl_shm global not available for software rendering");
        }
        allocateShmBuffer(windowDetails_.width, windowDetails_.height);
        break;
    case GraphicsAPI::Vulkan:
        //! The application creates the VkSurfaceKHR from display_ + surface_.
        break;
    case GraphicsAPI::Metal:
        //! Named rather than left to the default below purely for the
        //! diagnostic: "unsupported" reads like a build-option problem, when
        //! the truth is that Wayland and Metal exist on disjoint platforms.
        throw GraphicsException("Metal is an Apple-only graphics API and Wayland is a Linux display "
                                "protocol; the two can never pair (use Vulkan/OpenGL/CPU)");
    default:
        throw GraphicsException("Unsupported graphics API for Wayland");
    }
}

void WaylandWindowManager::initEGL()
{
#ifdef WMA_WAYLAND_HAS_GL
    auto eglDisplay = eglGetDisplay(reinterpret_cast<EGLNativeDisplayType>(display_));
    if (eglDisplay == EGL_NO_DISPLAY)
        throw GraphicsException("eglGetDisplay failed on Wayland");

    if (!eglInitialize(eglDisplay, nullptr, nullptr))
        throw GraphicsException("eglInitialize failed on Wayland");

    eglBindAPI(EGL_OPENGL_API);

    const EGLint configAttribs[] = {EGL_SURFACE_TYPE,
                                    EGL_WINDOW_BIT,
                                    EGL_RENDERABLE_TYPE,
                                    EGL_OPENGL_BIT,
                                    EGL_RED_SIZE,
                                    8,
                                    EGL_GREEN_SIZE,
                                    8,
                                    EGL_BLUE_SIZE,
                                    8,
                                    EGL_ALPHA_SIZE,
                                    8,
                                    EGL_DEPTH_SIZE,
                                    24,
                                    EGL_STENCIL_SIZE,
                                    8,
                                    EGL_NONE};
    EGLConfig config;
    EGLint numConfigs = 0;
    if (!eglChooseConfig(eglDisplay, configAttribs, &config, 1, &numConfigs) || numConfigs == 0)
        throw GraphicsException("eglChooseConfig found no suitable Wayland config");

#ifdef EGL_VERSION_1_5
    const EGLint contextAttribs[] = {
        EGL_CONTEXT_MAJOR_VERSION,           4,       EGL_CONTEXT_MINOR_VERSION, 6, EGL_CONTEXT_OPENGL_PROFILE_MASK,
        EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT, EGL_NONE};
#else
    const EGLint contextAttribs[] = {EGL_NONE};
#endif
    EGLContext ctx = eglCreateContext(eglDisplay, config, EGL_NO_CONTEXT, contextAttribs);
    if (ctx == EGL_NO_CONTEXT)
        throw GraphicsException("eglCreateContext failed on Wayland");

    wl_egl_window *eglWin = wl_egl_window_create(surface_, windowDetails_.width, windowDetails_.height);
    if (!eglWin)
        throw GraphicsException("wl_egl_window_create failed");

    EGLSurface eglSurf =
        eglCreateWindowSurface(eglDisplay, config, reinterpret_cast<EGLNativeWindowType>(eglWin), nullptr);
    if (eglSurf == EGL_NO_SURFACE)
    {
        wl_egl_window_destroy(eglWin);
        throw GraphicsException("eglCreateWindowSurface failed on Wayland");
    }

    eglMakeCurrent(eglDisplay, eglSurf, eglSurf, ctx);
    eglSwapInterval(eglDisplay, windowDetails_.vsync ? 1 : 0);

    eglDisplay_ = eglDisplay;
    eglContext_ = ctx;
    eglSurface_ = eglSurf;
    eglWindow_ = eglWin;
#else
    throw GraphicsException("OpenGL requested on the Wayland backend but WMA was built without EGL support");
#endif
}

void WaylandWindowManager::allocateShmBuffer(i32 width, i32 height)
{
    destroyShmBuffer();
    if (!shm_ || width <= 0 || height <= 0)
        return;

    const i32 stride = width * 4;
    const i32 size = stride * height;

    int fd = createAnonymousFile(size);
    if (fd < 0)
        throw WMAException("Failed to create Wayland shm file");

    void *data = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (data == MAP_FAILED)
    {
        close(fd);
        throw WMAException("Failed to mmap Wayland shm buffer");
    }

    wl_shm_pool *pool = wl_shm_create_pool(shm_, fd, size);
    shmBuffer_ = wl_shm_pool_create_buffer(pool, 0, width, height, stride, WL_SHM_FORMAT_XRGB8888);
    wl_shm_pool_destroy(pool);
    close(fd);

    std::memset(data, 0, static_cast<usize>(size));
    shmData_ = data;
    shmSize_ = size;
    shmWidth_ = width;
    shmHeight_ = height;
}

void WaylandWindowManager::destroyShmBuffer()
{
    if (shmBuffer_)
    {
        wl_buffer_destroy(shmBuffer_);
        shmBuffer_ = nullptr;
    }
    if (shmData_)
    {
        munmap(shmData_, static_cast<usize>(shmSize_));
        shmData_ = nullptr;
    }
    shmSize_ = 0;
    shmWidth_ = 0;
    shmHeight_ = 0;
}

void WaylandWindowManager::pollEvents()
{
    dispatch(0);
}

void WaylandWindowManager::waitEvents(int timeoutMs)
{
    dispatch(timeoutMs < 0 ? 0 : timeoutMs);
}

//! The two differ only in how long the poll below is allowed to wait, so they
//! share one body: a client that redraws on change waits here instead of on a
//! frame it is not going to draw.
void WaylandWindowManager::dispatch(int timeoutMs)
{
    if (!display_)
        return;

    while (wl_display_prepare_read(display_) != 0)
    {
        if (wl_display_dispatch_pending(display_) < 0)
        {
            windowShouldClose_ = true;
            return;
        }
    }

    if (wl_display_flush(display_) < 0 && errno != EAGAIN)
    {
        wl_display_cancel_read(display_);
        windowShouldClose_ = true;
        return;
    }

    //! read_events itself waits for the compositor with no bound; the timeout
    //! belongs here, where a zero is pollEvents' never-wait contract.
    pollfd ready{wl_display_get_fd(display_), POLLIN, 0};
    const int result = poll(&ready, 1, timeoutMs);
    if (result > 0 && (ready.revents & POLLIN))
    {
        if (wl_display_read_events(display_) < 0)
            windowShouldClose_ = true;
    }
    else
    {
        wl_display_cancel_read(display_);
        if ((result < 0 && errno != EINTR) || (ready.revents & (POLLERR | POLLHUP | POLLNVAL)))
            windowShouldClose_ = true;
    }

    if (wl_display_dispatch_pending(display_) < 0)
        windowShouldClose_ = true;
}

void WaylandWindowManager::swapBuffers()
{
#ifdef WMA_WAYLAND_HAS_GL
    if (graphicsAPI_ == GraphicsAPI::OpenGL && eglDisplay_ && eglSurface_)
    {
        eglSwapBuffers(static_cast<EGLDisplay>(eglDisplay_), static_cast<EGLSurface>(eglSurface_));
    }
#endif
}

void WaylandWindowManager::setupInputDevices()
{
    wl_display_roundtrip(display_);

    if (keyboard_)
    {
        keyboardListener_->initialize(keyboard_);
    }
    if (pointer_)
    {
        //! compositor_/shm_ are bound by the registry callback, which the
        //! roundtrip above guarantees has already run -- passing them here is
        //! what lets the listener restore the system cursor image after a
        //! hide, rather than only ever being able to hide it.
        mouseListener_->initialize(pointer_, compositor_, shm_);
    }
}

void *WaylandWindowManager::getWindowInstance()
{
    return static_cast<void *>(surface_);
}

void *WaylandWindowManager::getNativeDisplayHandle() const noexcept
{
    return static_cast<void *>(display_);
}

void *WaylandWindowManager::getGLProcAddress(const char *name) const
{
#ifdef WMA_WAYLAND_HAS_GL
    if (graphicsAPI_ != GraphicsAPI::OpenGL)
        return nullptr;
    return reinterpret_cast<void *>(eglGetProcAddress(name));
#else
    (void)name;
    return nullptr;
#endif
}

FramebufferSize WaylandWindowManager::getFramebufferSize() noexcept
{
    const i32 scale = role_ ? std::max(1, role_->bufferScale()) : 1;
    return {std::max(1, windowDetails_.width) * scale, std::max(1, windowDetails_.height) * scale};
}

SoftwareFramebuffer WaylandWindowManager::lockFramebuffer()
{
    if (graphicsAPI_ != GraphicsAPI::CPU || !shmData_)
        return {};
    return SoftwareFramebuffer{shmData_, shmWidth_, shmHeight_, shmWidth_ * 4};
}

void WaylandWindowManager::presentFramebuffer()
{
    if (graphicsAPI_ != GraphicsAPI::CPU || !shmBuffer_ || !surface_)
        return;
    wl_surface_attach(surface_, shmBuffer_, 0, 0);
    wl_surface_damage(surface_, 0, 0, shmWidth_, shmHeight_);
    wl_surface_commit(surface_);
    wl_display_flush(display_);
}

const std::vector<const char *> WaylandWindowManager::getVulkanExtensions() const
{
    return {"VK_KHR_surface", "VK_KHR_wayland_surface"};
}

WindowFlags *WaylandWindowManager::getWindowFlags() noexcept
{
    return &windowFlags_;
}
const WindowDetails *WaylandWindowManager::getWindowDetails() noexcept
{
    return &windowDetails_;
}
KeyboardListener &WaylandWindowManager::getKeyboardListener() noexcept
{
    return *keyboardListener_;
}

void WaylandWindowManager::setTextInputEnabled(bool enabled) noexcept
{
    //! Recorded only: the xkb keymap is live from the moment the compositor
    //! sends it. See IWindowManager::setTextInputEnabled.
    if (keyboardListener_)
        keyboardListener_->setTextInputEnabled(enabled);
}

bool WaylandWindowManager::isTextInputEnabled() const noexcept
{
    return keyboardListener_ && keyboardListener_->isTextInputEnabled();
}
MouseListener &WaylandWindowManager::getMouseListener() noexcept
{
    return *mouseListener_;
}
bool WaylandWindowManager::shouldClose() const
{
    return windowShouldClose_ || (role_ && role_->shouldClose());
}
WindowBackend WaylandWindowManager::getBackendType() const
{
    return WindowBackend::WAYLAND;
}
GraphicsAPI WaylandWindowManager::getGraphicsAPI() const
{
    return graphicsAPI_;
}

WmaCode WaylandWindowManager::destroy()
{
    keyboardListener_.reset();
    mouseListener_.reset();

#ifdef WMA_WAYLAND_HAS_GL
    if (eglDisplay_)
    {
        eglMakeCurrent(static_cast<EGLDisplay>(eglDisplay_), EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (eglSurface_)
        {
            eglDestroySurface(static_cast<EGLDisplay>(eglDisplay_), static_cast<EGLSurface>(eglSurface_));
            eglSurface_ = nullptr;
        }
        if (eglContext_)
        {
            eglDestroyContext(static_cast<EGLDisplay>(eglDisplay_), static_cast<EGLContext>(eglContext_));
            eglContext_ = nullptr;
        }
        eglTerminate(static_cast<EGLDisplay>(eglDisplay_));
        eglDisplay_ = nullptr;
    }
    if (eglWindow_)
    {
        wl_egl_window_destroy(static_cast<wl_egl_window *>(eglWindow_));
        eglWindow_ = nullptr;
    }
#endif

    destroyShmBuffer();

    if (keyboard_)
    {
        wl_keyboard_destroy(keyboard_);
        keyboard_ = nullptr;
    }
    if (pointer_)
    {
        wl_pointer_destroy(pointer_);
        pointer_ = nullptr;
    }
    if (seat_)
    {
        wl_seat_destroy(seat_);
        seat_ = nullptr;
    }
    if (shm_)
    {
        wl_shm_destroy(shm_);
        shm_ = nullptr;
    }

    if (xdgToplevelDecoration_)
    {
        zxdg_toplevel_decoration_v1_destroy(xdgToplevelDecoration_);
        xdgToplevelDecoration_ = nullptr;
    }
    if (xdgDecorationManager_)
    {
        zxdg_decoration_manager_v1_destroy(xdgDecorationManager_);
        xdgDecorationManager_ = nullptr;
    }
    if (xdgToplevel_)
    {
        xdg_toplevel_destroy(xdgToplevel_);
        xdgToplevel_ = nullptr;
    }
    if (xdgSurface_)
    {
        xdg_surface_destroy(xdgSurface_);
        xdgSurface_ = nullptr;
    }
    role_.reset();
    if (surface_)
    {
        wl_surface_destroy(surface_);
        surface_ = nullptr;
    }
    if (xdgWmBase_)
    {
        xdg_wm_base_destroy(xdgWmBase_);
        xdgWmBase_ = nullptr;
    }
    if (compositor_)
    {
        wl_compositor_destroy(compositor_);
        compositor_ = nullptr;
    }
    if (registry_)
    {
        wl_registry_destroy(registry_);
        registry_ = nullptr;
    }
    if (display_)
    {
        wl_display_disconnect(display_);
        display_ = nullptr;
    }

    return WmaCode::Ok;
}

void WaylandWindowManager::handleRegistryGlobal(void *data, wl_registry *registry, u32 name, const char *interface,
                                                u32 version)
{
    auto *manager = static_cast<WaylandWindowManager *>(data);

    if (strcmp(interface, wl_compositor_interface.name) == 0)
    {
        manager->compositor_ = static_cast<wl_compositor *>(
            wl_registry_bind(registry, name, &wl_compositor_interface, version < 4 ? version : 4));
    }
    else if (strcmp(interface, wl_shm_interface.name) == 0)
    {
        manager->shm_ = static_cast<wl_shm *>(wl_registry_bind(registry, name, &wl_shm_interface, 1));
    }
    else if (strcmp(interface, xdg_wm_base_interface.name) == 0)
    {
        manager->xdgWmBase_ = static_cast<xdg_wm_base *>(wl_registry_bind(registry, name, &xdg_wm_base_interface, 1));
        xdg_wm_base_add_listener(manager->xdgWmBase_, &xdgWmBaseListener_, manager);
    }
    else if (strcmp(interface, wl_seat_interface.name) == 0)
    {
        manager->seat_ = static_cast<wl_seat *>(wl_registry_bind(registry, name, &wl_seat_interface, 1));
        wl_seat_add_listener(manager->seat_, &seatListener_, manager);
    }
    else if (strcmp(interface, zxdg_decoration_manager_v1_interface.name) == 0)
    {
        manager->xdgDecorationManager_ = static_cast<zxdg_decoration_manager_v1 *>(
            wl_registry_bind(registry, name, &zxdg_decoration_manager_v1_interface, 1));
    }
}

void WaylandWindowManager::handleRegistryGlobalRemove(void *, wl_registry *, u32)
{
}

void WaylandWindowManager::handleSeatCapabilities(void *data, wl_seat *seat, u32 capabilities)
{
    auto *manager = static_cast<WaylandWindowManager *>(data);

    if (capabilities & WL_SEAT_CAPABILITY_KEYBOARD)
    {
        if (!manager->keyboard_)
        {
            manager->keyboard_ = wl_seat_get_keyboard(seat);

            /*
             * Subscribed here rather than in setupInputDevices(): the
             * compositor sends the keymap immediately in reply to
             * get_keyboard, and libwayland drops an event that reaches a proxy
             * with no listener. Attaching one roundtrip later therefore misses
             * the only keymap event there is, leaving xkb uninitialised -- keys
             * still dispatch (they come from a static evdev table) but no text
             * is ever produced, so a text field focuses and then ignores
             * everything typed into it.
             */
            manager->keyboardListener_->initialize(manager->keyboard_);
        }
    }
    else
    {
        if (manager->keyboard_)
        {
            manager->keyboardListener_->detach();
            manager->windowFlags_.focused = false;
            wl_keyboard_destroy(manager->keyboard_);
            manager->keyboard_ = nullptr;
        }
    }

    if (capabilities & WL_SEAT_CAPABILITY_POINTER)
    {
        if (!manager->pointer_)
        {
            manager->pointer_ = wl_seat_get_pointer(seat);
            manager->mouseListener_->initialize(manager->pointer_, manager->compositor_, manager->shm_);
        }
    }
    else
    {
        if (manager->pointer_)
        {
            manager->mouseListener_->detach();
            wl_pointer_destroy(manager->pointer_);
            manager->pointer_ = nullptr;
        }
    }
}

void WaylandWindowManager::handleSeatName(void *, wl_seat *, const char *)
{
}

void WaylandWindowManager::handleXdgWmBasePing(void *, xdg_wm_base *xdg_wm_base, u32 serial)
{
    xdg_wm_base_pong(xdg_wm_base, serial);
}

void WaylandWindowManager::handleXdgSurfaceConfigure(void *data, xdg_surface *xdg_surface, u32 serial)
{
    auto *manager = static_cast<WaylandWindowManager *>(data);
    xdg_surface_ack_configure(xdg_surface, serial);
    manager->configured_ = true;
}

void WaylandWindowManager::handleXdgToplevelConfigure(void *data, xdg_toplevel *, i32 width, i32 height, wl_array *)
{
    auto *manager = static_cast<WaylandWindowManager *>(data);
    if (width > 0 && height > 0 && (width != manager->windowDetails_.width || height != manager->windowDetails_.height))
    {
        manager->windowDetails_.width = width;
        manager->windowDetails_.height = height;
        manager->windowFlags_.resized = true;

        if (manager->graphicsAPI_ == GraphicsAPI::CPU)
            manager->allocateShmBuffer(width, height);

#ifdef WMA_WAYLAND_HAS_GL
        if (manager->graphicsAPI_ == GraphicsAPI::OpenGL && manager->eglWindow_)
            wl_egl_window_resize(static_cast<wl_egl_window *>(manager->eglWindow_), width, height, 0, 0);
#endif
    }
}

void WaylandWindowManager::handleXdgToplevelClose(void *data, xdg_toplevel *)
{
    auto *manager = static_cast<WaylandWindowManager *>(data);
    manager->windowShouldClose_ = true;
}

void WaylandWindowManager::handleXdgToplevelConfigureBounds(void *, xdg_toplevel *, i32, i32)
{
}

std::unique_ptr<IWindowManager> createWaylandWindowManager(const WindowDetails &details, GraphicsAPI api,
                                                           std::unique_ptr<WaylandSurfaceRole> role)
{
    return std::make_unique<WaylandWindowManager>(details, api, std::move(role));
}

} // namespace wma
