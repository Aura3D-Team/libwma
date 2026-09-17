# Changelog

All notable changes to libwma are documented in this file.

## [0.3.1]

### Fixed

- **Wayland: the compositor's keymap decides which key an event names.** The backend read the evdev keycode as a fixed QWERTY position, so it was correct only on a `us` layout: Dvorak, AZERTY and Colemak all reported the wrong key, and a virtual keyboard -- which assigns whatever keysyms it likes to whatever keycodes it likes -- produced nonsense. Anything typed through `wtype` began with keycode 1, which the position table calls Escape, so the first keystroke into a UI dismissed whatever had focus. Keys now resolve through `xkb_state_key_get_one_sym()` and the keysym table (`mapWaylandKeysym()`, the same mapping `mapX11Key()` uses, since xkbcommon reuses X11's keysym values). The position table is kept as `mapWaylandKey()` for the window before the compositor has sent a keymap, and for a keysym the table does not name -- a caller binding physical keys is better served by a position than by `KEY_UNKNOWN`
- `wma_test_wayland_keymap` covers it: xkbcommon compiles a keymap from a string, so the whole path is testable with no compositor and no seat

## [0.3.0]

### Added

- **Wayland: application-owned surface roles.** `createWaylandWindowManager(details, api, role)` takes an optional `WaylandSurfaceRole` -- a virtual interface a consumer implements to claim a fresh, uncommitted `wl_surface` for something other than an `xdg_toplevel`. libwma keeps the connection, the surface, seat input, event dispatch and native rendering handles; the role owns its own protocol objects, configure acknowledgement, logical dimensions, scale and close state (`attach()`/`rebind()`/`configured()`/`shouldClose()`), so a consumer such as libaurashell can build layer-shell surfaces (panels, overlays, lock screens) without libwma linking layer-shell protocol headers itself. `createWindow()` skips `xdg_wm_base`/`xdg_toplevel` entirely when a role is supplied and blocks on `role->configured()` in its place; that initial-configure wait now also throws a `WindowException` if the surface is asked to close before it ever configures, rather than spinning on `wl_display_dispatch` forever. New `IWindowManager::transparentFramebuffer()` (defaults `false`) and the Wayland backend's `getFramebufferSize()` defer to the role's own `transparentFramebuffer()`/`bufferScale()` -- the latter scales `WindowDetails`' logical width/height, since a role can run fractional-scale bookkeeping the base manager has no visibility into. Custom roles currently support `GraphicsAPI::Vulkan` only; any other API throws a `GraphicsException` at `createWindow()` rather than opening a surface nothing can draw into. Aura3D's `Engine` takes the factory through its own `(config, configPath, factory)` constructor, keeping layer-shell policy a consumer concern. Documented in `docs/wayland-surface-roles.md`

## [0.2.1]

### Changed

- **Breaking: `<wma/core/BuildConfig.hpp>` is gone.** `WMA_HAS_*` and
  `WMA_VERSION_*` are now PUBLIC compile definitions on `wma::wma` instead of
  a CMake-generated header. The header could not survive the single-prefix
  install below: one `include/` tree serves every platform, so a generated
  header could only hold one platform's answers -- a Linux consumer would
  have read `WMA_HAS_X11` off whichever build installed last. Delete the
  include; the macros arrive from the target, spelled and valued exactly as
  before (`#if WMA_HAS_X11` still works, all flags defined as 0 or 1).
  `<wma/wma.hpp>` now `#error`s if they are absent, so including it without
  linking `wma::wma` fails loudly instead of silently reporting every backend
  as unavailable
- Every platform now installs into **one prefix** instead of a per-platform
  directory: headers once in `<prefix>/include`, and the library ABI-tagged as
  `libwma_linux_x86_64.a`, `libwma_android_arm64_v8a.a`, `libwma_wasm32.a`.
  Each build contributes `wma-targets-<tag>.cmake` plus its own
  `wma-deps-<tag>.cmake` -- the per-platform `find_dependency()` calls had to
  move out of `wmaConfig.cmake` for the same reason as the header, since a
  shared config would carry whichever build ran last and a Linux consumer
  would skip `find_dependency(X11)`. The version file is `ARCH_INDEPENDENT`

### Fixed

