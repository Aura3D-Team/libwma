# WMA — Window Management Abstraction Library

WMA is a modern **C++23** window-management and input library that provides a
unified interface for creating and driving windows across multiple backends
(**GLFW / SDL3 / X11 / Wayland**) and graphics APIs
(**OpenGL / Vulkan / Metal / software**).

The same code runs on **Linux, Windows, macOS, iOS, Android and the Web (WASM)** —
SDL3 is the portable backend and the only one used on Android, iOS and WASM.

## ✨ Features

- **Multiple backends** — GLFW, SDL3, X11, Wayland
- **Graphics APIs** — OpenGL, Vulkan, Metal, and software (CPU) rendering
- **Metal on Apple** — a `GraphicsAPI::Metal` window hosts a `CAMetalLayer`, handed
  to the renderer through `getMetalLayer()`; macOS via SDL3 or GLFW, iOS via SDL3
- **Parallel software rendering** — `parallelFill()` spreads CPU pixel work across
  hardware threads, one row-band per core, the way a GPU spreads it across cores
- **Cross-platform** — desktop, Android (SDL3), iOS (SDL3) and WebAssembly (SDL3)
- **Modern C++23** — `enum class`, deducing-this input tables, `move_only_function`
- **Zero-alloc input** — free-function callbacks take a raw fn-pointer fast path
- **Layered input contexts** — switch/stack contexts for gameplay, menus, overlays
- **RAII** — subsystems are reference-counted, so multiple windows coexist safely

## Directory structure

```
libwma/
├── include/wma/
│   ├── core/            # Types, WindowDetails, WindowFlags, FrameTimer, BuildConfig
│   ├── input/           # Keyboard/mouse listeners, binding tables, contexts
│   ├── managers/        # IWindowManager interface
│   ├── exceptions/      # WMAException hierarchy
│   ├── rendering/       # SoftwareRenderer — parallel CPU framebuffer fill
│   ├── platform/apple/  # AppleMetalLayer — CAMetalLayer hosting for GLFW
│   └── backends/        # glfw / sdl / x11 / wayland
├── src/                 # Implementations + factory (WindowManager.cpp)
├── examples/basic_window/
├── cmake/
└── CMakeLists.txt
```

## 🚀 Quick start

```cpp
#include <wma/wma.hpp>

int main() {
    wma::WindowDetails cfg(1280, 720, /*resizable*/ true, /*targetFPS*/ 60);

    auto window = wma::createWindowManager(
        wma::WindowBackend::SDL3,   // or ::GLFW / ::X11 / ::WAYLAND
        cfg,
        wma::GraphicsAPI::CPU       // or ::OpenGL / ::Vulkan / ::Metal
    );
    window->createWindow("Hello WMA");

    auto& keyboard = window->getKeyboardListener();
    keyboard.addKeyAction(wma::Key::KEY_ESCAPE,
        wma::KeyAction{ [&]{ window->destroy(); } });

    // Managed loop (blocks on desktop, hands the loop to the browser on WASM):
    window->process([&]{
        wma::SoftwareFramebuffer fb = window->lockFramebuffer(); // CPU mode
        if (fb.valid()) {
            /* write 32-bit XRGB pixels into fb.pixels (fb.pitch bytes/row) */
            window->presentFramebuffer();
        }
    });
}
```

See [examples/basic_window/main.cpp](examples/basic_window/main.cpp) for a full
tour of the API — every keyboard/mouse method, both context models, the
Vulkan/OpenGL interop calls, and an animated software framebuffer — built with
`-DWMA_BUILD_EXAMPLES=ON`. It compiles **unmodified** for desktop, Android and
WASM (it picks its backend via `getDefaultBackend()`, which resolves to SDL3 on
every platform where SDL3 is enabled):

| Platform | Example artifact | Notes |
|---|---|---|
| Desktop | `wma_basic_window` executable | Run directly |
| Android | `libmain.so` (shared library) | Named `main` to match what SDL's `SDLActivity` loads via JNI. This repo only produces the `.so` — wiring it into an APK (e.g. SDL's `android-project` template, or your own `app/src/main/jniLibs`/`externalNativeBuild`) is left to whichever **Gradle** project consumes it; this build has no Gradle step of its own. |
| WASM | `wma_basic_window.html` / `.js` / `.wasm` | Uses a minimal shell ([shell.wasm.html](examples/basic_window/shell.wasm.html)) with the `<canvas id="canvas">` SDL3's Emscripten backend expects. Serve the three files from any static web server. |

## 🛠️ Building

### Requirements

