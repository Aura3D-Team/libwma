#include "wma/backends/sdl/SdlWindowManager.hpp"
#include "wma/exceptions/WMAException.hpp"

#include <ink/Inkogger.h>

#ifdef WMA_ENABLE_SDL

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#ifdef __APPLE__
//! SDL's Metal helpers. Compiled in only for Apple builds: SDL_Metal_CreateView
//! has no implementation on any other platform, so a GraphicsAPI::Metal request
//! elsewhere is rejected at createWindow() rather than left to fail obscurely.
#include <SDL3/SDL_metal.h>
#endif

#include <atomic>
#include <chrono>
#include <thread>
#include <utility>

namespace wma
{

//! SDL_Init/SDL_Quit are process-global. Reference-count them so multiple windows
//! can coexist and moving/destroying one never tears SDL down for a live one.
namespace
{
std::atomic<int> g_sdlRefCount{0};
}

SdlWindowManager::SdlWindowManager(const WindowDetails &windowDetails, GraphicsAPI graphicsAPI)
    : window_(nullptr), glContext_(nullptr), windowSurface_(nullptr), metalView_(nullptr), metalLayer_(nullptr),
      windowDetails_(windowDetails), windowFlags_{}, graphicsAPI_(graphicsAPI),
      keyboardListener_(std::make_unique<SDLKeyboardListener>()), mouseListener_(std::make_unique<SDLMouseListener>()),
      touchListener_(std::make_unique<SDLTouchListener>()), windowShouldClose_(false), ownsSubsystem_(false)
{
    initializeSDL();
}

SdlWindowManager::~SdlWindowManager()
{
    SdlWindowManager::destroy();
}

SdlWindowManager::SdlWindowManager(SdlWindowManager &&other) noexcept
    : window_(std::exchange(other.window_, nullptr)), glContext_(std::exchange(other.glContext_, nullptr)),
      windowSurface_(std::exchange(other.windowSurface_, nullptr)),
      metalView_(std::exchange(other.metalView_, nullptr)), metalLayer_(std::exchange(other.metalLayer_, nullptr)),
      windowDetails_(other.windowDetails_), windowFlags_(other.windowFlags_), graphicsAPI_(other.graphicsAPI_),
      keyboardListener_(std::move(other.keyboardListener_)), mouseListener_(std::move(other.mouseListener_)),
      touchListener_(std::move(other.touchListener_)), windowShouldClose_(other.windowShouldClose_),
      ownsSubsystem_(std::exchange(other.ownsSubsystem_, false))
{
}

SdlWindowManager &SdlWindowManager::operator=(SdlWindowManager &&other) noexcept
{
    if (this != &other)
    {
        destroy();
        window_ = std::exchange(other.window_, nullptr);
        glContext_ = std::exchange(other.glContext_, nullptr);
        windowSurface_ = std::exchange(other.windowSurface_, nullptr);
        metalView_ = std::exchange(other.metalView_, nullptr);
        metalLayer_ = std::exchange(other.metalLayer_, nullptr);
        windowDetails_ = other.windowDetails_;
        windowFlags_ = other.windowFlags_;
        graphicsAPI_ = other.graphicsAPI_;
        keyboardListener_ = std::move(other.keyboardListener_);
        mouseListener_ = std::move(other.mouseListener_);
        touchListener_ = std::move(other.touchListener_);
        windowShouldClose_ = other.windowShouldClose_;
        ownsSubsystem_ = std::exchange(other.ownsSubsystem_, false);
    }
    return *this;
}

void SdlWindowManager::createWindow(const char *windowName)
{
    SDL_WindowFlags windowFlags = 0;
    if (windowDetails_.resizable)
        windowFlags |= SDL_WINDOW_RESIZABLE;
    if (windowDetails_.fullscreen)
        windowFlags |= SDL_WINDOW_FULLSCREEN;

    switch (graphicsAPI_)
    {
    case GraphicsAPI::Vulkan:
        windowFlags |= SDL_WINDOW_VULKAN;
        break;
    case GraphicsAPI::OpenGL:
        windowFlags |= SDL_WINDOW_OPENGL;
        break;
    case GraphicsAPI::CPU:
        break;
    case GraphicsAPI::Metal:
#ifdef __APPLE__
        windowFlags |= SDL_WINDOW_METAL;
        break;
#else
        throw GraphicsException("Metal is an Apple-only graphics API; this build of wma targets "
                                "another platform (use Vulkan/OpenGL/CPU)");
#endif
    default:
        throw GraphicsException("Unsupported graphics API for SDL");
    }

#ifdef __EMSCRIPTEN__
    // Emscripten's OpenGL backend is WebGL2 (Aura3D compiles with
    // -sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2, since its ES3-only
    // entry points like glBindBufferBase need it). SDL_GL_CreateContext
    // otherwise requests a default profile that maps to WebGL1, which
    // the browser then refuses outright. Attributes must be set before
    // SDL_CreateWindow when the window will host a GL context.
    if (graphicsAPI_ == GraphicsAPI::OpenGL)
    {
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    }
#endif

    window_ = SDL_CreateWindow(windowName, windowDetails_.width, windowDetails_.height, windowFlags);

    if (!window_)
    {
        throw WindowException("Failed to create SDL window: " + std::string(SDL_GetError()));
    }

    if (graphicsAPI_ == GraphicsAPI::OpenGL)
    {
        glContext_ = SDL_GL_CreateContext(window_);
        if (!glContext_)
        {
            SDL_DestroyWindow(window_);
            window_ = nullptr;
            throw GraphicsException("Failed to create OpenGL context: " + std::string(SDL_GetError()));
        }
        SDL_GL_MakeCurrent(window_, static_cast<SDL_GLContext>(glContext_));
        SDL_GL_SetSwapInterval(windowDetails_.vsync ? 1 : 0);
    }

#ifdef __APPLE__
    if (graphicsAPI_ == GraphicsAPI::Metal)
    {
        /*
         * SDL adds the platform's Metal-backed view (NSView on macOS, UIView
         * on iOS) to the window and gives its CAMetalLayer the right
         * contentsScale for the display -- which is why the SDL route needs
         * no AppKit/UIKit code here, unlike the GLFW one.
         *
         * The layer arrives with no MTLDevice attached; that is the
         * renderer's to set, along with the pixel format. wma deliberately
         * does not choose either: both are the renderer's contract with its
         * own pipelines, and a window library guessing at them is how a
         * backend ends up fighting its own swapchain format.
         */
        metalView_ = SDL_Metal_CreateView(window_);
        if (!metalView_)
        {
            SDL_DestroyWindow(window_);
            window_ = nullptr;
            throw GraphicsException("Failed to create SDL Metal view: " + std::string(SDL_GetError()));
        }

        metalLayer_ = SDL_Metal_GetLayer(metalView_);
        if (!metalLayer_)
        {
            SDL_Metal_DestroyView(metalView_);
            metalView_ = nullptr;
            SDL_DestroyWindow(window_);
            window_ = nullptr;
            throw GraphicsException("SDL Metal view has no CAMetalLayer");
        }
    }
#endif

    keyboardListener_->initialize(window_);
    mouseListener_->initialize(window_);
    touchListener_->initialize(window_);
    INK_LOG << "SDL window created: " << windowName;
}

void SdlWindowManager::waitEvents(int timeoutMs)
{
#ifdef __EMSCRIPTEN__
    //! The browser owns the loop; a wait here stalls the tab.
    (void)timeoutMs;
#else
    //! A null event leaves the first arrival in the queue for pollEvents()
    //! below, so the wait adds nothing to the dispatch path.
    if (timeoutMs > 0)
        SDL_WaitEventTimeout(nullptr, timeoutMs);
#endif
    pollEvents();
}

void SdlWindowManager::pollEvents()
{
#ifdef __ANDROID__
    // Detect the ANativeWindow being torn down/replaced -- Android does
    // this around Activity backgrounding/foregrounding without SDL
    // exposing a dedicated queued event for it (SDL_EVENT_WILL_ENTER_*
    // background/foreground events require a separate SDL_AddEventWatch
    // callback, delivered on whatever thread posts them, which doesn't
    // fit this single-threaded poll loop). Comparing this property each
    // call is cheap and covers both directions: valid -> null (lost) and
    // null -> a *different* valid pointer (recreated).
    if (window_)
    {
        void *currentNativeWindow =
            SDL_GetPointerProperty(SDL_GetWindowProperties(window_), SDL_PROP_WINDOW_ANDROID_WINDOW_POINTER, nullptr);

        if (currentNativeWindow != lastNativeWindow_)
        {
            windowFlags_.surfaceLost = true;
            lastNativeWindow_ = currentNativeWindow;
        }
    }
#endif

    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        switch (event.type)
        {
        case SDL_EVENT_QUIT:
            windowShouldClose_ = true;
            break;
        case SDL_EVENT_WINDOW_RESIZED:
        case SDL_EVENT_WINDOW_FOCUS_GAINED:
        case SDL_EVENT_WINDOW_FOCUS_LOST:
        case SDL_EVENT_WINDOW_MINIMIZED:
        case SDL_EVENT_WINDOW_RESTORED:
            handleWindowEvent(&event);
            break;
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP:
            keyboardListener_->handleKeyEvent(event.key);
            break;
        case SDL_EVENT_TEXT_INPUT:
            keyboardListener_->handleTextInputEvent(event.text);
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP:
        case SDL_EVENT_MOUSE_MOTION:
        case SDL_EVENT_MOUSE_WHEEL:
            mouseListener_->handleEvent(event);
            break;
        case SDL_EVENT_FINGER_DOWN:
        case SDL_EVENT_FINGER_MOTION:
        case SDL_EVENT_FINGER_UP:
            touchListener_->handleEvent(event);
            break;
        default:
            break;
        }
    }
}