- `AlsaAudioDevice`: periodic audible pops on a CPU-contended machine. The writer thread ran at ordinary priority with no core of its own, so a busy render thread could starve it past its ~21ms period; the underrun that followed was recovered (`snd_pcm_recover`) but still audible, and silently so — the recovery call passes ALSA's own `silent` flag, so nothing showed up in the log either. Three changes: `kPeriodsOfLatency` raised from 2 to 3, so a missed wakeup is absorbed as latency instead of a dropout; the writer thread now requests `SCHED_FIFO` (best-effort — silently a no-op without `CAP_SYS_NICE`, the common case in a container); and it pins itself to the last CPU via `pthread_setaffinity_np` (skipped below two cores) so unrelated CPU-bound work is scheduled onto a *different* core rather than merely outranked on the same one

## [0.2.0]

### Added
- Audio output (`wma::IAudioDevice`), a peer of `IWindowManager`: it owns the platform device and the thread that drives it, and hands the consumer one job — fill a buffer of interleaved floats through `setMixCallback()`. Decoding, mixing and 3D panning are platform-independent maths and deliberately stay above this layer (in Aura3D's `AudioEngine`). Three backends: `AlsaAudioDevice` (native Linux via `libasound`, lowest latency, gated by the new `WMA_ENABLE_ALSA` and forced off on Android/WASM/Windows/Apple), `SDLAudioDevice` (the universal path, and the only one on Android/WASM/Apple where SDL3 *is* the native route to AAudio/OpenSL ES/Web Audio/CoreAudio; uses `SDL_InitSubSystem` so it coexists with a window manager's video subsystem), and `NullAudioDevice` (always compiled, never fails to open, and exposes `renderFrames()` so tests can drive a mixer with no device and no clock — the audio analogue of the inert `TouchListener`). `openAudioDevice()` degrades through the chain until one opens, ending at Null, so a machine with no sound hardware yields a silent device rather than an error; `createAudioDevice()`/`getDefaultAudioBackend()`/`isAudioBackendAvailable()`/`audioBackendName()` mirror the windowing factory. **`AudioBackend` is a separate axis from `WindowBackend`** — GLFW/X11/Wayland are display protocols with no audio API of their own, so any window backend pairs with any audio backend, and audio backends accordingly live in their own `audio/backends/<name>/` tree rather than under the windowing one
- Apple support (macOS & iOS): `GraphicsAPI::Metal` opens a window whose drawing surface is a `CAMetalLayer`, reached through the new `IWindowManager::getMetalLayer()`; SDL3 supplies it via `SDL_Metal_CreateView` (macOS and iOS), GLFW via `src/platform/apple/AppleMetalLayer.mm`, which hosts a layer in the window's `NSView` (macOS only — GLFW has no iOS port). `cmake/Platform.cmake` forces `WMA_ENABLE_X11`/`WMA_ENABLE_WAYLAND` off on Apple (neither can host a Metal layer) and GLFW off on iOS; `cmake/Dependencies.cmake` resolves `ink` from a `macos/debug|release` or `ios` prefix; `WMA_HAS_METAL` in `BuildConfig.hpp` reports whether a build can honour the request at all; new `macos-debug`/`macos-release`/`ios` CMake presets and `macos-build`/`ios-build` CI jobs. X11, Wayland and non-Apple SDL3/GLFW builds reject the request with a `GraphicsException` naming the reason rather than opening a window nothing can draw into. This extends 0.1.0's graphics-API support (OpenGL, Vulkan, software/CPU) with Metal, and its cross-platform support (Linux, Windows, macOS, Android, WebAssembly) with iOS
- Text input (`KeyboardListener::setTextInputAction()`), delivering the *characters* a keystroke produced rather than the physical key that moved — which is the only thing a text field can correctly consume, since Shift+2 is `@` or `"` depending on layout and a compose sequence is several keys and one character. Implemented on all four backends: SDL3 (`SDL_EVENT_TEXT_INPUT`), GLFW (`glfwSetCharCallback`), X11 (`Xutf8LookupString` against a per-window XIM/XIC, falling back to `XLookupString` where no input method is running) and Wayland (xkbcommon built from the compositor's keymap — which the backend previously discarded outright, `close(fd)` — plus `xkb_compose_state` so dead keys and Compose work). Characters arrive as decoded `wma::Codepoint` scalar values, not borrowed byte buffers, so a callback never has to reason about the lifetime of the platform's string; the three backends that hand over UTF-8 decode through the new `wma::utf8::decode()`, which drops malformed bytes while preserving the well-formed characters around them (covered by the new `tests/test_utf8.cpp`, and `enable_testing()` now actually registers the suite with CTest)
- `IWindowManager::setTextInputEnabled()`/`isTextInputEnabled()`: gates the platform's text machinery, and on Android/iOS is what raises and dismisses the on-screen keyboard — hence a request rather than always-on, since a soft keyboard must not cover the screen until a field wants typing. Backends with nothing to gate record the request and deliver text regardless, so a caller behaves identically everywhere
- `KeyboardListener::setKeyEventAction()`: one subscription to *every* key, alongside (not instead of) the per-key binding table. An overlay needs Tab, the arrows, Home/End, Backspace, Delete, Enter and Escape at once, and claiming a binding slot for each would evict whatever the application bound to them; both fire. Events carry `KeyState` (`Pressed`/`Repeat`/`Released`) and `KeyModifiers` — auto-repeat is reported to this stream but deliberately *not* to the binding table, so a held key keeps a text field deleting without re-firing a bound jump. `isKeyDown()`, `modifiers()` and `releaseAllKeys()` round it out; modifiers are derived from the tracked press/release stream rather than each backend's own mask, which the four disagree on in both layout and sampling time. X11 enables detectable auto-repeat so a held key stops arriving as synthetic release/press pairs, and focus loss clears held keys on SDL/X11/Wayland so a modifier held through an Alt-Tab does not stick
- `MouseListener::consumeScrollDelta()`: polls accumulated wheel movement and resets it. A context holds exactly one scroll action, so a second subscriber would displace the first — this is the non-displacing route an overlay needs, mirroring what `getCurrentPosition()` already provides for cursor motion, and it accumulates so a fast flick reads as many notches rather than one
- `IWindowManager::getFramebufferSize()`: the window's drawable size in *pixels*, as opposed to `WindowDetails`' logical width/height — the two differ by the backing scale factor, and a renderer that sizes its surface from the logical value draws a quarter of a Retina window stretched over the whole of it. SDL3 and GLFW override it (`SDL_GetWindowSizeInPixels`/`glfwGetFramebufferSize`); X11 and Wayland inherit a default that returns the logical size, which is correct where there is no separate backing-store scaling

### Changed
- Dropped the explicit `ink::threading` link: `ThreadPool`/`WorkerThread` are back in `ink::ink` as of ink 0.3.0, so linking `ink::ink` is sufficient again on every platform. The `if(NOT EMSCRIPTEN)` guard around that link is gone with it — ink now carries `-pthread` on Emscripten unconditionally, so **a page serving a WASM build of wma must send COOP/COEP** (`Cross-Origin-Opener-Policy: same-origin`, `Cross-Origin-Embedder-Policy: require-corp`) or the module will not instantiate. Requires ink >= 0.3.0; `ink::threading` no longer exists to link against

## [0.1.0]

### Added
- Multiple window-management backends: GLFW, SDL3, X11, Wayland
- Multiple graphics API support: OpenGL, Vulkan, and software (CPU) rendering
- Cross-platform support: Linux, Windows, macOS, Android (SDL3) and WebAssembly (SDL3)
- Parallel software rendering (`parallelFill()`) spreading CPU pixel work across hardware threads, falling back to a single-threaded row dispatch on WebAssembly (so the WASM build needs neither `ink::threading` nor `-pthread`/SharedArrayBuffer/COOP-COEP)
- Keyboard, mouse and touch input listeners with zero-alloc, layered input contexts
- Wayland server-side decorations via `xdg-decoration-unstable-v1`
- CMake presets for desktop (Linux/Windows debug & release), Android and WASM builds
- `find_package(wma)` CMake package config with runtime/compile-time backend querying (`BuildConfig.hpp`), linking `ink::ink` and `ink::threading` explicitly
- Windows support: `cmake/Platform.cmake` sets `NOMINMAX`/`WIN32_LEAN_AND_MEAN` and, under MSVC, `/EHsc`/`/utf-8`/`/Zc:__cplusplus`; `cmake/Dependencies.cmake` forces `WMA_ENABLE_X11`/`WMA_ENABLE_WAYLAND` off there (only SDL3/GLFW exist on Windows); the `windows-debug`/`windows-release` CMake presets enable `WMA_ENABLE_SDL`/`WMA_ENABLE_GLFW`
- Release pipeline: tagging `vX.Y.Z` runs `.github/workflows/release.yml`, packaging `libwma.a`/`wma.lib` (+ public headers + CMake package config) for Linux, Android, WebAssembly and Windows, attaching one archive per platform to a draft GitHub Release
- Windows CI: a `windows-build` job (`.github/workflows/ci.yml`) builds wma on `windows-latest` via Ninja + `ilammy/msvc-dev-cmd`/`seanmiddleditch/gha-setup-ninja`, building its `ink` dependency from source since the `vulkan-dev` container image (used for Linux/Android/WASM) has no Windows equivalent