- A C++23 compiler (GCC 14+, Clang 18+, or MSVC 19.4x+)
- CMake ≥ 4.3.3
- [`ink`](https://github.com/Arthu-RL/libink) (required)
- At least one backend SDK: **SDL3 3.2+**, **GLFW 3.4+**, **Xlib**, or **Wayland**
- Optional for OpenGL on the native backends: GLX (X11) / EGL + `wayland-egl` (Wayland)

### Desktop build & install

```bash
git clone https://github.com/Arthu-RL/libwma.git

cmake -S libwma -B libwma/build \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr/local \
    -DWMA_ENABLE_SDL=ON \
    -DWMA_ENABLE_GLFW=ON \
    -DWMA_ENABLE_X11=ON \
    -DWMA_ENABLE_WAYLAND=ON \
    -DWMA_BUILD_EXAMPLES=ON

cmake --build libwma/build --target install
```

Or use the `linux-debug`/`linux-release` presets directly — both build the
example (`WMA_BUILD_EXAMPLES=ON`) with every backend enabled except that
`linux-release` leaves GLFW off (see caveat below):
`cmake --preset linux-release && cmake --build --preset linux-release`.
On Windows, `windows-debug`/`windows-release` configure the same project with
an MSVC-aware toolchain from a Developer Command Prompt (or any shell with
`vcvarsall`/VS Build Tools on `PATH`) — `src/CMakeLists.txt` selects `/W4` +
`/Od`/`/O2` automatically instead of the GCC/Clang `-Wall`/`-O3` flags.

> Building the example with **both** `WMA_ENABLE_SDL=ON` and
> `WMA_ENABLE_GLFW=ON` can fail at the link step on some distros: prebuilt
> static `libSDL3.a` and `libglfw3.a` each vendor their own copy of the
> `wp_fractional_scale_manager_v1` Wayland-protocol glue as a strong symbol,
> which collides. This is a packaging issue in those two static libs, not a
> wma bug — if you hit it, drop `-DWMA_ENABLE_GLFW=OFF` for the example build,
> or link SDL3/GLFW as shared libraries instead.

### macOS & iOS (Metal)

`Platform.cmake` forces X11/Wayland off on Apple — neither can host a
`CAMetalLayer` — and turns on `WMA_HAS_METAL`, which compiles the one
Objective-C++ translation unit (`src/platform/apple/AppleMetalLayer.mm`) and links
QuartzCore/Foundation/AppKit:

```bash
# macOS (needs SDL3 and/or GLFW; `brew install sdl3 glfw`)
cmake --preset macos-release && cmake --build --preset macos-release

# iOS (SDL3 only — GLFW has no iOS port; SDL3 must be built for arm64-apple-ios)
cmake --preset ios && cmake --build --preset ios
```

`Metal` needs no SDK of its own: the frameworks ship with Xcode. Note that wma
creates the layer but never a `MTLDevice` — setting the device and pixel format
is the renderer's job, so `Metal` is deliberately *not* among the linked
frameworks here.

### Android (SDL3) & Web (WASM, SDL3)

The presets force SDL3 as the sole backend on these targets:

```bash
# Android (needs ANDROID_NDK_HOME + SDL3 built for the ABI)
cmake --preset android -DWMA_BUILD_EXAMPLES=ON && cmake --build --preset android

# WebAssembly (needs EMSDK + SDL3 built for wasm)
cmake --preset wasm -DWMA_BUILD_EXAMPLES=ON && cmake --build --preset wasm
```

Neither preset touches Gradle or any web tooling — they only produce the native
artifact (`.so` / `.html`+`.js`+`.wasm`) described in the table above.

### CMake options

| Option | Default | Description |
|--------|:-------:|-------------|
| `WMA_ENABLE_SDL` | `OFF` | Enable SDL3 backend (forced `ON` for Android/iOS/WASM) |
| `WMA_ENABLE_GLFW` | `OFF` | Enable GLFW backend (desktop only; forced `OFF` on iOS) |
| `WMA_ENABLE_X11` | `OFF` | Enable X11 backend (Linux only; forced `OFF` on Apple/Windows) |
| `WMA_ENABLE_WAYLAND` | `OFF` | Enable Wayland backend (Linux only; forced `OFF` on Apple/Windows) |
| `WMA_BUILD_EXAMPLES` | `OFF` | Build the example applications |
| `WMA_BUILD_TESTS` | `OFF` | Build the test suite (if `tests/` is present) |
| `WMA_ENABLE_LTO` | `ON` | Link-time optimization for Release |
| `WMA_NATIVE_OPTIMIZE` | `OFF` | `-march=native` (non-portable; local builds only; no-op on MSVC) |

> At least one backend must be enabled, otherwise `createWindowManager()` throws.

> Wayland windows get **server-side decorations only** (titlebar/close/min/max),
> requested via `xdg-decoration-unstable-v1` when the compositor advertises it
> (KDE, Sway, other wlroots compositors). Compositors that don't — GNOME/Mutter
> — fall back to a borderless surface; this library does not draw its own
> client-side decorations.

### Consuming with CMake

`find_package` re-discovers whatever backends the library was built with — your
own code needs no backend SDK on its include path, only `wma.hpp`.

```cmake
find_package(wma REQUIRED)
target_link_libraries(your_target PRIVATE wma::wma)
```

Query what was compiled in at runtime — or at compile time via the generated
`<wma/core/BuildConfig.hpp>` (`WMA_HAS_SDL`, `WMA_HAS_GLFW`, …).

## 📚 API overview

### `WindowDetails` — creation config

```cpp
wma::WindowDetails cfg {
    .width = 1920, .height = 1080,
    .resizable = true,
    .targetFPS = 144,   // 0 = unlimited
    .fullscreen = false
};
```

### `IWindowManager` — the window

| Method | Purpose |
|--------|---------|
| `createWindow(name)` | Create the OS window / surface |
| `process(fn)` | Managed loop; WASM-safe (uses `requestAnimationFrame`) |
| `pollEvents()` / `swapBuffers()` | Manual frame stepping (any platform) |
| `waitEvents(timeoutMs)` | `pollEvents()` that sleeps up to `timeoutMs` for an event first — for a loop that redraws on change and has no frame to block on. Never waits under Emscripten |
| `getWindowInstance()` | Native window handle |
| `getNativeDisplayHandle()` | Native display (`Display*` / `wl_display*`, else `nullptr`) |
| `getVulkanExtensions()` | Required Vulkan instance extensions |
| `getGLProcAddress(name)` | Load an OpenGL function (OpenGL mode) |
| `lockFramebuffer()` / `presentFramebuffer()` | Software (CPU) rendering |
| `getKeyboardListener()` / `getMouseListener()` | Input |
| `getWindowFlags()` | Runtime state (`resized`, `focused`, `deltaTime`, `fps`, …) |

### Graphics-API support per backend

| Backend | Software (CPU) | OpenGL | Vulkan | Platforms |
|---------|:--------------:|:------:|:------:|-----------|
| **SDL3** | ✅ surface | ✅ context + swap | ✅ | Desktop, Android, WASM |
| **GLFW** | ❌ throws² | ✅ context + `getProcAddress` | ✅ | Desktop |
| **X11** | ✅ XImage | ✅ GLX¹ | ✅ (`xlib_surface`) | Linux |
| **Wayland** | ✅ `wl_shm` | ✅ EGL¹ | ✅ (`wayland_surface`) | Linux |

¹ Requires GLX (X11) / EGL + `wayland-egl` (Wayland) at build time; otherwise
OpenGL on that backend reports as unsupported and the rest still builds.

² GLFW has no per-OS software-blit path; `createWindow()` throws
`GraphicsException` immediately for `GraphicsAPI::CPU` instead of opening a
window nothing can draw into. Use SDL3/X11/Wayland for CPU rendering, or
OpenGL/Vulkan with GLFW.

### Parallel software rendering — `wma::parallelFill()`

Writing every pixel of a `GraphicsAPI::CPU` framebuffer on a single thread wastes
the rest of the machine. `parallelFill()` splits the frame into one contiguous
row-band per hardware thread and runs them on a process-wide worker pool shared
by every window — the CPU-rendering equivalent of a GPU spreading pixel work
across its cores.

```cpp
wma::SoftwareFramebuffer fb = window->lockFramebuffer();
if (fb.valid()) {
    wma::parallelFill(fb, [](i32 x, i32 y) -> u32 {
        return /* packed 32-bit pixel for (x, y) */;
    });
    window->presentFramebuffer();
}
```

`pixel(x, y)` may run concurrently on different worker threads for different
rows, so it must not touch shared mutable state; `parallelFill()` blocks until
the whole frame is written, so the framebuffer is ready to present as soon as it
returns. `softwareRenderWorkerCount()` reports how many row-bands it uses (the
machine's `hardware_concurrency()`, clamped to at least 1).

### Input contexts

```cpp
auto& kb = window->getKeyboardListener();
const auto gameplay = kb.createContext();
const auto menu     = kb.createContext();

kb.addKeyAction(wma::Key::KEY_D, wma::KeyAction{[]{ /* move right */ }}, gameplay);
kb.addKeyAction(wma::Key::KEY_D, wma::KeyAction{[]{ /* menu select */ }}, menu);
kb.setActiveContext(gameplay);      // flat switch
kb.pushContext(menu);               // stack overlay (wins until popContext())
```

## 🏗️ Design principles

1. **RAII** with reference-counted subsystems (safe multi-window & move semantics)
2. **Type safety** — scoped enums; strong types throughout
3. **Modularity** — every backend and OpenGL/EGL path is optional
4. **Performance** — zero-cost input fast path, careful frame pacing
5. **Portability first** — the public header never leaks backend SDK headers

## 🐛 Contributing

1. Fork and branch (`git checkout -b feature/amazing-feature`)
2. Commit your changes
3. Open a Pull Request

## 📄 License

See [LICENSE](LICENSE).

---

Made with ❤️ by the Aura3D team
