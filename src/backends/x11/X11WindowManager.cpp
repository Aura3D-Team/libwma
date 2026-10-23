#ifdef WMA_ENABLE_X11
#include "wma/backends/x11/X11WindowManager.hpp"
#include "backends/x11/X11Api.hpp"
#include "backends/x11/X11Clipboard.hpp"
#include "backends/x11/X11MoveResize.hpp"
#include "wma/exceptions/WMAException.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
#include <poll.h>
#include <span>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <utility>

#include <X11/Xatom.h>

namespace wma
{

namespace
{

//! Xlib has one process-wide error handler; this is installed only around the attach round trip.
bool shmAttachFailed = false;

int onShmAttachError(Display *, XErrorEvent *)
{
    shmAttachFailed = true;
    return 0;
}

} // namespace

X11WindowManager::X11WindowManager(const WindowDetails &windowDetails, GraphicsAPI graphicsAPI)
    : display_(nullptr), window_(0), colormap_(0), wmDeleteWindow_(0), gc_(nullptr), glContext_(nullptr),
      fbConfig_(nullptr), windowDetails_(windowDetails), windowFlags_{}, graphicsAPI_(graphicsAPI),
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
    : display_(std::exchange(other.display_, nullptr)), lastX_(other.lastX_), lastY_(other.lastY_),
      randrEventBase_(std::exchange(other.randrEventBase_, -1)), window_(std::exchange(other.window_, 0)),
      colormap_(std::exchange(other.colormap_, 0)), wmDeleteWindow_(other.wmDeleteWindow_),
      netWmState_(other.netWmState_), netWmStateMaximizedVert_(other.netWmStateMaximizedVert_),
      netWmStateMaximizedHorz_(other.netWmStateMaximizedHorz_), netWmName_(other.netWmName_),
      utf8String_(other.utf8String_), maximized_(std::exchange(other.maximized_, false)),
      gc_(std::exchange(other.gc_, nullptr)), slots_(std::exchange(other.slots_, {})),
      shmCompletion_(other.shmCompletion_), shmRejected_(other.shmRejected_),
      presentOpcode_(std::exchange(other.presentOpcode_, -1)), presentEventId_(std::exchange(other.presentEventId_, 0)),
      presentSerial_(other.presentSerial_), presentPending_(other.presentPending_),
      glContext_(std::exchange(other.glContext_, nullptr)), fbConfig_(other.fbConfig_),
      windowDetails_(other.windowDetails_), windowFlags_(other.windowFlags_), graphicsAPI_(other.graphicsAPI_),
      keyboardListener_(std::move(other.keyboardListener_)), mouseListener_(std::move(other.mouseListener_)),
      clipboard_(std::move(other.clipboard_)), windowShouldClose_(other.windowShouldClose_),
      hitTest_(std::move(other.hitTest_)), caption_(other.caption_), captionButton_(other.captionButton_)
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
        lastX_ = other.lastX_;
        lastY_ = other.lastY_;
        randrEventBase_ = std::exchange(other.randrEventBase_, -1);
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
        slots_ = std::exchange(other.slots_, {});
        shmCompletion_ = other.shmCompletion_;
        shmRejected_ = other.shmRejected_;
        presentOpcode_ = std::exchange(other.presentOpcode_, -1);
        presentEventId_ = std::exchange(other.presentEventId_, 0);
        presentSerial_ = other.presentSerial_;
        presentPending_ = other.presentPending_;
        glContext_ = std::exchange(other.glContext_, nullptr);
        fbConfig_ = other.fbConfig_;
        windowDetails_ = other.windowDetails_;
        windowFlags_ = other.windowFlags_;
        graphicsAPI_ = other.graphicsAPI_;
        keyboardListener_ = std::move(other.keyboardListener_);
        mouseListener_ = std::move(other.mouseListener_);
        clipboard_ = std::move(other.clipboard_);
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