void SdlWindowManager::swapBuffers()
{
    if (graphicsAPI_ == GraphicsAPI::OpenGL && window_)
        SDL_GL_SwapWindow(window_);
}

void *SdlWindowManager::getWindowInstance()
{
    return window_;
}

bool SdlWindowManager::isSurfaceAvailable() const
{
#ifdef __ANDROID__
    if (!window_)
        return false;
    return SDL_GetPointerProperty(SDL_GetWindowProperties(window_), SDL_PROP_WINDOW_ANDROID_WINDOW_POINTER, nullptr) !=
           nullptr;
#else
    return window_ != nullptr;
#endif
}

bool SdlWindowManager::waitUntilWindowReady()
{
    if (!window_)
        return false;

#ifdef __ANDROID__
    using namespace std::chrono_literals;
    constexpr int kMaxAttempts = 100;       // ~5s at 50ms, then give up rather than hang
    constexpr int kRequiredStablePolls = 4; // ~200ms unchanged before trusting it

    int stablePolls = 0;
    void *lastNativeWindow = nullptr;
    int lastW = -1;
    int lastH = -1;

    for (int attempt = 0; attempt < kMaxAttempts; ++attempt)
    {
        // Pumping is what makes this work: without it SDL never processes
        // the surfaceDestroyed/surfaceChanged callbacks queued by the UI
        // thread, so its cached native-window pointer stays *stale* --
        // non-null but already released. Pumping lets SDL drop the dead
        // window and pick up a genuinely current one. Safe here because
        // SDL runs the app's main function on the thread owning its event
        // queue, which is the thread calling this.
        SDL_PumpEvents();

        void *nativeWindow =
            SDL_GetPointerProperty(SDL_GetWindowProperties(window_), SDL_PROP_WINDOW_ANDROID_WINDOW_POINTER, nullptr);

        int w = 0;
        int h = 0;
        const bool usable = nativeWindow != nullptr && SDL_GetWindowSizeInPixels(window_, &w, &h) && w > 0 && h > 0;

        // Require the *same* native window pointer across polls, not merely
        // some non-null one: a pointer that changed since the last poll
        // means SDL just swapped it out, so the churn hasn't settled yet.
        if (usable && nativeWindow == lastNativeWindow && w == lastW && h == lastH)
        {
            if (++stablePolls >= kRequiredStablePolls)
                return true;
        }
        else
        {
            stablePolls = 0;
        }

        lastNativeWindow = usable ? nativeWindow : nullptr;
        lastW = usable ? w : -1;
        lastH = usable ? h : -1;
        std::this_thread::sleep_for(50ms);
    }
    return false;
#else
    return true;
#endif
}

