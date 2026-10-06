#include "wma/backends/wayland/WaylandWindowManager.hpp"

#include <memory>
#include <utility>

std::unique_ptr<wma::IWindowManager> moveWaylandWindow(std::unique_ptr<wma::IWindowManager> window, bool assign)
{
    auto &source = static_cast<wma::WaylandWindowManager &>(*window);
    if (!assign)
        return std::make_unique<wma::WaylandWindowManager>(std::move(source));

    auto destination = std::make_unique<wma::WaylandWindowManager>(wma::WindowDetails{});
    *destination = std::move(source);
    return destination;
}
