#ifdef WMA_ENABLE_SDL
#include "backends/sdl/SdlFrameHitTest.hpp"

#include <algorithm>
#include <cstring>
#include <utility>
#include <vector>

#ifdef WMA_ENABLE_WAYLAND
#include <linux/input-event-codes.h>
#include <wayland-client.h>

#include "backends/wayland/WaylandDecorationProbe.hpp"
#include "wma/backends/wayland/protocols/xdg-shell-client-protocol.h"
#endif

namespace wma::sdl
{

#ifdef WMA_ENABLE_WAYLAND
/// A wl_pointer of our own on every seat of SDL's connection, for the press serial
/// a compositor move needs and SDL does not expose. A private queue keeps these
/// listeners out of SDL's dispatch; libdecor's plugins use the same technique.
struct FrameHitTest::WaylandSeats
{
    struct Seat
    {
        Seat() = default;
        Seat(const Seat &) = delete;
        Seat &operator=(const Seat &) = delete;
        ~Seat()
        {
            if (pointer)
                wl_pointer_destroy(pointer);
            if (seat)
                wl_seat_destroy(seat);
        }

        WaylandSeats *owner = nullptr;
        wl_seat *seat = nullptr;
        u32 name = 0;
        wl_pointer *pointer = nullptr;
        wl_surface *focus = nullptr;
        u32 pressSerial = 0;
        bool pressed = false;
    };

    WaylandSeats(wl_display *display, wl_surface *surface) : display(display), surface(surface)
    {
        queue = wl_display_create_queue(display);
        auto *wrapped = static_cast<wl_display *>(wl_proxy_create_wrapper(display));
        wl_proxy_set_queue(reinterpret_cast<wl_proxy *>(wrapped), queue);
        registry = wl_display_get_registry(wrapped);
        wl_proxy_wrapper_destroy(wrapped);
        wl_registry_add_listener(registry, &kRegistryListener, this);
        //! The first roundtrip binds the seats, the second receives their capabilities.
        wl_display_roundtrip_queue(display, queue);
        wl_display_roundtrip_queue(display, queue);
    }

    ~WaylandSeats()
    {
        seats.clear();
        if (registry)
            wl_registry_destroy(registry);
        if (queue)
            wl_event_queue_destroy(queue);
    }

    WaylandSeats(const WaylandSeats &) = delete;
    WaylandSeats &operator=(const WaylandSeats &) = delete;

    //! SDL has already read these from the socket; this only dispatches them.
    void pump()
    {
        wl_display_dispatch_queue_pending(display, queue);
    }

    //! Moves @p toplevel with the seat whose primary button is held on the window.
    bool startMove(xdg_toplevel *toplevel)
    {
        pump();
        if (!toplevel || !latest || !latest->pressed || latest->focus != surface)
            return false;
        xdg_toplevel_move(toplevel, latest->seat, latest->pressSerial);
        wl_display_flush(display);
        return true;
    }

    static void global(void *data, wl_registry *registry, u32 name, const char *interface, u32)
    {
        if (std::strcmp(interface, wl_seat_interface.name) != 0)
            return;
        auto &self = *static_cast<WaylandSeats *>(data);
        auto seat = std::make_unique<Seat>();
        seat->owner = &self;
        seat->name = name;
        seat->seat = static_cast<wl_seat *>(wl_registry_bind(registry, name, &wl_seat_interface, 1));
        wl_seat_add_listener(seat->seat, &kSeatListener, seat.get());
        self.seats.push_back(std::move(seat));
    }

    static void globalRemove(void *data, wl_registry *, u32 name)
    {
        auto &self = *static_cast<WaylandSeats *>(data);
        const auto removed = std::ranges::find(self.seats, name, &Seat::name);
        if (removed == self.seats.end())
            return;
        if (self.latest == removed->get())
            self.latest = nullptr;
        self.seats.erase(removed);
    }

    static void capabilities(void *data, wl_seat *, u32 capabilities)
    {
        auto &seat = *static_cast<Seat *>(data);
        const bool hasPointer = (capabilities & WL_SEAT_CAPABILITY_POINTER) != 0;
        if (hasPointer && !seat.pointer)
        {
            seat.pointer = wl_seat_get_pointer(seat.seat);
            wl_pointer_add_listener(seat.pointer, &kPointerListener, data);
        }
        else if (!hasPointer && seat.pointer)
        {
            wl_pointer_destroy(seat.pointer);
            seat.pointer = nullptr;
            seat.focus = nullptr;
            seat.pressed = false;
        }
    }