    if (!x11::load())
        throw WindowException("libX11.so.6 is not installed; the X11 backend cannot run");
#ifdef WMA_X11_HAS_GL
    if (graphicsAPI_ == GraphicsAPI::OpenGL && !x11::hasGlx())
        throw GraphicsException("OpenGL on X11 needs libGLX.so.0 or libGL.so.1, and neither is installed");
#endif

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
    //! A compositor discards a resized window's contents under the default ForgetGravity, which
    //! shows as a black flash until the next frame lands; keep them until it does.
    windowAttributes.bit_gravity = NorthWestGravity;
    unsigned long valueMask = CWEventMask | CWBitGravity;
    if (colormap_)
    {
        windowAttributes.colormap = colormap_;
        windowAttributes.border_pixel = 0;
        valueMask |= CWColormap | CWBorderPixel;
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
    clipboard_ = std::make_unique<X11Clipboard>(display_, window_);

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

    int randrError = 0;
    int randrBase = 0;
    if (x11::hasRandr() && XRRQueryExtension(display_, &randrBase, &randrError))
    {
        randrEventBase_ = randrBase;
        XRRSelectInput(display_, window_, RRScreenChangeNotifyMask);
    }

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
        if (x11::hasShm() && XShmQueryExtension(display_))
            shmCompletion_ = XShmGetEventBase(display_) + ShmCompletion;
        if (windowDetails_.vsync)
            setupPresent();
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
    std::unique_ptr<unsigned char, int (*)(void *)> owner(property, XFree);
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

void X11WindowManager::createSlot(Slot &slot, i32 width, i32 height)
{
    if (shmCompletion_ >= 0 && !shmRejected_ && createShmImage(slot, width, height))
    {
        //! A segment the server cannot map as a pixmap ends Present for the window; the loop is rate-capped instead.
        if (presentOpcode_ >= 0 && !createPresentPixmap(slot, width, height))
            presentOpcode_ = -1;
        return;
    }
    presentOpcode_ = -1;

    const int screen = DefaultScreen(display_);

    //! 32-bit ZPixmap buffer owned by the XImage (XDestroyImage frees data).
    std::unique_ptr<char, decltype(&std::free)> buffer(
        static_cast<char *>(std::calloc(static_cast<usize>(width) * height, 4)), &std::free);
    if (!buffer)
        throw WMAException("Out of memory allocating software framebuffer");

    slot.image = XCreateImage(display_, DefaultVisual(display_, screen), DefaultDepth(display_, screen), ZPixmap, 0,
                              buffer.get(), width, height, 32, 0);
    if (!slot.image)
        throw WMAException("Failed to create X11 software image");
    slot.image->data = buffer.release();
}

bool X11WindowManager::createShmImage(Slot &slot, i32 width, i32 height)
{
    const int screen = DefaultScreen(display_);
    XImage *image = XShmCreateImage(display_, DefaultVisual(display_, screen), DefaultDepth(display_, screen), ZPixmap,
                                    nullptr, &slot.shm, width, height);
    if (!image)
        return false;

    const int id = shmget(IPC_PRIVATE, static_cast<usize>(image->bytes_per_line) * height, IPC_CREAT | 0600);
    void *address = id < 0 ? nullptr : shmat(id, nullptr, 0);
    //! shmat() reports failure as (void *)-1.
    // NOLINTNEXTLINE(performance-no-int-to-ptr)
    if (address == reinterpret_cast<void *>(-1))
        address = nullptr;
    if (!address)
    {
        XDestroyImage(image);
        return false;
    }
    slot.shm.shmid = id;
    slot.shm.shmaddr = image->data = static_cast<char *>(address);
    slot.shm.readOnly = False;

    //! The attach is only validated by the round trip: a server on another host or IPC namespace
    //! cannot reach the segment and answers BadAccess.
    shmAttachFailed = false;
    const XErrorHandler previous = XSetErrorHandler(onShmAttachError);
    XShmAttach(display_, &slot.shm);
    XSync(display_, False);
    XSetErrorHandler(previous);
    //! Marked for removal only now, so a crash leaks nothing but the server could still attach.
    shmctl(id, IPC_RMID, nullptr);

    if (shmAttachFailed)
    {
        shmRejected_ = true;
        image->data = nullptr;
        XDestroyImage(image);
        shmdt(address);
        slot.shm = {};
        return false;
    }
    slot.image = image;
    return true;
}

void X11WindowManager::setupPresent()
{
    int opcode = 0;
    int eventBase = 0;
    int errorBase = 0;
    int major = 0;
    int minor = 0;
    int shmMajor = 0;
    int shmMinor = 0;
    Bool sharedPixmaps = False;
    //! Pixmaps over the segment are what Present flips; without them the frame would need a copy per present.
    if (shmCompletion_ < 0 || !x11::hasPresent() || !XShmQueryVersion(display_, &shmMajor, &shmMinor, &sharedPixmaps) ||
        !sharedPixmaps || !XPresentQueryExtension(display_, &opcode, &eventBase, &errorBase) ||
        !XPresentQueryVersion(display_, &major, &minor) || major < 1)
        return;
    presentEventId_ = XPresentSelectInput(display_, window_, PresentCompleteNotifyMask | PresentIdleNotifyMask);
    presentOpcode_ = opcode;
}

bool X11WindowManager::createPresentPixmap(Slot &slot, i32 width, i32 height)
{
    const int screen = DefaultScreen(display_);
    //! Like the attach: the server answers a pixmap it cannot back only asynchronously.
    shmAttachFailed = false;
    const XErrorHandler previous = XSetErrorHandler(onShmAttachError);
    const Pixmap pixmap =
        XShmCreatePixmap(display_, window_, slot.shm.shmaddr, &slot.shm, static_cast<unsigned>(width),
                         static_cast<unsigned>(height), static_cast<unsigned>(DefaultDepth(display_, screen)));
    XSync(display_, False);
    XSetErrorHandler(previous);
    if (shmAttachFailed || pixmap == 0)
    {
        if (pixmap != 0 && !shmAttachFailed)
            XFreePixmap(display_, pixmap);
        return false;
    }
    slot.pixmap = pixmap;
    return true;
}

void X11WindowManager::destroySlot(Slot &slot) noexcept
{
    if (slot.pixmap)
        XFreePixmap(display_, slot.pixmap);
    if (slot.shm.shmaddr)
    {
        XShmDetach(display_, &slot.shm);
        slot.image->data = nullptr; // owned by the segment, not malloc
        shmdt(slot.shm.shmaddr);
    }
    if (slot.image)
        XDestroyImage(slot.image); // also frees data of a non-SHM slot
    slot = {};
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

        if (event.type == GenericEvent && presentOpcode_ >= 0 && event.xcookie.extension == presentOpcode_)
        {
            XGenericEventCookie cookie = event.xcookie;
            if (XGetEventData(display_, &cookie))
            {
                handlePresentEvent(cookie);
                XFreeEventData(display_, &cookie);
            }
            continue;
        }

        if (event.type == shmCompletion_)
        {
            const ShmSeg done = reinterpret_cast<const XShmCompletionEvent &>(event).shmseg;
            for (Slot &slot : slots_)
                slot.busy = slot.busy && slot.shm.shmseg != done;
            continue;
        }

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
            mouseListener_->setFocused(true);
            break;
        case FocusOut:
            windowFlags_.focused = false;
            mouseListener_->setFocused(false);
            //! See FocusChangeMask in createWindow().
            keyboardListener_->releaseAllKeys();
            break;
        case ButtonPress:
        case ButtonRelease:
        case MotionNotify:
        case GenericEvent:
            mouseListener_->handleEvent(&event);
            break;
        case ClientMessage:
            if (static_cast<Atom>(event.xclient.data.l[0]) == wmDeleteWindow_)
            {
                close();
            }
            break;
        case SelectionRequest:
            if (clipboard_)
                clipboard_->handleRequest(event.xselectionrequest);
            break;
        case SelectionClear:
            if (clipboard_)
                clipboard_->handleClear(event.xselectionclear);
            break;
        default:
            break;
        }
    }
}

