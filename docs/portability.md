# Portability

## Run-time libraries

On Linux the X11, Wayland and ALSA backends open their system libraries with
`dlopen()`. The executable links only libc and libm, so it starts on a machine
that has none of them. The `-dev` packages are still needed to **build**.

| Library | If missing |
|---|---|
| `libX11.so.6` | no X11 backend |
| `libXext`, `libXi`, `libXrandr`, `libXpresent` | no MIT-SHM, raw mouse motion, refresh-rate pacing, X Present: the same fallbacks as a server without the extension |
| `libGLX.so.0` / `libGL.so.1` | no OpenGL on X11 |
| `libwayland-client.so.0` | no Wayland backend (needs 1.20+) |
| `libxkbcommon.so.0` | Wayland window works, keyboard stays silent |
| `libwayland-cursor.so.0` | compositor's default cursor only |
| `libEGL.so.1`, `libwayland-egl.so.1` | no OpenGL on Wayland |
| `libasound.so.2` | `openAudioDevice()` moves on to SDL3, then Null |

SDL3 and GLFW bind their own libraries the same way.

Ubuntu 18.04 and 20.04 ship libwayland 1.16 and 1.18, so the Wayland backend is unavailable there and `selectUsableBackend()` picks SDL3 or X11.

## Choosing a backend

```cpp
auto backend = wma::selectUsableBackend(wma::WindowBackend::WAYLAND);  // SDL3, Wayland, X11, GLFW if not
auto window  = wma::createWindowManager(backend, config, wma::GraphicsAPI::CPU);
```

A backend qualifies when it is compiled in, its libraries load (`isBackendAvailable()`), and the session has a
display server it can use:

| Backend | Display server |
|---|---|
| Wayland | `$WAYLAND_SOCKET`, `$WAYLAND_DISPLAY`, or `wayland-0` in `$XDG_RUNTIME_DIR` |
| X11 | `$DISPLAY` |
| GLFW | either of the above |
| SDL3 | always: it also runs on a console (KMSDRM) |

With no display at all the answer is SDL3, which then needs `SDL_VIDEO_DRIVER=dummy`; it never picks that driver
by itself. Without SDL3 compiled in, `selectUsableBackend()` throws.

## Shipping a binary

| Build | |
|---|---|
| glibc | a binary needs the glibc it was linked against: build in the oldest system you support |
| CPU | `-DWMA_CPU_LEVEL=x86-64-v2` (or `-v3`); never `-DWMA_NATIVE_OPTIMIZE=ON` |
| preset | `linux-dist` |

Aura3D's `ci/dist/Dockerfile` is such a system, and `ci/check-portability.py` fails a
binary that needs more.
