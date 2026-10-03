// Drive the real backend over a private Wayland connection. The server only
// implements the protocols needed by a Vulkan window; it never renders.
#include "wma/WaylandSurfaceRole.hpp"
#include "xdg-decoration-unstable-v1-server-protocol.h"
#include "xdg-shell-server-protocol.h"

#include <linux/input-event-codes.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <unistd.h>
#include <wayland-client-core.h>
#include <wayland-server.h>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

// Kept in a separate TU: generated client and server protocol headers define
// the same enums and cannot both be included here.
std::unique_ptr<wma::IWindowManager> moveWaylandWindow(std::unique_ptr<wma::IWindowManager> window, bool assign);

namespace
{
int failures = 0;

void check(bool condition, const char *description)
{
    std::printf("[%s] %s\n", condition ? "PASS" : "FAIL", description);
    failures += !condition;
}

void require(bool condition, const char *description)
{
    if (!condition)
        throw std::runtime_error(description);
}

struct Requests
{
    int minimizes = 0;
    int maximizes = 0;
    int restores = 0;
    int moves = 0;
    int resizes = 0;
    uint32_t serial = 0;
    uint32_t edge = 0;
    uint32_t decorationMode = 0;
    bool correctSeat = false;
    std::string title;
};

class Compositor
{
  public:
    explicit Compositor(bool decorations = false, uint32_t chosenMode = ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE)
        : chosenMode_(chosenMode)
    {
        display_ = wl_display_create();
        require(display_, "create server display");
        wakeFd_ = eventfd(0, EFD_CLOEXEC);
        require(wakeFd_ >= 0, "create server wake event");
        wakeSource_ = wl_event_loop_add_fd(wl_display_get_event_loop(display_), wakeFd_, WL_EVENT_READABLE, wake, this);
        require(wakeSource_, "register server wake event");
        wl_global_create(display_, &wl_compositor_interface, 4, this, bindCompositor);
        seatGlobal_ = wl_global_create(display_, &wl_seat_interface, 1, this, bindSeat);
        wl_global_create(display_, &xdg_wm_base_interface, 1, this, bindShell);
        if (decorations)
            wl_global_create(display_, &zxdg_decoration_manager_v1_interface, 1, this, bindDecorations);

        std::array<int, 2> sockets{};
        require(socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sockets.data()) == 0,
                "create private Wayland socket");
        require(wl_client_create(display_, sockets[0]), "create server client");
        // wl_display_connect consumes WAYLAND_SOCKET and takes ownership of fd.
        require(setenv("WAYLAND_SOCKET", std::to_string(sockets[1]).c_str(), 1) == 0, "select private Wayland socket");
        thread_ = std::thread(
            [this]
            {
                wl_display_run(display_);
            });
    }

    template <class Fn> auto invoke(Fn fn)
    {
        auto task = std::make_shared<std::packaged_task<decltype(fn())()>>(std::move(fn));
        auto result = task->get_future();
        {
            std::lock_guard lock(mutex_);
            tasks_.emplace_back(
                [task]
                {
                    (*task)();
                });
        }
        const uint64_t value = 1;
        require(write(wakeFd_, &value, sizeof(value)) == sizeof(value), "wake compositor");
        return result.get();
    }

    ~Compositor() noexcept
    try
    {
        invoke(
            [this]
            {
                wl_display_terminate(display_);
            });
        thread_.join();
        wl_event_source_remove(wakeSource_);
        ::close(wakeFd_);
        wl_display_destroy_clients(display_);
        wl_display_destroy(display_);
    }
    catch (...)
    {
        std::terminate();
    }

    Requests requests()
    {
        return invoke(
            [this]
            {
                return requests_;
            });
    }

