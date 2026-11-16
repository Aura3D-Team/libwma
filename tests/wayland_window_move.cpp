#include "wma/backends/wayland/WaylandWindowManager.hpp"

#include <wayland-client.h>

#include <memory>
#include <utility>

void commitWaylandSurface(wma::IWindowManager &window)
{
    wl_surface_commit(static_cast<wl_surface *>(window.getWindowInstance()));
    wl_display_flush(static_cast<wl_display *>(window.getNativeDisplayHandle()));
}

std::unique_ptr<wma::IWindowManager> moveWaylandWindow(std::unique_ptr<wma::IWindowManager> window, bool assign)
{
    auto &source = static_cast<wma::WaylandWindowManager &>(*window);
    if (!assign)
        return std::make_unique<wma::WaylandWindowManager>(std::move(source));

    auto destination = std::make_unique<wma::WaylandWindowManager>(wma::WindowDetails{});
    *destination = std::move(source);
    return destination;
}