void *SdlWindowManager::getNativeDisplayHandle() const noexcept
{
    return nullptr;
}

void *SdlWindowManager::getGLProcAddress(const char *name) const
{
    if (graphicsAPI_ != GraphicsAPI::OpenGL)
        return nullptr;
    return reinterpret_cast<void *>(SDL_GL_GetProcAddress(name));
}

void *SdlWindowManager::getMetalLayer() const noexcept
{
    //! Null on every non-Metal window, and on every non-Apple build, where
    //! createWindow() has already refused the request.
    return metalLayer_;
}

SoftwareFramebuffer SdlWindowManager::lockFramebuffer()
{
    if (graphicsAPI_ != GraphicsAPI::CPU || !window_)
        return {};

    SDL_Surface *surface = SDL_GetWindowSurface(window_);
    if (!surface)
        return {};

    windowSurface_ = surface;
    if (SDL_MUSTLOCK(surface))
        SDL_LockSurface(surface);

    return SoftwareFramebuffer{surface->pixels, surface->w, surface->h, surface->pitch};
}

void SdlWindowManager::presentFramebuffer()
{
    if (!windowSurface_ || !window_)
        return;
    auto *surface = static_cast<SDL_Surface *>(windowSurface_);
    if (SDL_MUSTLOCK(surface))
        SDL_UnlockSurface(surface);
    SDL_UpdateWindowSurface(window_);
    windowSurface_ = nullptr;
}

