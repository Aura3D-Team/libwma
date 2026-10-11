#include "wma/backends/x11/X11MouseListener.hpp"
#include "wma/backends/x11/X11WindowManager.hpp"
#include "wma/core/Types.hpp"
#include "wma/exceptions/WMAException.hpp"

#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <X11/extensions/XInput2.h>
#include <utility>

namespace wma
{
namespace
{

//! The core cursor font has no diagonal double arrows; corners read as diagonals.
[[nodiscard]] unsigned int toFontShape(SystemCursor shape) noexcept
{
    switch (shape)
    {
    case SystemCursor::NsResize:
        return XC_sb_v_double_arrow;
    case SystemCursor::EwResize:
        return XC_sb_h_double_arrow;
    case SystemCursor::NwseResize:
        return XC_bottom_right_corner;
    case SystemCursor::NeswResize:
        return XC_bottom_left_corner;
    case SystemCursor::Default:
        break;
    }
    return XC_left_ptr;
}

} // namespace

X11MouseListener::X11MouseListener() : MouseListener(), display_(nullptr)
{
}

void X11MouseListener::initialize(Display *display, Window window)
{
    if (!display)
    {
        throw InputException("Invalid X11 Display pointer");
    }
    display_ = display;
    x11Window_ = window;
    invisibleCursor_ = createInvisibleCursor(display, window);

    int firstEvent = 0;
    int firstError = 0;
    int major = 2;
    int minor = 0;
    if (XQueryExtension(display, "XInputExtension", &xiOpcode_, &firstEvent, &firstError) &&
        XIQueryVersion(display, &major, &minor) != Success)
        xiOpcode_ = 0;

    Window root, child;
    int rootX, rootY, x, y;
    unsigned int mask;
    XQueryPointer(display, DefaultRootWindow(display), &root, &child, &rootX, &rootY, &x, &y, &mask);
    currentPosition_ = WMAMousePosition(static_cast<f64>(x), static_cast<f64>(y));
    lastPosition_ = currentPosition_;
    firstMouse_ = true;
}

void X11MouseListener::handleEvent(const XEvent *event)
{
    if (!event)
        return;

    switch (event->type)
    {
    case ButtonPress:
    {
        const int btn = static_cast<int>(event->xbutton.button);
        if (btn == Button4 || btn == Button5)
        {
            const f64 scrollY = (btn == Button4) ? 1.0 : -1.0;
            dispatchScroll(WMAMouseScroll(0.0, scrollY));
            break;
        }
        if (btn == Button1)
        {
            //! Assigned, not just set: a claim whose release the window manager
            //! took must not swallow the release of the next, unclaimed press.
            pressClaimed_ = owner_ && owner_->claimPress(event->xbutton);
            if (pressClaimed_)
                break;
        }
        dispatchButtonPress(convertButton(btn));
        break;
    }
    case ButtonRelease:
    {
        const int btn = static_cast<int>(event->xbutton.button);
        if (btn >= Button4)
            break;
        if (btn == Button1 && std::exchange(pressClaimed_, false))
        {
            if (owner_)
                owner_->releaseClaim();
            break;
        }
        dispatchButtonRelease(convertButton(btn));
        break;
    }
    case GenericEvent:
        if (captured_ && event->xcookie.extension == xiOpcode_)
        {
            XGenericEventCookie cookie = event->xcookie;
            if (XGetEventData(display_, &cookie))
            {
                if (cookie.evtype == XI_RawMotion)
                    handleRawMotion(cookie);
                XFreeEventData(display_, &cookie);
            }
        }
        break;
    case MotionNotify:
    {
        //! The confined pointer stops at the window edge, so its position says nothing about
        //! how far the user moved; raw motion carries that.
        if (captured_)
            break;
        f64 xpos = static_cast<f64>(event->xmotion.x);
        f64 ypos = static_cast<f64>(event->xmotion.y);
        if (firstMouse_)
        {
            lastPosition_ = WMAMousePosition(xpos, ypos);
            firstMouse_ = false;
        }
        f64 deltaX = (xpos - lastPosition_.x) * sensitivity_;
        f64 deltaY = (lastPosition_.y - ypos) * sensitivity_;
        currentPosition_ = WMAMousePosition(xpos, ypos, deltaX, deltaY);
        dispatchMove(currentPosition_);
        lastPosition_ = WMAMousePosition(xpos, ypos);
        if (pressClaimed_ && owner_)
            owner_->dragTo(event->xmotion);
        setHitCursor(owner_ ? owner_->hoverCursor(xpos, ypos) : SystemCursor::Default);
        break;
    }
    default:
        break;
    }
}

Cursor X11MouseListener::createInvisibleCursor(Display *display, Window window)
{
    Pixmap bmNo;
    XColor black;
    static char noData[] = {0, 0, 0, 0, 0, 0, 0, 0};
    black.red = black.green = black.blue = 0;
    bmNo = XCreateBitmapFromData(display, window, noData, 8, 8);
    return XCreatePixmapCursor(display, bmNo, bmNo, &black, &black, 0, 0);
}

void X11MouseListener::updateCursorState()
{
    if (!display_ || !x11Window_)
        return;
    const SystemCursor shape = effectiveSystemCursor();
    if (cursorEnabled_ && shape == SystemCursor::Default)
    {
        XUndefineCursor(display_, x11Window_);
    }
    else if (cursorEnabled_)
    {
        Cursor &cursor = shapeCursors_[static_cast<usize>(shape)];
        if (!cursor)
            cursor = XCreateFontCursor(display_, toFontShape(shape));
        XDefineCursor(display_, x11Window_, cursor);
    }
    else
    {
        XDefineCursor(display_, x11Window_, invisibleCursor_);
    }
    updateCapture();
    XFlush(display_);
}

void X11MouseListener::setFocused(bool focused)
{
    focused_ = focused;
    updateCapture();
    XFlush(display_);
}

void X11MouseListener::updateCapture()
{
    const bool wanted = !cursorEnabled_ && focused_;
    if (wanted == captured_ || !display_ || !x11Window_)
        return;

    if (wanted)
    {
        constexpr unsigned int events = ButtonPressMask | ButtonReleaseMask | PointerMotionMask;
        if (XGrabPointer(display_, x11Window_, True, events, GrabModeAsync, GrabModeAsync, x11Window_, invisibleCursor_,
                         CurrentTime) != GrabSuccess)
            return;
    }
    else
    {
        XUngrabPointer(display_, CurrentTime);
    }
    selectRawMotion(wanted);
    captured_ = wanted;
    firstMouse_ = true;
}

void X11MouseListener::selectRawMotion(bool enabled) const
{
    if (!xiOpcode_)
        return;
    std::array<unsigned char, XIMaskLen(XI_LASTEVENT)> bits{};
    if (enabled)
        XISetMask(bits.data(), XI_RawMotion);
    XIEventMask mask{XIAllMasterDevices, static_cast<int>(bits.size()), bits.data()};
    XISelectEvents(display_, DefaultRootWindow(display_), &mask, 1);
}

void X11MouseListener::handleRawMotion(XGenericEventCookie &cookie)
{
    //! The accelerated deltas, in pixels, so the feel matches what absolute motion gave before.
    const auto &raw = *static_cast<const XIRawEvent *>(cookie.data);
    const double *value = raw.valuators.values;
    f64 delta[2]{};
    for (int axis = 0; axis < raw.valuators.mask_len * 8 && axis < 2; ++axis)
    {
        if (XIMaskIsSet(raw.valuators.mask, axis))
            delta[axis] = *value++;
    }
    dispatchMove(WMAMousePosition(currentPosition_.x, currentPosition_.y, delta[0] * sensitivity_,
                                  -delta[1] * sensitivity_));
}

i32 X11MouseListener::convertButton(i32 x11Button) const
{
    switch (x11Button)
    {
    case Button1:
        return MouseButton::WMALeft;
    case Button2:
        return MouseButton::WMAMiddle;
    case Button3:
        return MouseButton::WMARight;
    default:
        return x11Button;
    }
}

} // namespace wma