void X11WindowManager::handlePresentEvent(XGenericEventCookie &cookie)
{
    if (cookie.evtype == PresentCompleteNotify)
    {
        const auto &done = *static_cast<const XPresentCompleteNotifyEvent *>(cookie.data);
        if (done.serial_number == presentPending_)
            presentPending_ = 0;
    }
    else if (cookie.evtype == PresentIdleNotify)
    {
        const auto &idle = *static_cast<const XPresentIdleNotifyEvent *>(cookie.data);
        for (Slot &slot : slots_)
            slot.busy = slot.busy && slot.pixmap != idle.pixmap;
    }
}

void X11WindowManager::waitForPresent()
{
    //! A window the server never completes for (a stalled output) still ticks, slowly, instead of freezing.
    constexpr auto kLimit = std::chrono::milliseconds(100);
    //! Nothing was presented, so nothing to wait for; one refresh of blocking keeps an idle loop from spinning
    //! and still wakes on input.
    constexpr int kIdleWaitMs = 16;

    if (presentPending_ == 0)
    {
        waitEvents(kIdleWaitMs);
        return;
    }

    const auto deadline = std::chrono::steady_clock::now() + kLimit;
    while (presentPending_ != 0 && !windowShouldClose_)
    {
        const auto remaining =
            std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now());
        if (remaining.count() <= 0)
        {
            presentPending_ = 0;
            break;
        }
        waitEvents(static_cast<int>(remaining.count()));
    }
}