u64 SdlWindowManager::getSDLWindowFlags() const
{
    return window_ ? SDL_GetWindowFlags(window_) : 0;
}

FramebufferSize SdlWindowManager::getFramebufferSize() noexcept
{
    int width = 0;
    int height = 0;

    //! SDL_GetWindowSizeInPixels, not SDL_GetWindowSize: the latter reports
    //! the logical size, which the base implementation already covers.
    if (window_)
        SDL_GetWindowSizeInPixels(window_, &width, &height);

    return FramebufferSize{width > 0 ? width : 1, height > 0 ? height : 1};
}

WindowFlags *SdlWindowManager::getWindowFlags() noexcept
{
    return &windowFlags_;
}
const WindowDetails *SdlWindowManager::getWindowDetails() noexcept
{
    return &windowDetails_;
}

const std::vector<const char *> SdlWindowManager::getVulkanExtensions() const
{
    u32 extensionCount = 0;
    const char *const *extNames = SDL_Vulkan_GetInstanceExtensions(&extensionCount);

    if (!extNames)
        throw GraphicsException("Failed to get Vulkan extensions: " + std::string(SDL_GetError()));

    return std::vector<const char *>(extNames, extNames + extensionCount);
}

KeyboardListener &SdlWindowManager::getKeyboardListener() noexcept
{
    return *keyboardListener_;
}

void SdlWindowManager::setTextInputEnabled(bool enabled) noexcept
{
    //! SDL is the one backend where this genuinely gates delivery -- and,
    //! on Android/iOS, raises the on-screen keyboard.
    if (keyboardListener_)
        keyboardListener_->setTextInputEnabled(enabled);
}