    void enter(uint32_t serial = 100)
    {
        invoke(
            [this, serial]
            {
                wl_pointer_send_enter(pointer_, serial, surface_, wl_fixed_from_int(12), 0);
            });
    }
    void button(uint32_t serial, uint32_t state)
    {
        invoke(
            [this, serial, state]
            {
                wl_pointer_send_button(pointer_, serial, 0, BTN_LEFT, state);
            });
    }
    void motion(int x, int y)
    {
        invoke(
            [this, x, y]
            {
                wl_pointer_send_motion(pointer_, 0, wl_fixed_from_int(x), wl_fixed_from_int(y));
            });
    }
    void leave()
    {
        invoke(
            [this]
            {
                wl_pointer_send_leave(pointer_, 200, surface_);
            });
    }
    void removePointer()
    {
        invoke(
            [this]
            {
                wl_seat_send_capabilities(seat_, 0);
            });
    }
    void removeSeat()
    {
        invoke(
            [this]
            {
                wl_global_remove(seatGlobal_);
            });
    }
    void configure(bool maximized, int width = 0, int height = 0, bool finish = true)
    {
        invoke(
            [this, maximized, width, height, finish]
            {
                sendToplevelConfigure(maximized, width, height);
                if (finish)
                    xdg_surface_send_configure(xdgSurface_, ++configureSerial_);
            });
    }
    void finishConfigure()
    {
        invoke(
            [this]
            {
                xdg_surface_send_configure(xdgSurface_, ++configureSerial_);
            });
    }
    void decorationConfigure(uint32_t mode)
    {
        invoke(
            [this, mode]
            {
                zxdg_toplevel_decoration_v1_send_configure(decoration_, mode);
            });
    }
    void closeWindow()
    {
        invoke(
            [this]
            {
                xdg_toplevel_send_close(toplevel_);
            });
    }

  private:
    wl_display *display_ = nullptr;
    wl_event_source *wakeSource_ = nullptr;
    int wakeFd_ = -1;
    std::thread thread_;
    std::mutex mutex_;
    std::vector<std::function<void()>> tasks_;
    wl_resource *seat_ = nullptr;
    wl_global *seatGlobal_ = nullptr;
    wl_resource *pointer_ = nullptr;
    wl_resource *surface_ = nullptr;
    wl_resource *xdgSurface_ = nullptr;
    wl_resource *toplevel_ = nullptr;
    wl_resource *decoration_ = nullptr;
    bool initialConfigure_ = false;
    uint32_t configureSerial_ = 1;
    uint32_t chosenMode_;
    Requests requests_;

    static Compositor &owner(wl_resource *resource)
    {
        return *static_cast<Compositor *>(wl_resource_get_user_data(resource));
    }
    static void destroy(wl_client *, wl_resource *resource)
    {
        wl_resource_destroy(resource);
    }
    static int wake(int fd, uint32_t, void *data)
    {
        uint64_t value;
        require(read(fd, &value, sizeof(value)) == sizeof(value), "read compositor wake event");
        auto &self = *static_cast<Compositor *>(data);
        std::vector<std::function<void()>> tasks;
        {
            std::lock_guard lock(self.mutex_);
            tasks.swap(self.tasks_);
        }
        for (auto &task : tasks)
            task();
        wl_display_flush_clients(self.display_);
        return 0;
    }

    void sendToplevelConfigure(bool maximized, int width, int height)
    {
        wl_array states;
        wl_array_init(&states);
        if (maximized)
            *static_cast<uint32_t *>(wl_array_add(&states, sizeof(uint32_t))) = XDG_TOPLEVEL_STATE_MAXIMIZED;
        xdg_toplevel_send_configure(toplevel_, width, height, &states);
        wl_array_release(&states);
    }

