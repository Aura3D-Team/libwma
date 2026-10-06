#ifdef WMA_ENABLE_X11
#include "wma/backends/x11/X11WindowManager.hpp"
#include "backends/x11/X11MoveResize.hpp"
#include "wma/exceptions/WMAException.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
#include <poll.h>
#include <span>
#include <utility>

//! For XkbSetDetectableAutoRepeat; see createWindow().
#include <X11/XKBlib.h>
#include <X11/Xatom.h>

#ifdef WMA_X11_HAS_GL
#include <GL/glx.h>
#endif

namespace wma
{

X11WindowManager::X11WindowManager(const WindowDetails &windowDetails, GraphicsAPI graphicsAPI)
    : display_(nullptr), window_(0), colormap_(0), wmDeleteWindow_(0), gc_(nullptr), image_(nullptr),
      glContext_(nullptr), fbConfig_(nullptr), windowDetails_(windowDetails), windowFlags_{}, graphicsAPI_(graphicsAPI),
      keyboardListener_(std::make_unique<X11KeyboardListener>()), mouseListener_(std::make_unique<X11MouseListener>()),
      windowShouldClose_(false)
{
    mouseListener_->owner_ = this;
}

X11WindowManager::~X11WindowManager()
{
    X11WindowManager::destroy();
}

X11WindowManager::X11WindowManager(X11WindowManager &&other) noexcept
    : display_(std::exchange(other.display_, nullptr)), window_(std::exchange(other.window_, 0)),
      colormap_(std::exchange(other.colormap_, 0)), wmDeleteWindow_(other.wmDeleteWindow_),
      netWmState_(other.netWmState_), netWmStateMaximizedVert_(other.netWmStateMaximizedVert_),
      netWmStateMaximizedHorz_(other.netWmStateMaximizedHorz_), netWmName_(other.netWmName_),
      utf8String_(other.utf8String_), maximized_(std::exchange(other.maximized_, false)),
      gc_(std::exchange(other.gc_, nullptr)), image_(std::exchange(other.image_, nullptr)),
      glContext_(std::exchange(other.glContext_, nullptr)), fbConfig_(other.fbConfig_),
      windowDetails_(other.windowDetails_), windowFlags_(other.windowFlags_), graphicsAPI_(other.graphicsAPI_),
      keyboardListener_(std::move(other.keyboardListener_)), mouseListener_(std::move(other.mouseListener_)),
      windowShouldClose_(other.windowShouldClose_), hitTest_(std::move(other.hitTest_)), caption_(other.caption_),
      captionButton_(other.captionButton_)
{
    if (mouseListener_)
        mouseListener_->owner_ = this;
}

X11WindowManager &X11WindowManager::operator=(X11WindowManager &&other) noexcept
{
    if (this != &other)
    {
        destroy();
        display_ = std::exchange(other.display_, nullptr);
        window_ = std::exchange(other.window_, 0);
        colormap_ = std::exchange(other.colormap_, 0);
        wmDeleteWindow_ = other.wmDeleteWindow_;
        netWmState_ = other.netWmState_;
        netWmStateMaximizedVert_ = other.netWmStateMaximizedVert_;
        netWmStateMaximizedHorz_ = other.netWmStateMaximizedHorz_;
        netWmName_ = other.netWmName_;
        utf8String_ = other.utf8String_;
        maximized_ = std::exchange(other.maximized_, false);
        gc_ = std::exchange(other.gc_, nullptr);
        image_ = std::exchange(other.image_, nullptr);
        glContext_ = std::exchange(other.glContext_, nullptr);
        fbConfig_ = other.fbConfig_;
        windowDetails_ = other.windowDetails_;
        windowFlags_ = other.windowFlags_;
        graphicsAPI_ = other.graphicsAPI_;
        keyboardListener_ = std::move(other.keyboardListener_);
        mouseListener_ = std::move(other.mouseListener_);
        windowShouldClose_ = other.windowShouldClose_;
        hitTest_ = std::move(other.hitTest_);
        caption_ = other.caption_;
        captionButton_ = other.captionButton_;
        if (mouseListener_)
            mouseListener_->owner_ = this;
    }
    return *this;
}

void X11WindowManager::createWindow(const char *windowName)
{
    /*
     * Rejected up front rather than through a switch: this backend dispatches on
     * graphicsAPI_ with if/else chains, so an unhandled value would quietly open
     * an ordinary X window and leave getMetalLayer() returning nullptr -- the
     * failure would then surface inside the renderer, several layers away from
     * the mistaken setting that caused it.
     */
    if (graphicsAPI_ == GraphicsAPI::Metal)
    {
        throw GraphicsException("Metal is an Apple-only graphics API and X11 is a Linux/Unix display "
                                "protocol; the two can never pair (use Vulkan/OpenGL/CPU)");
    }

    display_ = XOpenDisplay(nullptr);
    if (!display_)
    {
        throw WindowException("Failed to open X11 display (is DISPLAY set?)");
    }

    const int screen = DefaultScreen(display_);
    Window rootWindow = RootWindow(display_, screen);

    Visual *visual = DefaultVisual(display_, screen);
    int depth = DefaultDepth(display_, screen);

#ifdef WMA_X11_HAS_GL
    if (graphicsAPI_ == GraphicsAPI::OpenGL)
    {
        static int fbAttribs[] = {GLX_X_RENDERABLE,
                                  True,
                                  GLX_DRAWABLE_TYPE,
                                  GLX_WINDOW_BIT,
                                  GLX_RENDER_TYPE,
                                  GLX_RGBA_BIT,
                                  GLX_X_VISUAL_TYPE,
                                  GLX_TRUE_COLOR,
                                  GLX_RED_SIZE,
                                  8,
                                  GLX_GREEN_SIZE,
                                  8,
                                  GLX_BLUE_SIZE,
                                  8,
                                  GLX_ALPHA_SIZE,
                                  8,
                                  GLX_DEPTH_SIZE,
                                  24,
                                  GLX_STENCIL_SIZE,
                                  8,
                                  GLX_DOUBLEBUFFER,
                                  True,
                                  None};
        int fbCount = 0;
        GLXFBConfig *fbc = glXChooseFBConfig(display_, screen, fbAttribs, &fbCount);
        if (!fbc || fbCount == 0)
        {
            if (fbc)
                XFree(static_cast<void *>(fbc));
            XCloseDisplay(display_);
            display_ = nullptr;
            throw GraphicsException("No suitable GLX framebuffer configuration found");
        }
        fbConfig_ = fbc[0];
        XVisualInfo *vi = glXGetVisualFromFBConfig(display_, static_cast<GLXFBConfig>(fbConfig_));
        if (!vi)
        {
            XFree(static_cast<void *>(fbc));
            XCloseDisplay(display_);
            display_ = nullptr;
            throw GraphicsException("glXGetVisualFromFBConfig returned no visual");
        }
        visual = vi->visual;
        depth = vi->depth;
        colormap_ = XCreateColormap(display_, rootWindow, visual, AllocNone);
        XFree(vi);
        XFree(static_cast<void *>(fbc));
    }
#endif

    XSetWindowAttributes windowAttributes{};
    //! FocusChangeMask is what lets held keys be cleared on focus loss (see
    //! pollEvents): releases that happen while another window has focus are
    //! never delivered here, so a modifier held through an Alt-Tab would
    //! otherwise stay down forever. PropertyChangeMask tracks _NET_WM_STATE.
    windowAttributes.event_mask = ExposureMask | KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask |
                                  PointerMotionMask | StructureNotifyMask | FocusChangeMask | PropertyChangeMask;
    unsigned long valueMask = CWEventMask;
    if (colormap_)
    {
        windowAttributes.colormap = colormap_;
        windowAttributes.background_pixel = 0;
        windowAttributes.border_pixel = 0;
        valueMask |= CWColormap | CWBackPixel | CWBorderPixel;
    }

    window_ = XCreateWindow(display_, rootWindow, 100, 100, windowDetails_.width, windowDetails_.height, 0, depth,
                            InputOutput, visual, valueMask, &windowAttributes);

    if (!window_)
    {
        XCloseDisplay(display_);
        display_ = nullptr;
        throw WindowException("Failed to create X11 window");
    }

    //! One round trip for every atom the window uses afterwards.
    std::array<const char *, 5> atomNames{"_NET_WM_STATE", "_NET_WM_STATE_MAXIMIZED_VERT",
                                          "_NET_WM_STATE_MAXIMIZED_HORZ", "_NET_WM_NAME", "UTF8_STRING"};
    std::array<Atom, atomNames.size()> atoms{};
    XInternAtoms(display_, const_cast<char **>(atomNames.data()), static_cast<int>(atomNames.size()), False,
                 atoms.data());
    netWmState_ = atoms[0];
    netWmStateMaximizedVert_ = atoms[1];
    netWmStateMaximizedHorz_ = atoms[2];
    netWmName_ = atoms[3];
    utf8String_ = atoms[4];

    setTitle(windowName);

    if (windowDetails_.decorationMode == DecorationMode::ClientSide)
    {
        //! Motif hints are the de facto WM convention for suppressing the frame.
        const Atom motifHints = XInternAtom(display_, "_MOTIF_WM_HINTS", False);
        const std::array<unsigned long, 5> hints{1UL << 1, 0, 0, 0, 0};
        XChangeProperty(display_, window_, motifHints, motifHints, 32, PropModeReplace,
                        reinterpret_cast<const unsigned char *>(hints.data()), static_cast<int>(hints.size()));
    }

    wmDeleteWindow_ = XInternAtom(display_, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(display_, window_, &wmDeleteWindow_, 1);

    XMapWindow(display_, window_);
    XFlush(display_);

    keyboardListener_->initialize(display_);

    /*
     * Without detectable auto-repeat the server brackets every repeat with a
     * synthetic KeyRelease/KeyPress pair, so a held key looks like a stream of
     * fresh presses -- bound actions would re-fire, and the listener could not
     * tell a repeat from a genuine press. Best-effort: an ancient server
     * without XKB simply leaves `supported` False and behaves as before.
     */
    Bool detectableAutoRepeatSupported = False;
    XkbSetDetectableAutoRepeat(display_, True, &detectableAutoRepeatSupported);

    //! After the window exists: the input context is bound to it.
    keyboardListener_->attachWindow(window_);

    mouseListener_->initialize(display_, window_);

    if (graphicsAPI_ == GraphicsAPI::OpenGL)
    {
        initGL();
    }
    else if (graphicsAPI_ == GraphicsAPI::CPU)
    {
        gc_ = XCreateGC(display_, window_, 0, nullptr);
        allocateSoftwareImage(windowDetails_.width, windowDetails_.height);
    }
}

bool X11WindowManager::minimize() noexcept
{
    if (!display_ || !window_)
        return false;
    const bool sent = XIconifyWindow(display_, window_, DefaultScreen(display_)) != 0;
    XFlush(display_);
    return sent;
}

bool X11WindowManager::requestMaximized(bool maximized) noexcept
{
    if (!display_ || !window_)
        return false;

    XEvent event{};
    event.xclient.type = ClientMessage;
    event.xclient.window = window_;
    event.xclient.message_type = netWmState_;
    event.xclient.format = 32;
    event.xclient.data.l[0] = maximized ? 1 : 0; // EWMH ADD / REMOVE
    event.xclient.data.l[1] = static_cast<long>(netWmStateMaximizedVert_);
    event.xclient.data.l[2] = static_cast<long>(netWmStateMaximizedHorz_);
    event.xclient.data.l[3] = 1; // Application source, not a pager.
    const bool sent = XSendEvent(display_, DefaultRootWindow(display_), False,
                                 SubstructureRedirectMask | SubstructureNotifyMask, &event) != 0;
    XFlush(display_);
    return sent;
}

bool X11WindowManager::maximize() noexcept
{
    return requestMaximized(true);
}

bool X11WindowManager::restore() noexcept
{
    if (!display_ || !window_)
        return false;
    XMapWindow(display_, window_);
    return requestMaximized(false);
}

bool X11WindowManager::isMaximized() const noexcept
{
    return maximized_;
}

void X11WindowManager::refreshMaximized() noexcept
{
    maximized_ = false;
    if (!display_ || !window_)
        return;

    Atom actualType = None;
    int actualFormat = 0;
    unsigned long count = 0;
    unsigned long remaining = 0;
    unsigned char *property = nullptr;
    const int result = XGetWindowProperty(display_, window_, netWmState_, 0, 1024, False, XA_ATOM, &actualType,
                                          &actualFormat, &count, &remaining, &property);
    std::unique_ptr<unsigned char, decltype(&XFree)> owner(property, &XFree);
    if (result != Success || actualType != XA_ATOM || actualFormat != 32 || !property)
        return;

    //! Format-32 properties arrive as long-sized items, which is what Atom is.
    const std::span atoms{reinterpret_cast<const Atom *>(property), count};
    maximized_ = std::ranges::find(atoms, netWmStateMaximizedVert_) != atoms.end() &&
                 std::ranges::find(atoms, netWmStateMaximizedHorz_) != atoms.end();
}

void X11WindowManager::close() noexcept
{
    windowShouldClose_ = true;
}

bool X11WindowManager::setHitTest(HitTest hitTest)
{
    if (!display_ || !window_)
        return false;
    hitTest_ = std::move(hitTest);
    return true;
}

bool X11WindowManager::claimPress(const XButtonEvent &press)
{
    if (!hitTest_)
        return false;

    const f64 x = static_cast<f64>(press.x);
    const f64 y = static_cast<f64>(press.y);
    const WindowHit hit = hitTest_(x, y);
    if (hit == WindowHit::Client || (hit != WindowHit::Caption && !windowDetails_.resizable))
        return false;
    if (hit == WindowHit::Caption)
    {
        if (caption_.press(x, y))
            detail::toggleMaximized(*this);
        else
            captionButton_ = press.button;
        return true;
    }
    return x11::startMoveResize(display_, window_, hit, press.x_root, press.y_root, press.button);
}

void X11WindowManager::dragTo(const XMotionEvent &motion)
{
    if (caption_.moved(static_cast<f64>(motion.x), static_cast<f64>(motion.y)))
        x11::startMoveResize(display_, window_, WindowHit::Caption, motion.x_root, motion.y_root, captionButton_);
}

void X11WindowManager::releaseClaim() noexcept
{
    caption_.released();
}

SystemCursor X11WindowManager::hoverCursor(f64 x, f64 y) const
{
    return hitTest_ && windowDetails_.resizable ? detail::cursorFor(hitTest_(x, y)) : SystemCursor::Default;
}

bool X11WindowManager::setTitle(const char *title) noexcept
{
    if (!display_ || !window_ || !title)
        return false;
    const usize length = std::strlen(title);
    if (length > static_cast<usize>(std::numeric_limits<int>::max()))
        return false;

    XStoreName(display_, window_, title);
    XChangeProperty(display_, window_, netWmName_, utf8String_, 8, PropModeReplace,
                    reinterpret_cast<const unsigned char *>(title), static_cast<int>(length));
    XFlush(display_);
    return true;
}

DecorationMode X11WindowManager::getDecorationMode() const noexcept
{
    return windowDetails_.decorationMode;
}

void X11WindowManager::initGL()
{
#ifdef WMA_X11_HAS_GL
    glContext_ = glXCreateNewContext(display_, static_cast<GLXFBConfig>(fbConfig_), GLX_RGBA_TYPE, nullptr, True);
    if (!glContext_)
        throw GraphicsException("Failed to create GLX (OpenGL) context");

    glXMakeCurrent(display_, window_, static_cast<GLXContext>(glContext_));

    //! Best-effort vsync via GLX_EXT_swap_control.
    using PFNGLXSWAPINTERVALEXTPROC = void (*)(Display *, GLXDrawable, int);
    auto swapIntervalEXT = reinterpret_cast<PFNGLXSWAPINTERVALEXTPROC>(
        glXGetProcAddressARB(reinterpret_cast<const GLubyte *>("glXSwapIntervalEXT")));

    if (swapIntervalEXT)
        swapIntervalEXT(display_, window_, windowDetails_.vsync ? 1 : 0);

#else
    throw GraphicsException("OpenGL requested on the X11 backend but WMA was built without GLX support");
#endif
}

void X11WindowManager::allocateSoftwareImage(i32 width, i32 height)
{
    destroySoftwareImage();
    if (width <= 0 || height <= 0)
        return;

    const int screen = DefaultScreen(display_);
    Visual *visual = DefaultVisual(display_, screen);
    const int depth = DefaultDepth(display_, screen);

    //! 32-bit ZPixmap buffer owned by the XImage (XDestroyImage frees data).
    std::unique_ptr<char, decltype(&std::free)> buffer(
        static_cast<char *>(std::malloc(static_cast<usize>(width) * height * 4)), &std::free);
    if (!buffer)
        throw WMAException("Out of memory allocating software framebuffer");
    std::memset(buffer.get(), 0, static_cast<usize>(width) * height * 4);

    image_ = XCreateImage(display_, visual, depth, ZPixmap, 0, buffer.get(), width, height, 32, 0);
    if (!image_)
    {
        throw WMAException("Failed to create X11 software image");
    }
    image_->data = buffer.release();
}

void X11WindowManager::destroySoftwareImage()
{
    if (image_)
    {
        XDestroyImage(image_); // also frees image_->data
        image_ = nullptr;
    }
}

void X11WindowManager::waitEvents(int timeoutMs)
{
    //! XPending flushes and counts what is already queued; only an empty
    //! queue is worth sleeping on.
    if (display_ && timeoutMs > 0 && XPending(display_) == 0)
    {
        pollfd ready{ConnectionNumber(display_), POLLIN, 0};
        poll(&ready, 1, timeoutMs);
    }
    pollEvents();
}

void X11WindowManager::pollEvents()
{
    if (!display_)
        return;

    while (XPending(display_) > 0)
    {
        XEvent event;
        XNextEvent(display_, &event);

        /*
         * Offered to the input method first, as XFilterEvent's contract
         * requires. A keystroke being composed (a dead key, or an IME
         * candidate selection) is consumed here and must not also be handled
         * as an ordinary key press, or a compose sequence would both compose
         * *and* type its raw keys.
         */
        if (keyboardListener_->filterEvent(&event))
            continue;

        handleWindowEvent(&event);

        switch (event.type)
        {
        case Expose:
            break;
        case KeyPress:
        case KeyRelease:
            keyboardListener_->handleKeyEvent(XLookupKeysym(&event.xkey, 0), event.xkey);
            break;
        case FocusIn:
            windowFlags_.focused = true;
            break;
        case FocusOut:
            windowFlags_.focused = false;
            //! See FocusChangeMask in createWindow().
            keyboardListener_->releaseAllKeys();
            break;
        case ButtonPress:
        case ButtonRelease:
        case MotionNotify:
            mouseListener_->handleEvent(&event);
            break;
        case ClientMessage:
            if (static_cast<Atom>(event.xclient.data.l[0]) == wmDeleteWindow_)
            {
                close();
            }
            break;
        default:
            break;
        }
    }
}

void X11WindowManager::swapBuffers()
{
#ifdef WMA_X11_HAS_GL
    if (graphicsAPI_ == GraphicsAPI::OpenGL && display_ && window_)
        glXSwapBuffers(display_, window_);
#endif
}

void X11WindowManager::handleWindowEvent(const XEvent *event)
{
    if (event->type == ConfigureNotify)
    {
        const XConfigureEvent &xce = event->xconfigure;
        if (xce.width != windowDetails_.width || xce.height != windowDetails_.height)
        {
            windowDetails_.width = xce.width;
            windowDetails_.height = xce.height;
            windowFlags_.resized = true;
            if (graphicsAPI_ == GraphicsAPI::CPU && display_)
                allocateSoftwareImage(xce.width, xce.height);
        }
    }
    else if (event->type == PropertyNotify && event->xproperty.atom == netWmState_)
    {
        refreshMaximized();
    }
}

void *X11WindowManager::getWindowInstance()
{
    //! X11 exposes an integer XID through the shared opaque-handle API.
    // NOLINTNEXTLINE(performance-no-int-to-ptr)
    return reinterpret_cast<void *>(window_);
}

void *X11WindowManager::getNativeDisplayHandle() const noexcept
{
    return static_cast<void *>(display_);
}

void *X11WindowManager::getGLProcAddress(const char *name) const
{
#ifdef WMA_X11_HAS_GL
    if (graphicsAPI_ != GraphicsAPI::OpenGL)
        return nullptr;
    return reinterpret_cast<void *>(glXGetProcAddressARB(reinterpret_cast<const GLubyte *>(name)));
#else
    (void)name;
    return nullptr;
#endif
}

SoftwareFramebuffer X11WindowManager::lockFramebuffer()
{
    if (graphicsAPI_ != GraphicsAPI::CPU || !image_)
        return {};
    return SoftwareFramebuffer{image_->data, windowDetails_.width, windowDetails_.height, image_->bytes_per_line};
}

void X11WindowManager::presentFramebuffer()
{
    if (!image_ || !gc_ || !display_ || !window_)
        return;
    XPutImage(display_, window_, gc_, image_, 0, 0, 0, 0, static_cast<unsigned>(image_->width),
              static_cast<unsigned>(image_->height));
    XFlush(display_);
}

const std::vector<const char *> X11WindowManager::getVulkanExtensions() const
{
    return {"VK_KHR_surface", "VK_KHR_xlib_surface"};
}

WindowFlags *X11WindowManager::getWindowFlags() noexcept
{
    return &windowFlags_;
}
const WindowDetails *X11WindowManager::getWindowDetails() noexcept
{
    return &windowDetails_;
}
KeyboardListener &X11WindowManager::getKeyboardListener() noexcept
{
    return *keyboardListener_;
}

void X11WindowManager::setTextInputEnabled(bool enabled) noexcept
{
    //! Recorded only: the XIC is opened once in createWindow() rather than
    //! toggled per field. See IWindowManager::setTextInputEnabled.
    if (keyboardListener_)
        keyboardListener_->setTextInputEnabled(enabled);
}

bool X11WindowManager::isTextInputEnabled() const noexcept
{
    return keyboardListener_ && keyboardListener_->isTextInputEnabled();
}
MouseListener &X11WindowManager::getMouseListener() noexcept
{
    return *mouseListener_;
}
bool X11WindowManager::shouldClose() const
{
    return windowShouldClose_;
}
WindowBackend X11WindowManager::getBackendType() const
{
    return WindowBackend::X11;
}
GraphicsAPI X11WindowManager::getGraphicsAPI() const
{
    return graphicsAPI_;
}

WmaCode X11WindowManager::destroy()
{
    windowShouldClose_ = true;
    destroySoftwareImage();

    if (display_)
    {
#ifdef WMA_X11_HAS_GL
        if (glContext_)
        {
            glXMakeCurrent(display_, None, nullptr);
            glXDestroyContext(display_, static_cast<GLXContext>(glContext_));
            glContext_ = nullptr;
        }
#endif
        if (gc_)
        {
            XFreeGC(display_, gc_);
            gc_ = nullptr;
        }
        if (window_)
        {
            XDestroyWindow(display_, window_);
            window_ = 0;
        }
        maximized_ = false;
        if (colormap_)
        {
            XFreeColormap(display_, colormap_);
            colormap_ = 0;
        }
        XCloseDisplay(display_);
        display_ = nullptr;
    }
    return WmaCode::Ok;
}

} // namespace wma
#endif // WMA_ENABLE_X11