bool SdlWindowManager::isTextInputEnabled() const noexcept
{
    return keyboardListener_ && keyboardListener_->isTextInputEnabled();
}
MouseListener &SdlWindowManager::getMouseListener() noexcept
{
    return *mouseListener_;
}
TouchListener &SdlWindowManager::getTouchListener() noexcept
{
    return *touchListener_;
}
bool SdlWindowManager::shouldClose() const
{
    return windowShouldClose_;
}
WindowBackend SdlWindowManager::getBackendType() const
{
    return WindowBackend::SDL3;
}
GraphicsAPI SdlWindowManager::getGraphicsAPI() const
{
    return graphicsAPI_;
}

void SdlWindowManager::handleWindowEvent(const SDL_Event *event)
{
    switch (event->type)
    {
    case SDL_EVENT_WINDOW_RESIZED:
        windowDetails_.width = event->window.data1;
        windowDetails_.height = event->window.data2;
        windowFlags_.resized = true;
        break;
    case SDL_EVENT_WINDOW_FOCUS_GAINED:
        windowFlags_.focused = true;
        break;
    case SDL_EVENT_WINDOW_FOCUS_LOST:
        windowFlags_.focused = false;
        //! Keys released while another window had focus never reach us,
        //! so anything held at this moment would otherwise stay stuck
        //! down -- most visibly a modifier held through an Alt-Tab.
        keyboardListener_->releaseAllKeys();
        break;
    case SDL_EVENT_WINDOW_MINIMIZED:
        windowFlags_.minimized = true;
        break;
    case SDL_EVENT_WINDOW_RESTORED:
        windowFlags_.minimized = false;
        break;
    default:
        break;
    }
}

void SdlWindowManager::initializeSDL()
{
    if (g_sdlRefCount.fetch_add(1, std::memory_order_acq_rel) == 0)
    {
        if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
        {
            g_sdlRefCount.fetch_sub(1, std::memory_order_acq_rel);
            throw WMAException("Failed to initialize SDL: " + std::string(SDL_GetError()));
        }
    }
    ownsSubsystem_ = true;

    if (graphicsAPI_ == GraphicsAPI::OpenGL)
    {
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 6);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
        SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    }

    if (graphicsAPI_ == GraphicsAPI::Vulkan)
    {
        if (!SDL_Vulkan_LoadLibrary(nullptr))
        {
            if (ownsSubsystem_ && g_sdlRefCount.fetch_sub(1, std::memory_order_acq_rel) == 1)
                SDL_Quit();

            ownsSubsystem_ = false;
            throw GraphicsException("Failed to load Vulkan library: " + std::string(SDL_GetError()));
        }
    }
    mouseListener_->setSensitivity(1.0);
}

WmaCode SdlWindowManager::destroy()
{
    windowShouldClose_ = true;

    if (glContext_)
    {
        SDL_GL_DestroyContext(static_cast<SDL_GLContext>(glContext_));
        glContext_ = nullptr;
    }

    if (graphicsAPI_ == GraphicsAPI::Vulkan && ownsSubsystem_)
        SDL_Vulkan_UnloadLibrary();

#ifdef __APPLE__
    //! Before SDL_DestroyWindow, as SDL_Metal_CreateView's contract requires.
    //! The layer belongs to the view, so it dies with it.
    if (metalView_)
    {
        SDL_Metal_DestroyView(metalView_);
        metalView_ = nullptr;
    }
#endif
    metalLayer_ = nullptr;

    if (window_)
    {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }

    if (ownsSubsystem_)
    {
        ownsSubsystem_ = false;
        if (g_sdlRefCount.fetch_sub(1, std::memory_order_acq_rel) == 1)
            SDL_Quit();
    }
    return WmaCode::Ok;
}

} // namespace wma
#endif // WMA_ENABLE_SDL
