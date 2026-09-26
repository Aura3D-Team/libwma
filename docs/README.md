# wma docs

Platform windowing, input and audio. C++23, static library.

| | |
|---|---|
| [building.md](building.md) | Build it, and the container/preset targets. |
| [code-quality.md](code-quality.md) | C++ formatting and static analysis. |
| [windowing.md](windowing.md) | Link it, open a window, run a frame loop. |
| [input.md](input.md) | Keyboard, mouse, text input. |
| [audio.md](audio.md) | Open a device, feed it samples. |

Everything public is behind `#include <wma/wma.hpp>` and lives in `namespace wma`.
Backend headers (`SdlWindowManager.hpp`, …) are not in that umbrella on purpose —
they drag in SDK headers. Use the factories instead.