bool X11WindowManager::presentWaitsForDisplay() const noexcept
{
    return graphicsAPI_ == GraphicsAPI::CPU && presentOpcode_ >= 0;
}

void X11WindowManager::swapBuffers()
{
    if (graphicsAPI_ == GraphicsAPI::CPU && presentOpcode_ >= 0)
        waitForPresent();
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
        //! A move is the only way the window changes monitor; a resize drag must not re-query RandR per frame.
        if (xce.x != lastX_ || xce.y != lastY_)
        {
            lastX_ = xce.x;
            lastY_ = xce.y;
            markDisplayChanged();
        }
        if (xce.width != windowDetails_.width || xce.height != windowDetails_.height)
        {
            windowDetails_.width = xce.width;
            windowDetails_.height = xce.height;
            windowFlags_.resized = true;
        }
    }
    else if (randrEventBase_ >= 0 && event->type == randrEventBase_ + RRScreenChangeNotify)
    {
        XRRUpdateConfiguration(const_cast<XEvent *>(event));
        markDisplayChanged();
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
    if (graphicsAPI_ != GraphicsAPI::CPU || !display_)
        return {};

    //! Both with the server: refuse the frame. That is the pacing, as with the Wayland frame callback;
    //! IRenderer::needsFrame() already covers the drop, and the completion event wakes waitEvents().
    const auto idle = std::ranges::find_if(slots_,
                                           [](const Slot &slot)
                                           {
                                               return !slot.busy;
                                           });
    if (idle == slots_.end())
        return {};

    //! Sized on demand rather than per ConfigureNotify: a drag delivers several of those per frame.
    const i32 width = windowDetails_.width;
    const i32 height = windowDetails_.height;
    if (!idle->image || idle->image->width != width || idle->image->height != height)
    {
        destroySlot(*idle);
        if (width <= 0 || height <= 0)
            return {};
        createSlot(*idle, width, height);
    }
    locked_ = &*idle;
    return SoftwareFramebuffer{idle->image->data, width, height, idle->image->bytes_per_line};
}

