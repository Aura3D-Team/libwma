# Building

All builds run inside the `vulkan-dev` container; the host has no SDKs.

```sh
docker exec vulkan-dev bash -lc '
  cd /home/developer/workspace/libwma &&
  cmake --preset linux-debug &&
  cmake --build build/linux-debug -j8'
```

## Presets

`linux-debug` · `linux-release` · `linux-dist` · `windows-debug` · `windows-release` ·
`macos-debug` · `macos-release` · `ios` · `android` · `wasm`

## Options

| Option | Default | |
|---|---|---|
| `WMA_BUILD_TESTS` | `OFF` | `ctest` suite |
| `WMA_BUILD_EXAMPLES` | | `examples/basic_window` |
| `WMA_NATIVE_OPTIMIZE` | `OFF` | `-march=native`; machine-local builds only |
| `WMA_CPU_LEVEL` | empty | x86-64 level for shipped builds, e.g. `x86-64-v2` |

Backends are auto-detected and forced off where they cannot exist —
Android/WASM are SDL3-only. Check what you got with `WMA_HAS_GLFW`, `WMA_HAS_SDL`,
`WMA_HAS_X11`, `WMA_HAS_WAYLAND` from the generated `<wma/core/BuildConfig.hpp>`.

## Tests

```sh
cmake --preset linux-debug -DWMA_BUILD_TESTS=ON
cmake --build build/linux-debug -j8
ctest --test-dir build/linux-debug --output-on-failure
```

`wma_test_mix_callback_slot` is threaded — run it under `-fsanitize=thread` when
touching audio.