    static void bindCompositor(wl_client *client, void *data, uint32_t version, uint32_t id)
    {
        static const struct wl_compositor_interface impl = {
            .create_surface =
                [](wl_client *client, wl_resource *resource, uint32_t id)
            {
                static const struct wl_surface_interface impl = []
                {
                    struct wl_surface_interface value{};
                    value.destroy = destroy;
                    value.commit = [](wl_client *, wl_resource *resource)
                    {
                        auto &self = owner(resource);
                        if (!self.toplevel_ || self.initialConfigure_ || resource != self.surface_)
                            return;
                        self.initialConfigure_ = true;
                        if (self.decoration_)
                            zxdg_toplevel_decoration_v1_send_configure(self.decoration_, self.chosenMode_);
                        self.sendToplevelConfigure(false, 0, 0);
                        xdg_surface_send_configure(self.xdgSurface_, ++self.configureSerial_);
                    };
                    return value;
                }();
                auto *surface =
                    wl_resource_create(client, &wl_surface_interface, wl_resource_get_version(resource), id);
                wl_resource_set_implementation(surface, &impl, &owner(resource), nullptr);
            },
            .create_region = nullptr,
        };
        auto *resource = wl_resource_create(client, &wl_compositor_interface, static_cast<int>(version), id);
        wl_resource_set_implementation(resource, &impl, data, nullptr);
    }

    static void bindSeat(wl_client *client, void *data, uint32_t version, uint32_t id)
    {
        static const struct wl_seat_interface impl = []
        {
            struct wl_seat_interface value{};
            value.get_pointer = [](wl_client *client, wl_resource *resource, uint32_t id)
            {
                static const struct wl_pointer_interface impl = {
                    .set_cursor =
                        [](wl_client *, wl_resource *, uint32_t, wl_resource *, int32_t, int32_t)
                    {
                    },
                    .release = destroy,
                };
                auto &self = owner(resource);
                self.pointer_ = wl_resource_create(client, &wl_pointer_interface, 1, id);
                wl_resource_set_implementation(self.pointer_, &impl, &self, nullptr);
            };
            return value;
        }();
        auto &self = *static_cast<Compositor *>(data);
        self.seat_ = wl_resource_create(client, &wl_seat_interface, static_cast<int>(version), id);
        wl_resource_set_implementation(self.seat_, &impl, data, nullptr);
        wl_seat_send_capabilities(self.seat_, WL_SEAT_CAPABILITY_POINTER);
    }

    static void bindShell(wl_client *client, void *data, uint32_t version, uint32_t id)
    {
        static const struct xdg_wm_base_interface impl = {
            .destroy = destroy,
            .create_positioner = nullptr,
            .get_xdg_surface =
                [](wl_client *client, wl_resource *resource, uint32_t id, wl_resource *surface)
            {
                static const struct xdg_surface_interface impl = []
                {
                    struct xdg_surface_interface value{};
                    value.destroy = destroy;
                    value.get_toplevel = [](wl_client *client, wl_resource *resource, uint32_t id)
                    {
                        static const struct xdg_toplevel_interface impl = []
                        {
                            struct xdg_toplevel_interface value{};
                            value.destroy = destroy;
                            value.set_title = [](wl_client *, wl_resource *resource, const char *title)
                            {
                                owner(resource).requests_.title = title;
                            };
                            value.set_app_id = [](wl_client *, wl_resource *, const char *)
                            {
                            };
                            value.move = [](wl_client *, wl_resource *resource, wl_resource *seat, uint32_t serial)
                            {
                                auto &self = owner(resource);
                                ++self.requests_.moves;
                                self.requests_.serial = serial;
                                self.requests_.correctSeat = seat == self.seat_;
                            };
                            value.resize = [](wl_client *, wl_resource *resource, wl_resource *seat, uint32_t serial,
                                              uint32_t edge)
                            {
                                auto &self = owner(resource);
                                ++self.requests_.resizes;
                                self.requests_.serial = serial;
                                self.requests_.edge = edge;
                                self.requests_.correctSeat = seat == self.seat_;
                            };
                            value.set_max_size = [](wl_client *, wl_resource *, int32_t, int32_t)
                            {
                            };
                            value.set_min_size = [](wl_client *, wl_resource *, int32_t, int32_t)
                            {
                            };
                            value.set_maximized = [](wl_client *, wl_resource *resource)
                            {
                                ++owner(resource).requests_.maximizes;
                            };
                            value.unset_maximized = [](wl_client *, wl_resource *resource)
                            {
                                ++owner(resource).requests_.restores;
                            };
                            value.set_minimized = [](wl_client *, wl_resource *resource)
                            {
                                ++owner(resource).requests_.minimizes;
                            };
                            return value;
                        }();
                        auto &self = owner(resource);
                        self.toplevel_ = wl_resource_create(client, &xdg_toplevel_interface, 1, id);
                        wl_resource_set_implementation(self.toplevel_, &impl, &self, nullptr);
                    };
                    value.ack_configure = [](wl_client *, wl_resource *, uint32_t)
                    {
                    };
                    return value;
                }();
                auto &self = owner(resource);
                self.surface_ = surface;
                self.xdgSurface_ = wl_resource_create(client, &xdg_surface_interface, 1, id);
                wl_resource_set_implementation(self.xdgSurface_, &impl, &self, nullptr);
            },
            .pong =
                [](wl_client *, wl_resource *, uint32_t)
            {
            },
        };
        auto *resource = wl_resource_create(client, &xdg_wm_base_interface, static_cast<int>(version), id);
        wl_resource_set_implementation(resource, &impl, data, nullptr);
    }

