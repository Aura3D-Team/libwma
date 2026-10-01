# Application-owned Wayland roles

Include `<wma/WaylandSurfaceRole.hpp>` and pass an owned `WaylandSurfaceRole` to `createWaylandWindowManager(details, api, role)` in builds with `WMA_HAS_WAYLAND`.

| Owner | Responsibility |
|---|---|
| libwma | connection, uncommitted wl_surface, seat input, event dispatch, native rendering handles |
| role | protocol objects, configure acknowledgement, logical dimensions, scale, close state |

The manager calls `attach()` before the first commit, waits for `configured()`, and destroys the role before the wl_surface and display. `rebind()` updates borrowed details/flags pointers after a manager move. Mark `WindowFlags::resized` when dimensions or buffer scale change. Current custom-role rendering supports Vulkan; other graphics APIs fail explicitly.

Aura3D accepts an application window factory through `Engine(config, configPath, factory)`. This keeps layer-shell protocols and policy in a consumer such as libaurashell.

Toplevel controls (`minimize`, `maximize`, `restore`, `setTitle`, `beginMove`,
`beginResize`) return `false` for custom roles. `close()` signals the manager's
`shouldClose()` without destroying the role or rendering surface.
