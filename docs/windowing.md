# Windowing

## Link it

```cmake
find_package(wma REQUIRED)
target_link_libraries(your_target PRIVATE wma::wma)
```

## Open a window

```cpp
#include <wma/wma.hpp>

wma::WindowDetails config{
    .width = 1280, .height = 720,
    .resizable = true,
    .targetFPS = 60,        // 0 = unlimited
    .fullscreen = false,
};

auto window = wma::createWindowManager(
    wma::getDefaultBackend(), config, wma::GraphicsAPI::CPU);

window->createWindow("My Game");
```

`getDefaultBackend()` picks what the build actually has, so this line needs no
`#ifdef` on desktop, Android or WASM.

## Frame loop

```cpp
window->process([&] {
    auto* flags = window->getWindowFlags();   // deltaTime, fps, resized, focused, minimized

    if (flags->resized) {
        const auto* d = window->getWindowDetails();
        resize(d->width, d->height);
        flags->resized = false;               // you clear it
    }
});
```

`process()` runs until `shouldClose()`. Call `window->close()` to request exit;
destroy the native window after releasing its rendering resources.

> On WASM `process()` drives `requestAnimationFrame` and never returns — put
> shutdown work in the loop, not after it.

## Application-drawn decorations

```cpp
#include <wma/wma.hpp>

auto makeDecoratedWindow()
{
    wma::WindowDetails details{
        .width = 1280, .height = 720,
        .decorationMode = wma::DecorationMode::ClientSide,
    };
    auto window = wma::createWindowManager(
        wma::WindowBackend::SDL3, details, wma::GraphicsAPI::Vulkan);
    window->createWindow("Aura3D");

    // Window-local logical coordinates; cheap and side-effect free.
    const bool native = window->setHitTest([](f64 x, f64 y) {
        if (y < 4) return wma::WindowHit::Top;
        if (y < 34 && x < 1100) return wma::WindowHit::Caption;
        return wma::WindowHit::Client;
    });
    (void)native; // false: draw no title bar, nothing could move the window
    return window;
}
```

WMA never draws decorations; it acts on the regions the application reports.

| Region | Press | Hover |
|---|---|---|
| `Client` | Delivered to the mouse listener | Application cursor |
| `Caption` | Native move once the pointer travels 4 px; a click-click toggles maximize | Application cursor |
| Edges and corners | Native resize (resizable windows) | Resize cursor |

`Caption` and edge presses never reach the mouse listener, and neither do their
releases: the window manager takes the grab, so no input state is left
half-pressed. Buttons drawn in the title bar must report `Client`.

| Backend | Move/resize | Notes |
|---|---|---|
| Native Wayland | `xdg_toplevel.move/resize` with the press serial | |
| Native X11 | EWMH `_NET_WM_MOVERESIZE` | Motif hints remove the frame |
| SDL3 | `SDL_SetWindowHitTest` | `false` in the browser and on Android |
| GLFW on X11 | EWMH through GLFW's native handles | GLFW 3.4 or newer |
| GLFW elsewhere | `false` | `ClientSide` falls back to GLFW's own frame; on Wayland without xdg-decoration that is GLFW's built-in frame, not libdecor's |

| UI action | WMA call |
|---|---|
| Minimize button | `window.minimize()` |
| Maximize/restore button | `window.isMaximized() ? window.restore() : window.maximize()` |
| Close button | `window.close()` |
| Application title changes | `window.setTitle("New title")` |
| Decide whether to paint decorations | `isToplevel()`, `getDecorationMode() == ClientSide` and `setHitTest()` succeeded |

Call controls on the event thread after window creation. Boolean results mean
a request was submitted; `false` means unsupported or invalid. Maximize state
is reported by the platform, so continue pumping events after requests.

The Wayland compositor chooses the final decoration mode. Query
`getDecorationMode()` after `createWindow()` and subsequent event dispatches;
without xdg-decoration it is `ClientSide`, on native Wayland and SDL3 alike. `restore()` unsets maximization;
Wayland provides no request to undo minimization. Custom Wayland surface roles
reject toplevel controls, but `close()` still signals the local event loop.

## Backends and graphics APIs

Two independent axes.

| `WindowBackend` | `GraphicsAPI` |
|---|---|
| `GLFW` `SDL3` `X11` `WAYLAND` | `OpenGL` `Vulkan` `CPU` `Metal` |

```cpp
if (wma::isBackendAvailable(wma::WindowBackend::WAYLAND)) { ... }
```

`createWindowManager` throws `WindowException` for a backend that was not
compiled in — check first, or take the default.

## Graphics interop

```cpp
window->getVulkanExtensions();      // required instance extensions
window->getGLProcAddress("glClear");
window->getMetalLayer();            // CAMetalLayer*
window->getWindowInstance();        // SDL_Window* / GLFWwindow* / X11 Window / wl_surface*
window->getNativeDisplayHandle();   // X11/Wayland only, else nullptr
```

## Software rendering

With `GraphicsAPI::CPU`:

```cpp
wma::SoftwareFramebuffer fb = window->lockFramebuffer();
if (fb.valid()) {
    for (i32 y = 0; y < fb.height; ++y) {
        auto* row = reinterpret_cast<u32*>(
            static_cast<u8*>(fb.pixels) + static_cast<size_t>(y) * fb.pitch);
        for (i32 x = 0; x < fb.width; ++x)
            row[x] = 0x00FF8000;      // XRGB8888
    }
    window->presentFramebuffer();
}
```

Pixels are 32-bit; channel order is backend-defined. Always go through `pitch`,
never `width * 4`.

## Errors

Everything derives from `wma::WMAException`. Catch most-derived first:
`WindowException`, `GraphicsException`, `InputException`, `AudioException`.