    static void bindDecorations(wl_client *client, void *data, uint32_t version, uint32_t id)
    {
        static const struct zxdg_decoration_manager_v1_interface impl = {
            .destroy = destroy,
            .get_toplevel_decoration =
                [](wl_client *client, wl_resource *resource, uint32_t id, wl_resource *)
            {
                static const struct zxdg_toplevel_decoration_v1_interface impl = {
                    .destroy = destroy,
                    .set_mode =
                        [](wl_client *, wl_resource *resource, uint32_t mode)
                    {
                        owner(resource).requests_.decorationMode = mode;
                    },
                    .unset_mode =
                        [](wl_client *, wl_resource *)
                    {
                    },
                };
                auto &self = owner(resource);
                self.decoration_ = wl_resource_create(client, &zxdg_toplevel_decoration_v1_interface, 1, id);
                wl_resource_set_implementation(self.decoration_, &impl, &self, nullptr);
            },
        };
        auto *resource =
            wl_resource_create(client, &zxdg_decoration_manager_v1_interface, static_cast<int>(version), id);
        wl_resource_set_implementation(resource, &impl, data, nullptr);
    }
};

wma::WindowHit caption(f64, f64)
{
    return wma::WindowHit::Caption;
}

void sync(wma::IWindowManager &window)
{
    auto *display = static_cast<wl_display *>(window.getNativeDisplayHandle());
    // The second roundtrip also flushes requests made by input callbacks
    // dispatched during the first roundtrip.
    require(wl_display_roundtrip(display) >= 0 && wl_display_roundtrip(display) >= 0,
            "roundtrip without protocol errors");
}

