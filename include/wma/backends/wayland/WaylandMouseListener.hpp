#ifndef WMA_BACKENDS_WAYLAND_MOUSE_LISTENER_HPP
#define WMA_BACKENDS_WAYLAND_MOUSE_LISTENER_HPP

#include "wma/input/mouse/MouseListener.hpp"
#include <wayland-client.h>
#include <wayland-cursor.h>

namespace wma
{

/**
 * @class WaylandMouseListener
 * @brief Wayland pointer input, including cursor visibility.
 *
 * Cursor handling on Wayland differs from X11/SDL in two ways a port from
 * either of those tends to miss, and that this class exists to absorb:
 *
 *  1. There is no "hide the cursor" request. A client sets the pointer's
 *     appearance with wl_pointer.set_cursor, and a null surface there means
 *     "draw nothing" -- that null *is* the hide, not a separate call.
 *
 *  2. wl_pointer.set_cursor is only valid with the serial of the *most
 *     recent* wl_pointer.enter, and the compositor silently resets the
 *     pointer back to its default image on every enter -- including the
 *     first one, and every one after a leave (switching windows, alt-tab).
 *     So "cursor hidden" is not state that can be set once at startup; it has
 *     to be re-asserted on every enter for as long as it should hold.
 */
class WaylandMouseListener : public MouseListener
{
  public:
    WaylandMouseListener();
    ~WaylandMouseListener() override;

    /**
     * @brief Binds this listener to @p pointer.
     *
     * @param pointer    Seat pointer to listen on.
     * @param compositor Used to create the surface a restored cursor image is
     *                   attached to. Optional: without it the cursor can
     *                   still be hidden, just not restored to the system
     *                   theme afterwards (setCursorEnabled(true) becomes a
     *                   no-op rather than failing outright).
     * @param shm        Used to load the system cursor theme. Same caveat as
     *                   @p compositor.
     */
    void initialize(wl_pointer *pointer, wl_compositor *compositor = nullptr, wl_shm *shm = nullptr);

    void detach() noexcept;

    wl_pointer *getPointer() const
    {
        return pointer_;
    }

    void handleEnter(u32 serial, wl_surface *surface, wl_fixed_t x, wl_fixed_t y);
    void handleLeave(u32 serial, wl_surface *surface);
    void handleMotion(u32 time, wl_fixed_t x, wl_fixed_t y);
    void handleButton(u32 serial, u32 time, u32 button, u32 state);
    void handleAxis(u32 time, u32 axis, wl_fixed_t value);
    void handleFrame();
    void handleAxisSource(u32 axis_source);
    void handleAxisStop(u32 time, u32 axis);
    void handleAxisDiscrete(u32 axis, i32 discrete);

  protected:
    void updateCursorState() override;

  private:
    wl_pointer *pointer_ = nullptr;
    wl_surface *cursorSurface_ = nullptr;

    //! Non-owning; the theme owns the images it hands out. Null when @c shm_
    //! or the theme load failed, in which case the cursor can be hidden but
    //! never restored (see initialize()'s doc comment).
    wl_cursor_theme *cursorTheme_ = nullptr;

    //! Serial of the most recent wl_pointer.enter -- the one wl_pointer.
    //! set_cursor must be called with. 0 (never a valid serial) until the
    //! first enter arrives.
    u32 enterSerial_ = 0;

    /**
     * @brief Re-applies the current cursorEnabled_ state to the compositor.
     *
     * The one place that actually calls wl_pointer_set_cursor, used both by
     * updateCursorState() (an explicit setCursorEnabled() call) and
     * handleEnter() (the compositor's implicit per-enter reset). A no-op
     * before the first enter: without a valid serial there is nothing legal
     * to call set_cursor with yet, and handleEnter() re-applies it the moment
     * one exists.
     */
    void applyCursorState();

    static const wl_pointer_listener pointerListener_;

    static void handleEnterCallback(void *data, wl_pointer *pointer, u32 serial, wl_surface *surface, wl_fixed_t x,
                                    wl_fixed_t y);
    static void handleLeaveCallback(void *data, wl_pointer *pointer, u32 serial, wl_surface *surface);
    static void handleMotionCallback(void *data, wl_pointer *pointer, u32 time, wl_fixed_t x, wl_fixed_t y);
    static void handleButtonCallback(void *data, wl_pointer *pointer, u32 serial, u32 time, u32 button, u32 state);
    static void handleAxisCallback(void *data, wl_pointer *pointer, u32 time, u32 axis, wl_fixed_t value);
    static void handleFrameCallback(void *data, wl_pointer *pointer);
    static void handleAxisSourceCallback(void *data, wl_pointer *pointer, u32 axis_source);
    static void handleAxisStopCallback(void *data, wl_pointer *pointer, u32 time, u32 axis);
    static void handleAxisDiscreteCallback(void *data, wl_pointer *pointer, u32 axis, i32 discrete);

    i32 convertButton(u32 waylandButton) const;
};

} // namespace wma

#endif // WMA_BACKENDS_WAYLAND_MOUSE_LISTENER_HPP
