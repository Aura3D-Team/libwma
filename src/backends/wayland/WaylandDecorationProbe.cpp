#include "backends/wayland/WaylandDecorationProbe.hpp"

#include <cstdlib>
#include <cstring>
#include <string_view>

#include <dirent.h>
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

bool libdecorHasPlugin() noexcept
{
    const char *dir = std::getenv("LIBDECOR_PLUGIN_DIR");
#ifdef WMA_LIBDECOR_PLUGIN_DIR
    if (!dir)
        dir = WMA_LIBDECOR_PLUGIN_DIR;
#endif
    if (!dir)
        return false;

    DIR *handle = opendir(dir);
    if (!handle)
        return false;

    bool found = false;
    while (const dirent *entry = readdir(handle))
    {
        const std::string_view name(entry->d_name);
        if (name.ends_with(".so"))
        {
            found = true;
            break;
        }
    }
    closedir(handle);
    return found;
}

} // namespace wma::wayland