void testControlsAndConfigure()
{
    Compositor server;
    auto window = wma::createWaylandWindowManager({}, wma::GraphicsAPI::Vulkan, {});
    check(!window->minimize() && !window->maximize() && !window->restore() && !window->setHitTest(caption) &&
              !window->setTitle("before create"),
          "controls reject calls before a native toplevel exists");
    window->createWindow("initial title");
    check(window->isToplevel(), "xdg_toplevel windows identify themselves as toplevels");
    sync(*window);
    check(window->getDecorationMode() == wma::DecorationMode::ClientSide,
          "missing decoration protocol reports client-side fallback");
    check(window->setTitle("Aura3D \xc3\xa9") && !window->setTitle(nullptr), "title accepts UTF-8 and rejects null");
    check(window->minimize() && window->maximize(), "minimize and maximize queue requests");
    sync(*window);
    auto requests = server.requests();
    check(requests.title == "Aura3D \xc3\xa9" && requests.minimizes == 1 && requests.maximizes == 1,
          "title, minimize, and maximize reach xdg_toplevel");
    check(!window->isMaximized(), "maximize request does not optimistically change compositor state");
    server.configure(true, 0, 0, false);
    sync(*window);
    check(!window->isMaximized(), "toplevel state waits for the matching surface configure");
    server.finishConfigure();
    sync(*window);
    check(window->isMaximized() && window->getWindowDetails()->width == 800 &&
              window->getWindowDetails()->height == 600,
          "zero-size configure updates state and preserves dimensions");
    check(window->restore(), "restore queues unset_maximized");
    sync(*window);
    check(server.requests().restores == 1 && window->isMaximized(), "restore state also waits for configure");
    server.configure(false, 1024, 768);
    sync(*window);
    check(!window->isMaximized() && window->getWindowDetails()->width == 1024 &&
              window->getWindowDetails()->height == 768,
          "restored configure updates size and state");
    server.configure(true, 1920, 1080);
    sync(*window);
    server.configure(false);
    sync(*window);
    check(!window->isMaximized() && window->getWindowDetails()->width == 1024 &&
              window->getWindowDetails()->height == 768,
          "zero-size restore returns to the last floating size");
    server.closeWindow();
    sync(*window);
    check(window->shouldClose(), "compositor close sets the existing shouldClose flag");
}

void testHitTest()
{
    Compositor server;
    auto window = wma::createWaylandWindowManager({}, wma::GraphicsAPI::Vulkan, {});
    window->createWindow("hit test");
    sync(*window);

    wma::WindowHit region = wma::WindowHit::Client;
    check(window->setHitTest(
              [&region](f64, f64)
              {
                  return region;
              }),
          "toplevels accept a hit test");
    int presses = 0;
    int releases = 0;
    window->getMouseListener().addButtonAction(wma::MouseButton::WMALeft, wma::MouseAction(
                                                                              [&presses]
                                                                              {
                                                                                  ++presses;
                                                                              },
                                                                              [&releases]
                                                                              {
                                                                                  ++releases;
                                                                              }));
    server.enter();
    server.button(300, WL_POINTER_BUTTON_STATE_PRESSED);
    server.button(301, WL_POINTER_BUTTON_STATE_RELEASED);
    sync(*window);
    check(presses == 1 && releases == 1 && server.requests().moves == 0, "client presses reach the application");

    region = wma::WindowHit::Caption;
    server.button(321, WL_POINTER_BUTTON_STATE_PRESSED);
    sync(*window);
    check(server.requests().moves == 0 && presses == 1, "a caption press waits for the pointer to travel");
    server.motion(30, 0);
    sync(*window);
    const auto move = server.requests();
    check(move.moves == 1 && move.serial == 321 && move.correctSeat && presses == 1,
          "travel turns the caption press into a move with its own serial and seat, unseen by the application");

    //! The compositor kept the release; the pointer is back on the same spot of the moved window.
    server.button(330, WL_POINTER_BUTTON_STATE_PRESSED);
    sync(*window);
    check(server.requests().maximizes == 0, "grabbing the bar again right after a drag does not maximize");
    server.motion(50, 0);
    sync(*window);
    check(server.requests().moves == 2 && server.requests().serial == 330, "the second grab moves the window again");

    server.leave();
    server.enter();
    server.button(340, WL_POINTER_BUTTON_STATE_PRESSED);
    server.button(341, WL_POINTER_BUTTON_STATE_RELEASED);
    server.button(342, WL_POINTER_BUTTON_STATE_PRESSED);
    server.button(343, WL_POINTER_BUTTON_STATE_RELEASED);
    sync(*window);
    check(server.requests().maximizes == 1 && server.requests().moves == 2 && presses == 1 && releases == 1,
          "a caption double-click toggles maximize, and its presses and releases stay hidden");

    constexpr std::array edges = {
        std::pair{wma::WindowHit::Top, XDG_TOPLEVEL_RESIZE_EDGE_TOP},
        std::pair{wma::WindowHit::Bottom, XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM},
        std::pair{wma::WindowHit::Left, XDG_TOPLEVEL_RESIZE_EDGE_LEFT},
        std::pair{wma::WindowHit::Right, XDG_TOPLEVEL_RESIZE_EDGE_RIGHT},
        std::pair{wma::WindowHit::TopLeft, XDG_TOPLEVEL_RESIZE_EDGE_TOP_LEFT},
        std::pair{wma::WindowHit::TopRight, XDG_TOPLEVEL_RESIZE_EDGE_TOP_RIGHT},
        std::pair{wma::WindowHit::BottomLeft, XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM_LEFT},
        std::pair{wma::WindowHit::BottomRight, XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM_RIGHT},
    };
    uint32_t serial = 400;
    bool edgesCorrect = true;
    for (const auto [hit, nativeEdge] : edges)
    {
        region = hit;
        server.button(serial, WL_POINTER_BUTTON_STATE_PRESSED);
        server.button(serial + 1, WL_POINTER_BUTTON_STATE_RELEASED);
        sync(*window);
        const auto request = server.requests();
        edgesCorrect &=
            request.serial == serial && request.edge == static_cast<uint32_t>(nativeEdge) && request.correctSeat;
        serial += 2;
    }
    check(edgesCorrect && server.requests().resizes == 8 && presses == 1,
          "all eight border regions resize with the correct protocol edge");

    region = wma::WindowHit::Caption;
    server.leave();
    server.button(500, WL_POINTER_BUTTON_STATE_PRESSED);
    sync(*window);
    check(server.requests().moves == 2, "presses outside the focused surface start nothing");
    server.enter();
    check(window->setHitTest({}), "the hit test can be cleared");
    server.button(501, WL_POINTER_BUTTON_STATE_PRESSED);
    sync(*window);
    check(server.requests().moves == 2 && presses == 3, "without a hit test every press reaches the application");
    window->getMouseListener().clearAllActions();
    window->close();
    check(window->shouldClose() && window->getWindowInstance(),
          "application close marks the window without destroying it");
}

