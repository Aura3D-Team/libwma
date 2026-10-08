#include "backends/wayland/WaylandDecorationProbe.hpp"

#include <cstring>

#include <wayland-client.h>

#include <ink/ink_base.hpp>

#include "wma/backends/wayland/protocols/xdg-decoration-unstable-v1-client-protocol.h"

namespace wma::wayland
{

bool offersServerDecorations(wl_display *display) noexcept
{
    if (!display)
        return false;

    wl_event_queue *queue = wl_display_create_queue(display);
    auto *wrapped = static_cast<wl_display *>(wl_proxy_create_wrapper(display));
    wl_proxy_set_queue(reinterpret_cast<wl_proxy *>(wrapped), queue);
    wl_registry *registry = wl_display_get_registry(wrapped);
    wl_proxy_wrapper_destroy(wrapped);

    static const wl_registry_listener kListener{
        .global =
            [](void *data, wl_registry *, u32, const char *interface, u32)
        {
            if (std::strcmp(interface, zxdg_decoration_manager_v1_interface.name) == 0)
                *static_cast<bool *>(data) = true;
        },
        .global_remove =
            [](void *, wl_registry *, u32)
        {
        }};
    bool found = false;
    wl_registry_add_listener(registry, &kListener, &found);
    wl_display_roundtrip_queue(display, queue);
    wl_registry_destroy(registry);
    wl_event_queue_destroy(queue);
    return found;
}

} // namespace wma::wayland
