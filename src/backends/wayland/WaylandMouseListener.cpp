#include "wma/backends/wayland/WaylandMouseListener.hpp"
#include "wma/core/Types.hpp"
#include "wma/exceptions/WMAException.hpp"

#include <linux/input-event-codes.h>

namespace
{

//! Cursor-theme lookup name for the default arrow pointer. "left_ptr" is the
//! legacy X-cursor name every theme still ships for compatibility; the newer
//! "default" alias is not guaranteed present on every theme, so the legacy
//! name is the safer bet for actually finding an image.
constexpr const char *kDefaultCursorName = "left_ptr";

//! Cursor image size requested from the theme, in surface pixels. 24 is the
//! conventional X11/Wayland default; asking for it explicitly keeps the
//! visible size consistent across themes rather than trusting whichever
//! default size a given theme happens to ship.
constexpr int kCursorSize = 24;

} // namespace

namespace wma
{

const wl_pointer_listener WaylandMouseListener::pointerListener_ = {.enter = handleEnterCallback,
                                                                    .leave = handleLeaveCallback,
                                                                    .motion = handleMotionCallback,
                                                                    .button = handleButtonCallback,
                                                                    .axis = handleAxisCallback,
                                                                    .frame = handleFrameCallback,
                                                                    .axis_source = handleAxisSourceCallback,
                                                                    .axis_stop = handleAxisStopCallback,
                                                                    .axis_discrete = handleAxisDiscreteCallback};

WaylandMouseListener::WaylandMouseListener() : MouseListener(), pointer_(nullptr), cursorSurface_(nullptr)
{
}

WaylandMouseListener::~WaylandMouseListener()
{
    detach();
}

void WaylandMouseListener::detach() noexcept
{
    if (cursorTheme_)
    {
        wl_cursor_theme_destroy(cursorTheme_);
        cursorTheme_ = nullptr;
    }
    if (cursorSurface_)
    {
        wl_surface_destroy(cursorSurface_);
        cursorSurface_ = nullptr;
    }
    //! The window manager owns the seat proxy.
    pointer_ = nullptr;
}

void WaylandMouseListener::initialize(wl_pointer *pointer, wl_compositor *compositor, wl_shm *shm)
{
    if (!pointer)
    {
        throw InputException("Invalid Wayland pointer");
    }
    if (pointer_ == pointer)
        return;
    detach();
    pointer_ = pointer;
    wl_pointer_add_listener(pointer_, &pointerListener_, this);

    //! Both optional, and both needed only to *restore* the system cursor;
    //! hiding it (wl_pointer_set_cursor with a null surface) needs neither.
    //! A caller that only ever hides the cursor -- or one built before this
    //! parameter pair existed, via the two-argument overload -- still works,
    //! just without a restore path (see applyCursorState()).
    if (compositor)
    {
        cursorSurface_ = wl_compositor_create_surface(compositor);
    }
    if (shm)
    {
        //! nullptr theme name asks the compositor for its configured default
        //! theme rather than pinning one by name, so this follows whatever
        //! cursor theme the user has actually set system-wide.
        cursorTheme_ = wl_cursor_theme_load(nullptr, kCursorSize, shm);
    }
}

void WaylandMouseListener::handleEnter(u32 serial, wl_surface *, wl_fixed_t x, wl_fixed_t y)
{
    f64 xpos = wl_fixed_to_double(x);
    f64 ypos = wl_fixed_to_double(y);
    currentPosition_ = WMAMousePosition(xpos, ypos);
    lastPosition_ = currentPosition_;
    firstMouse_ = true;

    /*
     * The compositor resets the pointer to its default image on every enter,
     * silently overriding whatever this listener last asked for -- including
     * a prior hide. This is what makes "cursor stays hidden while playing"
     * hold across, e.g., a window losing and regaining pointer focus, rather
     * than working only up to the first re-entry.
     */
    enterSerial_ = serial;
    applyCursorState();
}

void WaylandMouseListener::handleLeave(u32, wl_surface *)
{
}

void WaylandMouseListener::handleMotion(u32, wl_fixed_t x, wl_fixed_t y)
{
    f64 xpos = wl_fixed_to_double(x);
    f64 ypos = wl_fixed_to_double(y);

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
}

void WaylandMouseListener::handleButton(u32, u32, u32 button, u32 state)
{
    const i32 unifiedButton = convertButton(button);
    if (state == WL_POINTER_BUTTON_STATE_PRESSED)
    {
        dispatchButtonPress(unifiedButton);
    }
    else if (state == WL_POINTER_BUTTON_STATE_RELEASED)
    {
        dispatchButtonRelease(unifiedButton);
    }
}

void WaylandMouseListener::handleAxis(u32, u32 axis, wl_fixed_t value)
{
    const f64 scrollValue = wl_fixed_to_double(value);
    WMAMouseScroll scroll;

    if (axis == WL_POINTER_AXIS_VERTICAL_SCROLL)
    {
        scroll.yOffset = scrollValue > 0 ? -1.0 : 1.0;
    }
    else if (axis == WL_POINTER_AXIS_HORIZONTAL_SCROLL)
    {
        scroll.xOffset = scrollValue > 0 ? 1.0 : -1.0;
    }
    dispatchScroll(scroll);
}

void WaylandMouseListener::handleFrame()
{
}
void WaylandMouseListener::handleAxisSource(u32)
{
}
void WaylandMouseListener::handleAxisStop(u32, u32)
{
}

void WaylandMouseListener::handleAxisDiscrete(u32 axis, i32 discrete)
{
    WMAMouseScroll scroll;
    if (axis == WL_POINTER_AXIS_VERTICAL_SCROLL)
    {
        scroll.yOffset = static_cast<f64>(discrete);
    }
    else if (axis == WL_POINTER_AXIS_HORIZONTAL_SCROLL)
    {
        scroll.xOffset = static_cast<f64>(discrete);
    }
    dispatchScroll(scroll);
}

void WaylandMouseListener::updateCursorState()
{
    applyCursorState();
}

void WaylandMouseListener::applyCursorState()
{
    if (!pointer_)
        return;

    /*
     * wl_pointer.set_cursor is only valid with the serial of the most recent
     * enter (the spec calls an earlier or absent one a protocol error the
     * compositor may act on by killing the connection). Before the first
     * enter there is nothing legal to call it with; handleEnter() re-applies
     * this the moment a serial exists, so nothing is lost by skipping here.
     */
    if (enterSerial_ == 0)
        return;

    if (!cursorEnabled_)
    {
        //! The hide. A null surface is not "leave it as-is" -- it is the
        //! documented way to tell the compositor to draw nothing at the
        //! pointer position, which is the only such mechanism Wayland has.
        wl_pointer_set_cursor(pointer_, enterSerial_, nullptr, 0, 0);
        return;
    }

    //! Restoring needs an actual image to hand the compositor; without a
    //! theme and a surface to attach it to there is nothing to restore *to*,
    //! so the pointer is left exactly as the compositor's own implicit
    //! per-enter reset already put it (its default image).
    if (!cursorTheme_ || !cursorSurface_)
        return;

    wl_cursor *cursor = wl_cursor_theme_get_cursor(cursorTheme_, kDefaultCursorName);
    if (!cursor || cursor->image_count == 0)
        return;

    //! Static image: the first frame of a (possibly animated) cursor is
    //! enough for a restored system pointer, and avoids owning an animation
    //! timer purely to cycle frames on a cursor this class does not draw.
    wl_cursor_image *image = cursor->images[0];
    wl_buffer *buffer = wl_cursor_image_get_buffer(image);
    if (!buffer)
        return;

    wl_surface_attach(cursorSurface_, buffer, 0, 0);
    wl_surface_damage(cursorSurface_, 0, 0, static_cast<i32>(image->width), static_cast<i32>(image->height));
    wl_surface_commit(cursorSurface_);

    wl_pointer_set_cursor(pointer_, enterSerial_, cursorSurface_, static_cast<i32>(image->hotspot_x),
                          static_cast<i32>(image->hotspot_y));
}

i32 WaylandMouseListener::convertButton(u32 waylandButton) const
{
    switch (waylandButton)
    {
    case BTN_LEFT:
        return MouseButton::WMALeft;
    case BTN_RIGHT:
        return MouseButton::WMARight;
    case BTN_MIDDLE:
        return MouseButton::WMAMiddle;
    case BTN_SIDE:
        return MouseButton::WMAButton4;
    case BTN_EXTRA:
        return MouseButton::WMAButton5;
    default:
        return static_cast<i32>(waylandButton);
    }
}

void WaylandMouseListener::handleEnterCallback(void *data, wl_pointer *, u32 serial, wl_surface *surface, wl_fixed_t x,
                                               wl_fixed_t y)
{
    auto *l = static_cast<WaylandMouseListener *>(data);
    if (l)
        l->handleEnter(serial, surface, x, y);
}

void WaylandMouseListener::handleLeaveCallback(void *data, wl_pointer *, u32 serial, wl_surface *surface)
{
    auto *l = static_cast<WaylandMouseListener *>(data);
    if (l)
        l->handleLeave(serial, surface);
}

void WaylandMouseListener::handleMotionCallback(void *data, wl_pointer *, u32 time, wl_fixed_t x, wl_fixed_t y)
{
    auto *l = static_cast<WaylandMouseListener *>(data);
    if (l)
        l->handleMotion(time, x, y);
}

void WaylandMouseListener::handleButtonCallback(void *data, wl_pointer *, u32 serial, u32 time, u32 button, u32 state)
{
    auto *l = static_cast<WaylandMouseListener *>(data);
    if (l)
        l->handleButton(serial, time, button, state);
}

void WaylandMouseListener::handleAxisCallback(void *data, wl_pointer *, u32 time, u32 axis, wl_fixed_t value)
{
    auto *l = static_cast<WaylandMouseListener *>(data);
    if (l)
        l->handleAxis(time, axis, value);
}

void WaylandMouseListener::handleFrameCallback(void *data, wl_pointer *)
{
    auto *l = static_cast<WaylandMouseListener *>(data);
    if (l)
        l->handleFrame();
}

void WaylandMouseListener::handleAxisSourceCallback(void *data, wl_pointer *, u32 axis_source)
{
    auto *l = static_cast<WaylandMouseListener *>(data);
    if (l)
        l->handleAxisSource(axis_source);
}

void WaylandMouseListener::handleAxisStopCallback(void *data, wl_pointer *, u32 time, u32 axis)
{
    auto *l = static_cast<WaylandMouseListener *>(data);
    if (l)
        l->handleAxisStop(time, axis);
}

void WaylandMouseListener::handleAxisDiscreteCallback(void *data, wl_pointer *, u32 axis, i32 discrete)
{
    auto *l = static_cast<WaylandMouseListener *>(data);
    if (l)
        l->handleAxisDiscrete(axis, discrete);
}

} // namespace wma