void testDecorationNegotiation()
{
    Compositor server(true, ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
    wma::WindowDetails details;
    details.decorationMode = wma::DecorationMode::ClientSide;
    auto window = wma::createWaylandWindowManager(details, wma::GraphicsAPI::Vulkan, {});
    window->createWindow("decorations");
    sync(*window);
    check(server.requests().decorationMode == ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE,
          "explicit client-side preference is sent to xdg-decoration");
    check(window->getDecorationMode() == wma::DecorationMode::ServerSide,
          "effective decoration mode follows the compositor's decision");
    server.decorationConfigure(ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE);
    sync(*window);
    check(window->getDecorationMode() == wma::DecorationMode::ServerSide,
          "decoration changes wait for the matching surface configure");
    server.finishConfigure();
    sync(*window);
    check(window->getDecorationMode() == wma::DecorationMode::ClientSide,
          "surface configure applies the negotiated client-side mode");
}

class CustomRole : public wma::WaylandSurfaceRole
{
  public:
    void attach(wl_display *, wl_surface *, wma::WindowDetails *, wma::WindowFlags *) override
    {
    }
    void rebind(wma::WindowDetails *, wma::WindowFlags *) noexcept override
    {
    }
    bool configured() const noexcept override
    {
        return true;
    }
    bool shouldClose() const noexcept override
    {
        return false;
    }
};

void testCustomRole()
{
    Compositor server;
    auto window = wma::createWaylandWindowManager({}, wma::GraphicsAPI::Vulkan, std::make_unique<CustomRole>());
    window->createWindow("custom surface role");
    check(!window->isToplevel(), "custom roles do not receive application title bars");
    check(!window->minimize() && !window->maximize() && !window->restore() && !window->setHitTest(caption) &&
              !window->setTitle("custom") && !window->isMaximized(),
          "custom surface roles safely reject xdg_toplevel-only controls");
    window->close();
    check(window->shouldClose(), "custom-role windows support application close");
    sync(*window);
}

void testFixedSizeWindow()
{
    Compositor server;
    wma::WindowDetails details;
    details.resizable = false;
    auto window = wma::createWaylandWindowManager(details, wma::GraphicsAPI::Vulkan, {});
    window->createWindow("fixed size");
    sync(*window);
    wma::WindowHit region = wma::WindowHit::BottomRight;
    window->setHitTest(
        [&region](f64, f64)
        {
            return region;
        });
    int presses = 0;
    window->getMouseListener().addButtonAction(wma::MouseButton::WMALeft, wma::MouseAction(
                                                                              [&presses]
                                                                              {
                                                                                  ++presses;
                                                                              }));
    server.enter();
    server.button(600, WL_POINTER_BUTTON_STATE_PRESSED);
    server.button(601, WL_POINTER_BUTTON_STATE_RELEASED);
    sync(*window);
    region = wma::WindowHit::Caption;
    server.button(602, WL_POINTER_BUTTON_STATE_PRESSED);
    server.motion(30, 0);
    sync(*window);
    check(server.requests().resizes == 0 && presses == 1 && server.requests().moves == 1,
          "fixed-size windows pass border presses to the application but still move");
}

void testMovesAndSeatRemoval()
{
    Compositor server(true, ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
    auto window = wma::createWaylandWindowManager({}, wma::GraphicsAPI::Vulkan, {});
    window->createWindow("move ownership");
    sync(*window);
    check(server.requests().decorationMode == ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE,
          "default decoration preference requests server-side decorations");

    check(window->setHitTest(caption), "a hit test is installed before moving the manager");
    for (const bool assign : {false, true})
    {
        server.configure(true, 0, 0, false);
        server.decorationConfigure(ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE);
        sync(*window);
        window = moveWaylandWindow(std::move(window), assign);
        server.finishConfigure();
        sync(*window);
        check(window->isMaximized() && window->getDecorationMode() == wma::DecorationMode::ClientSide,
              assign ? "move assignment preserves pending configure state"
                     : "move construction preserves pending configure state");

        server.configure(false);
        server.decorationConfigure(ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
        server.finishConfigure();
        sync(*window);
        check(!window->isMaximized() && window->getDecorationMode() == wma::DecorationMode::ServerSide,
              "configure listeners point to the new owner after moving");
    }

    server.enter();
    server.button(700, WL_POINTER_BUTTON_STATE_PRESSED);
    server.motion(30, 0);
    sync(*window);
    check(server.requests().moves == 1 && server.requests().serial == 700,
          "the hit test and pointer follow the moved manager");
    server.button(701, WL_POINTER_BUTTON_STATE_RELEASED);
    server.removeSeat();
    sync(*window);
    server.closeWindow();
    sync(*window);
    check(window->shouldClose(), "close events address the moved manager");
    window->destroy();
    check(!window->setHitTest(caption) && !window->maximize() && !window->restore() && !window->minimize() &&
              !window->setTitle("destroyed") && !window->isMaximized(),
          "controls safely reject calls after native resources are destroyed");
}
} // namespace

int main()
{
    try
    {
        testControlsAndConfigure();
        testHitTest();
        testDecorationNegotiation();
        testCustomRole();
        testFixedSizeWindow();
        testMovesAndSeatRemoval();
    }
    catch (const std::exception &error)
    {
        std::fprintf(stderr, "Wayland test failed: %s\n", error.what());
        return 1;
    }
    return failures ? 1 : 0;
}