    static void enter(void *data, wl_pointer *, u32, wl_surface *surface, wl_fixed_t, wl_fixed_t)
    {
        static_cast<Seat *>(data)->focus = surface;
    }

    static void leave(void *data, wl_pointer *, u32, wl_surface *)
    {
        auto &seat = *static_cast<Seat *>(data);
        seat.focus = nullptr;
        seat.pressed = false;
    }

    static void motion(void *, wl_pointer *, u32, wl_fixed_t, wl_fixed_t)
    {
    }

    static void button(void *data, wl_pointer *, u32 serial, u32, u32 button, u32 state)
    {
        if (button != BTN_LEFT)
            return;
        auto &seat = *static_cast<Seat *>(data);
        seat.pressed = state == WL_POINTER_BUTTON_STATE_PRESSED;
        if (seat.pressed)
        {
            seat.pressSerial = serial;
            seat.owner->latest = &seat;
        }
    }

    static void axis(void *, wl_pointer *, u32, u32, wl_fixed_t)
    {
    }

    static const wl_registry_listener kRegistryListener;
    static const wl_seat_listener kSeatListener;
    static const wl_pointer_listener kPointerListener;

    wl_display *display;
    wl_surface *surface;
    wl_event_queue *queue = nullptr;
    wl_registry *registry = nullptr;
    std::vector<std::unique_ptr<Seat>> seats;
    Seat *latest = nullptr;
};

const wl_registry_listener FrameHitTest::WaylandSeats::kRegistryListener{.global = global,
                                                                         .global_remove = globalRemove};

//! Bound at version 1, so the later events these leave null are never sent.
const wl_seat_listener FrameHitTest::WaylandSeats::kSeatListener = []() noexcept
{
    wl_seat_listener listener{};
    listener.capabilities = capabilities;
    return listener;
}();

const wl_pointer_listener FrameHitTest::WaylandSeats::kPointerListener = []() noexcept
{
    wl_pointer_listener listener{};
    listener.enter = enter;
    listener.leave = leave;
    listener.motion = motion;
    listener.button = button;
    listener.axis = axis;
    return listener;
}();
#else
struct FrameHitTest::WaylandSeats
{
};
#endif

FrameHitTest::FrameHitTest(SDL_Window *window, HitTest hitTest) : window_(window), hitTest_(std::move(hitTest))
{
    installed_ = window_ && hitTest_ && SDL_SetWindowHitTest(window_, &FrameHitTest::thunk, this);

#ifdef WMA_ENABLE_WAYLAND
    const char *driver = SDL_GetCurrentVideoDriver();
    if (installed_ && driver && std::strcmp(driver, "wayland") == 0)
    {
        const SDL_PropertiesID props = SDL_GetWindowProperties(window_);
        auto *display =
            static_cast<wl_display *>(SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, nullptr));
        auto *surface =
            static_cast<wl_surface *>(SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, nullptr));
        if (display && surface)
            wayland_ = std::make_unique<WaylandSeats>(display, surface);
    }
#endif
}

FrameHitTest::~FrameHitTest()
{
    if (installed_)
        SDL_SetWindowHitTest(window_, nullptr, nullptr);
}

void FrameHitTest::pump()
{
#ifdef WMA_ENABLE_WAYLAND
    if (wayland_)
        wayland_->pump();
#endif
}

bool FrameHitTest::ownsWindow(SDL_WindowID id) const noexcept
{
    return id == SDL_GetWindowID(window_);
}

void FrameHitTest::startMove()
{
#ifdef WMA_ENABLE_WAYLAND
    if (wayland_)
    {
        auto *toplevel = static_cast<xdg_toplevel *>(SDL_GetPointerProperty(
            SDL_GetWindowProperties(window_), SDL_PROP_WINDOW_WAYLAND_XDG_TOPLEVEL_POINTER, nullptr));
        wayland_->startMove(toplevel);
    }
#endif
}

