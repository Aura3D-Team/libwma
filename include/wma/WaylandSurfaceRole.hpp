#ifndef WMA_WAYLAND_SURFACE_ROLE_HPP
#define WMA_WAYLAND_SURFACE_ROLE_HPP

#include "wma/managers/IWindowManager.hpp"

struct wl_display;
struct wl_surface;

namespace wma
{

//! An application-supplied role for a fresh, uncommitted Wayland surface.
//! The manager owns the connection and destroys the role before the surface.
class WaylandSurfaceRole
{
  public:
    virtual ~WaylandSurfaceRole() = default;
    virtual void attach(wl_display *, wl_surface *, WindowDetails *, WindowFlags *) = 0;
    virtual void rebind(WindowDetails *, WindowFlags *) noexcept = 0;
    [[nodiscard]] virtual bool configured() const noexcept = 0;
    [[nodiscard]] virtual bool shouldClose() const noexcept = 0;
    [[nodiscard]] virtual bool transparentFramebuffer() const noexcept
    {
        return false;
    }
    [[nodiscard]] virtual i32 bufferScale() const noexcept
    {
        return 1;
    }
};

//! Available in builds with WMA_HAS_WAYLAND. No protocol SDK headers required.
[[nodiscard]] std::unique_ptr<IWindowManager> createWaylandWindowManager(const WindowDetails &details, GraphicsAPI api,
                                                                         std::unique_ptr<WaylandSurfaceRole> role);

} // namespace wma
#endif
