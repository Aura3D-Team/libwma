#include "backends/x11/X11MoveResize.hpp"

#include <X11/Xlib.h>

namespace wma::x11
{
namespace
{

//! _NET_WM_MOVERESIZE directions from the EWMH specification.
[[nodiscard]] constexpr long direction(WindowHit hit) noexcept
{
    switch (hit)
    {
    case WindowHit::TopLeft:
        return 0;
    case WindowHit::Top:
        return 1;
    case WindowHit::TopRight:
        return 2;
    case WindowHit::Right:
        return 3;
    case WindowHit::BottomRight:
        return 4;
    case WindowHit::Bottom:
        return 5;
    case WindowHit::BottomLeft:
        return 6;
    case WindowHit::Left:
        return 7;
    case WindowHit::Caption:
        return 8;
    case WindowHit::Client:
        break;
    }
    return -1;
}

} // namespace

bool startMoveResize(Display *display, unsigned long window, WindowHit hit, int rootX, int rootY,
                     unsigned int button) noexcept
{
    const long action = direction(hit);
    if (!display || !window || action < 0)
        return false;

    //! The press's implicit grab would otherwise keep the pointer from the window manager.
    XUngrabPointer(display, CurrentTime);

    XEvent event{};
    event.xclient.type = ClientMessage;
    event.xclient.window = window;
    event.xclient.message_type = XInternAtom(display, "_NET_WM_MOVERESIZE", False);
    event.xclient.format = 32;
    event.xclient.data.l[0] = rootX;
    event.xclient.data.l[1] = rootY;
    event.xclient.data.l[2] = action;
    event.xclient.data.l[3] = static_cast<long>(button);
    event.xclient.data.l[4] = 1; // Application source, not a pager.
    const bool sent = XSendEvent(display, DefaultRootWindow(display), False,
                                 SubstructureRedirectMask | SubstructureNotifyMask, &event) != 0;
    XFlush(display);
    return sent;
}

bool startMoveResize(Display *display, unsigned long window, WindowHit hit) noexcept
{
    if (!display || !window)
        return false;

    ::Window root = 0;
    ::Window child = 0;
    int rootX = 0;
    int rootY = 0;
    int windowX = 0;
    int windowY = 0;
    unsigned int mask = 0;
    if (!XQueryPointer(display, window, &root, &child, &rootX, &rootY, &windowX, &windowY, &mask))
        return false;

    unsigned int button = 0;
    if (mask & Button1Mask)
        button = Button1;
    else if (mask & Button2Mask)
        button = Button2;
    else if (mask & Button3Mask)
        button = Button3;
    else
        return false;

    return startMoveResize(display, window, hit, rootX, rootY, button);
}

} // namespace wma::x11
