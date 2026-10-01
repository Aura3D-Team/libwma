#ifdef WMA_ENABLE_SDL
#include "backends/sdl/SdlFrameHitTest.hpp"

#include <cstring>
#include <utility>
#include <vector>

#ifdef WMA_ENABLE_WAYLAND
#include <linux/input-event-codes.h>
#include <wayland-client.h>
#endif

namespace wma::sdl
{

#ifdef WMA_ENABLE_WAYLAND
/// A second wl_pointer on SDL's connection, on a private queue so its listeners
/// never run inside SDL's own dispatch. libdecor's plugins use the same technique.
struct FrameHitTest::WaylandPresses
{
    struct Press
    {
        f64 x;
        f64 y;
    };

    WaylandPresses(wl_display *display, wl_surface *surface) : display(display), surface(surface)
    {
        queue = wl_display_create_queue(display);
        auto *wrapped = static_cast<wl_display *>(wl_proxy_create_wrapper(display));
        wl_proxy_set_queue(reinterpret_cast<wl_proxy *>(wrapped), queue);
        registry = wl_display_get_registry(wrapped);
        wl_proxy_wrapper_destroy(wrapped);
        wl_registry_add_listener(registry, &kRegistryListener, this);
        //! The first roundtrip binds the seat, the second receives its capabilities.
        wl_display_roundtrip_queue(display, queue);
        wl_display_roundtrip_queue(display, queue);
    }

    ~WaylandPresses()
    {
        dropSeat();
        if (registry)
            wl_registry_destroy(registry);
        if (queue)
            wl_event_queue_destroy(queue);
    }

    WaylandPresses(const WaylandPresses &) = delete;
    WaylandPresses &operator=(const WaylandPresses &) = delete;

    //! SDL has already read these from the socket; this only dispatches them.
    const std::vector<Press> &drain()
    {
        presses.clear();
        wl_display_dispatch_queue_pending(display, queue);
        return presses;
    }

    void dropSeat() noexcept
    {
        if (pointer)
            wl_pointer_destroy(pointer);
        if (seat)
            wl_seat_destroy(seat);
        pointer = nullptr;
        seat = nullptr;
        seatName = 0;
        focus = nullptr;
    }

    static WaylandPresses &self(void *data)
    {
        return *static_cast<WaylandPresses *>(data);
    }

    static void global(void *data, wl_registry *registry, u32 name, const char *interface, u32)
    {
        auto &presses = self(data);
        if (presses.seat || std::strcmp(interface, wl_seat_interface.name) != 0)
            return;
        presses.seat = static_cast<wl_seat *>(wl_registry_bind(registry, name, &wl_seat_interface, 1));
        presses.seatName = name;
        wl_seat_add_listener(presses.seat, &kSeatListener, data);
    }

    static void globalRemove(void *data, wl_registry *, u32 name)
    {
        if (self(data).seat && self(data).seatName == name)
            self(data).dropSeat();
    }

    static void capabilities(void *data, wl_seat *seat, u32 capabilities)
    {
        auto &presses = self(data);
        const bool hasPointer = (capabilities & WL_SEAT_CAPABILITY_POINTER) != 0;
        if (hasPointer && !presses.pointer)
        {
            presses.pointer = wl_seat_get_pointer(seat);
            wl_pointer_add_listener(presses.pointer, &kPointerListener, data);
        }
        else if (!hasPointer && presses.pointer)
        {
            wl_pointer_destroy(presses.pointer);
            presses.pointer = nullptr;
            presses.focus = nullptr;
        }
    }

    static void enter(void *data, wl_pointer *, u32, wl_surface *surface, wl_fixed_t x, wl_fixed_t y)
    {
        auto &presses = self(data);
        presses.focus = surface;
        presses.x = wl_fixed_to_double(x);
        presses.y = wl_fixed_to_double(y);
    }

    static void leave(void *data, wl_pointer *, u32, wl_surface *)
    {
        self(data).focus = nullptr;
    }

    static void motion(void *data, wl_pointer *, u32, wl_fixed_t x, wl_fixed_t y)
    {
        self(data).x = wl_fixed_to_double(x);
        self(data).y = wl_fixed_to_double(y);
    }

    static void button(void *data, wl_pointer *, u32, u32, u32 button, u32 state)
    {
        auto &presses = self(data);
        if (button == BTN_LEFT && state == WL_POINTER_BUTTON_STATE_PRESSED && presses.focus == presses.surface)
            presses.presses.push_back({presses.x, presses.y});
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
    wl_seat *seat = nullptr;
    u32 seatName = 0;
    wl_pointer *pointer = nullptr;
    wl_surface *focus = nullptr;
    f64 x = 0.0;
    f64 y = 0.0;
    std::vector<Press> presses;
};

const wl_registry_listener FrameHitTest::WaylandPresses::kRegistryListener{.global = global,
                                                                           .global_remove = globalRemove};

//! Bound at version 1, so the later events these leave null are never sent.
const wl_seat_listener FrameHitTest::WaylandPresses::kSeatListener = []
{
    wl_seat_listener listener{};
    listener.capabilities = capabilities;
    return listener;
}();

const wl_pointer_listener FrameHitTest::WaylandPresses::kPointerListener = []
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
struct FrameHitTest::WaylandPresses
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
            wayland_ = std::make_unique<WaylandPresses>(display, surface);
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
    {
        for (const auto &press : wayland_->drain())
            captionPressed(press.x, press.y);
    }
#endif
}

void FrameHitTest::captionPressed(f64 x, f64 y)
{
    //! A press on an armed spot is the second click, delivered by SDL to consume().
    if (hitTest_(x, y) == WindowHit::Caption && !clicks_.armedNear(x, y))
        (void)clicks_.press(x, y);
}

bool FrameHitTest::consume(const SDL_Event &event, IWindowManager &window)
{
    switch (event.type)
    {
    case SDL_EVENT_WINDOW_HIT_TEST:
    {
        if (event.window.windowID != SDL_GetWindowID(window_))
            return false;
        //! X11 reports the press it handed to the window manager; the pointer
        //! position it last reported is where that press happened.
        f32 x = 0.0f;
        f32 y = 0.0f;
        SDL_GetMouseState(&x, &y);
        captionPressed(x, y);
        return true;
    }
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    {
        const f64 x = event.button.x;
        const f64 y = event.button.y;
        if (event.button.button != SDL_BUTTON_LEFT || event.button.windowID != SDL_GetWindowID(window_) ||
            !clicks_.armedNear(x, y) || hitTest_(x, y) != WindowHit::Caption)
            return false;
        (void)clicks_.press(x, y);
        detail::toggleMaximized(window);
        releasePending_ = true;
        return true;
    }
    case SDL_EVENT_MOUSE_BUTTON_UP:
        return event.button.button == SDL_BUTTON_LEFT && std::exchange(releasePending_, false);
    default:
        return false;
    }
}

SDL_HitTestResult SDLCALL FrameHitTest::thunk(SDL_Window *window, const SDL_Point *point, void *data)
{
    auto &self = *static_cast<FrameHitTest *>(data);
    const f64 x = point->x;
    const f64 y = point->y;

    const WindowHit hit = self.hitTest_(x, y);
    if (hit == WindowHit::Caption)
    {
        //! Let the second click of a double-click reach consume().
        return self.clicks_.armedNear(x, y) ? SDL_HITTEST_NORMAL : SDL_HITTEST_DRAGGABLE;
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