void X11WindowManager::presentFramebuffer()
{
    if (!locked_ || !gc_ || !display_ || !window_)
        return;
    Slot &slot = *std::exchange(locked_, nullptr);
    const auto width = static_cast<unsigned>(slot.image->width);
    const auto height = static_cast<unsigned>(slot.image->height);
    if (slot.pixmap && presentOpcode_ >= 0)
    {
        //! Serial 0 means "none pending"; skip it on wrap.
        presentSerial_ = presentSerial_ + 1 == 0 ? 1 : presentSerial_ + 1;
        slot.busy = true;
        //! No target MSC and no divisor: the server copies at the next vblank, which is the wait.
        XPresentPixmap(display_, window_, slot.pixmap, presentSerial_, None, None, 0, 0, None, None, None,
                       PresentOptionNone, 0, 0, 0, nullptr, 0);
        presentPending_ = presentSerial_;
    }
    else if (slot.shm.shmaddr)
    {
        //! The slots move with the manager, which leaves the image's own pointer stale.
        slot.image->obdata = reinterpret_cast<XPointer>(&slot.shm);
        XShmPutImage(display_, window_, gc_, slot.image, 0, 0, 0, 0, width, height, True);
        slot.busy = true;
    }
    else
    {
        XPutImage(display_, window_, gc_, slot.image, 0, 0, 0, 0, width, height);
    }
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

u32 X11WindowManager::displayRefreshHz() const noexcept
{
    if (!display_ || !x11::hasRandr())
        return 0;

    const Window root = DefaultRootWindow(display_);
    XRRScreenResources *resources = XRRGetScreenResourcesCurrent(display_, root);
    if (!resources)
        return 0;

    //! The monitor under the window's centre; failing that, the first one that is lit.
    int centerX = 0;
    int centerY = 0;
    XWindowAttributes attributes{};
    Window ignored = 0;
    const bool placed = XGetWindowAttributes(display_, window_, &attributes) != 0 &&
                        XTranslateCoordinates(display_, window_, root, attributes.width / 2, attributes.height / 2,
                                              &centerX, &centerY, &ignored) != 0;

    double hz = 0.0;
    for (int index = 0; index < resources->ncrtc; ++index)
    {
        XRRCrtcInfo *crtc = XRRGetCrtcInfo(display_, resources, resources->crtcs[index]);
        if (!crtc)
            continue;

        double rate = 0.0;
        for (int mode = 0; crtc->mode != None && mode < resources->nmode; ++mode)
        {
            const XRRModeInfo &info = resources->modes[mode];
            if (info.id != crtc->mode || info.hTotal == 0 || info.vTotal == 0)
                continue;
            //! dotClock / (pixels per frame); an interlaced mode scans half a frame per pass, a doublescan twice.
            double lines = info.vTotal;
            if (info.modeFlags & RR_DoubleScan)
                lines *= 2.0;
            if (info.modeFlags & RR_Interlace)
                lines *= 0.5;
            rate = static_cast<double>(info.dotClock) / (static_cast<double>(info.hTotal) * lines);
        }

        const bool under = placed && centerX >= crtc->x && centerY >= crtc->y &&
                           centerX < crtc->x + static_cast<int>(crtc->width) &&
                           centerY < crtc->y + static_cast<int>(crtc->height);
        XRRFreeCrtcInfo(crtc);

        if (rate <= 0.0)
            continue;
        if (hz <= 0.0 || under)
            hz = rate;
        if (under)
            break;
    }
    XRRFreeScreenResources(resources);
    return hz > 0.0 ? static_cast<u32>(std::lround(hz)) : 0u;
}

bool X11WindowManager::isClipboardSupported() const noexcept
{
    return clipboard_ != nullptr;
}

std::string X11WindowManager::getClipboardText()
{
    return clipboard_ ? clipboard_->text() : std::string{};
}

bool X11WindowManager::setClipboardText(std::string_view text)
{
    return clipboard_ && clipboard_->setText(text);
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
    locked_ = nullptr;

    if (display_)
    {
        //! While the connection is open: giving up the selection is a request on it.
        clipboard_.reset();
        for (Slot &slot : slots_)
            destroySlot(slot);
#ifdef WMA_X11_HAS_GL
        if (glContext_)
        {
            glXMakeCurrent(display_, None, nullptr);
            glXDestroyContext(display_, static_cast<GLXContext>(glContext_));
            glContext_ = nullptr;
        }
#endif
        if (presentEventId_)
        {
            XPresentFreeInput(display_, window_, presentEventId_);
            presentEventId_ = 0;
        }
        presentOpcode_ = -1;
        presentPending_ = 0;
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