bool FrameHitTest::consume(const SDL_Event &event, IWindowManager &window)
{
    switch (event.type)
    {
    case SDL_EVENT_WINDOW_HIT_TEST:
    {
        if (!ownsWindow(event.window.windowID))
            return false;
        //! SDL already handed this press to the window manager; the pointer
        //! position it last reported is where the press happened.
        f32 x = 0.0f;
        f32 y = 0.0f;
        SDL_GetMouseState(&x, &y);
        if (hitTest_(x, y) == WindowHit::Caption)
            caption_.clicked(x, y);
        return true;
    }
    case SDL_EVENT_WINDOW_MOVED:
    case SDL_EVENT_WINDOW_FOCUS_LOST:
        //! A click leaves the window where it was; a press that moved it was a drag.
        if (ownsWindow(event.window.windowID))
            caption_.cancel();
        return false;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    {
        const f64 x = event.button.x;
        const f64 y = event.button.y;
        if (event.button.button != SDL_BUTTON_LEFT || !ownsWindow(event.button.windowID) ||
            hitTest_(x, y) != WindowHit::Caption)
            return false;
        if (caption_.press(x, y))
            detail::toggleMaximized(window);
        releasePending_ = true;
        return true;
    }
    case SDL_EVENT_MOUSE_MOTION:
        if (caption_.pending() && ownsWindow(event.motion.windowID) && caption_.moved(event.motion.x, event.motion.y))
            startMove();
        return false;
    case SDL_EVENT_MOUSE_BUTTON_UP:
        //! After a move this is the release SDL sends when the compositor takes the pointer.
        if (event.button.button != SDL_BUTTON_LEFT || !std::exchange(releasePending_, false))
            return false;
        caption_.released();
        return true;
    default:
        return false;
    }
}

bool serverDecorationsAvailable() noexcept
{
#ifdef WMA_ENABLE_WAYLAND
    const char *driver = SDL_GetCurrentVideoDriver();
    auto *display = static_cast<wl_display *>(
        SDL_GetPointerProperty(SDL_GetGlobalProperties(), SDL_PROP_GLOBAL_VIDEO_WAYLAND_WL_DISPLAY_POINTER, nullptr));
    if (!driver || std::strcmp(driver, "wayland") != 0 || !display)
        return true;
    return wayland::offersServerDecorations(display);
#else
    return true;
#endif
}

SDL_HitTestResult SDLCALL FrameHitTest::thunk(SDL_Window *window, const SDL_Point *point, void *data)
{
    auto &self = *static_cast<FrameHitTest *>(data);
    const f64 x = point->x;
    const f64 y = point->y;

    const WindowHit hit = self.hitTest_(x, y);
    if (hit == WindowHit::Caption)
    {
        //! On Wayland consume() moves the window itself. Elsewhere SDL moves on the
        //! press, except the second click of a double-click, which consume() needs.
        return self.wayland_ || self.caption_.armedNear(x, y) ? SDL_HITTEST_NORMAL : SDL_HITTEST_DRAGGABLE;
    }
    if ((SDL_GetWindowFlags(window) & SDL_WINDOW_RESIZABLE) == 0)
        return SDL_HITTEST_NORMAL;

    switch (hit)
    {
    case WindowHit::Top:
        return SDL_HITTEST_RESIZE_TOP;
    case WindowHit::Bottom:
        return SDL_HITTEST_RESIZE_BOTTOM;
    case WindowHit::Left:
        return SDL_HITTEST_RESIZE_LEFT;
    case WindowHit::Right:
        return SDL_HITTEST_RESIZE_RIGHT;
    case WindowHit::TopLeft:
        return SDL_HITTEST_RESIZE_TOPLEFT;
    case WindowHit::TopRight:
        return SDL_HITTEST_RESIZE_TOPRIGHT;
    case WindowHit::BottomLeft:
        return SDL_HITTEST_RESIZE_BOTTOMLEFT;
    case WindowHit::BottomRight:
        return SDL_HITTEST_RESIZE_BOTTOMRIGHT;
    case WindowHit::Client:
    case WindowHit::Caption:
        break;
    }
    return SDL_HITTEST_NORMAL;
}

} // namespace wma::sdl
#endif // WMA_ENABLE_SDL
