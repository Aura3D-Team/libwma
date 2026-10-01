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
        wma::WindowBackend::WAYLAND, details, wma::GraphicsAPI::Vulkan);
    window->createWindow("Aura3D");
    return window;
}

void toggleMaximized(wma::IWindowManager& window)
{
    if (window.isMaximized())
        window.restore();
    else
        window.maximize();
}

void titleBarPressed(wma::IWindowManager& window)
{
    window.beginMove();
}

void resizeBorderPressed(wma::IWindowManager& window, wma::ResizeEdge edge)
{
    window.beginResize(edge);
}
```

Aura3D can call these through `renderer.getWindowManager()`. Its UI owns hit
testing, title text, buttons, borders and rendering; WMA owns native requests.

| UI action | WMA call |
|---|---|
| Minimize button | `window.minimize()` |
| Maximize/restore button | `toggleMaximized(window)` |
| Close button | `window.close()` |
| Application title changes | `window.setTitle("New title")` |
| Decide whether to paint decorations | `window.isToplevel() && window.getDecorationMode() == wma::DecorationMode::ClientSide` |

Call controls on the event thread after window creation. Boolean results mean
a request was submitted; `false` means unsupported or invalid. Maximize state
is reported by the platform, so continue pumping events after requests.

| Backend | Controls, title, decoration preference | Interactive move/resize |
|---|---|---|
| Native Wayland | xdg-shell + optional xdg-decoration | Active pointer-button grab |
| X11 | ICCCM/EWMH + Motif hints | `false` |
| SDL3 / GLFW | Native library calls | `false` |

Wayland uses the press serial and seat internally. Invoke move/resize from the
UI's pointer-down handler, or while that press remains held; release, leave,
device removal or a successful submission invalidates the grab. No global
coordinates or native handles pass through the UI. Non-resizable windows reject
resize requests. Native Wayland touch-driven move/resize is not implemented.
After a successful move/resize request, release any UI pointer capture: the
compositor may consume the button-release event.

The Wayland compositor chooses the final decoration mode. Query
`getDecorationMode()` after `createWindow()` and subsequent event dispatches;
without xdg-decoration it is `ClientSide`. `restore()` unsets maximization;
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
