# Third-party licenses

libwma itself is Apache-2.0 ([LICENSE](LICENSE), [NOTICE](NOTICE)). It bundles
no third-party source; the only third-party code that ends up in its archive
is generated at configure time (below).

## Generated into the library

`wayland-scanner` turns these `wayland-protocols` files into
`include/wma/backends/wayland/protocols/*` (installed headers) and the
`*-protocol.c` files compiled into the Wayland backend. The generated files
carry the notice below.

| Protocol | Copyright |
|---|---|
| `xdg-shell` | Kristian Høgsberg 2008-2013; Rafael Antognolli 2013; Jasper St. Pierre 2013; Intel Corporation 2010-2013; Samsung Electronics Co., Ltd 2015-2017; Red Hat Inc. 2015-2017 |
| `xdg-decoration-unstable-v1` | Simon Ser 2018 |
| `pointer-constraints-unstable-v1` | Jonas Ådahl 2014; Red Hat Inc. 2015 |
| `relative-pointer-unstable-v1` | Jonas Ådahl 2014; Red Hat Inc. 2015 |

```text
Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice (including the next
paragraph) shall be included in all copies or substantial portions of the
Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.
```

## Linked, not distributed

Resolved by `find_package` / `pkg_check_modules`; their licenses bind whoever
ships them.

| Component | License | Used by |
|---|---|---|
| SDL3 | zlib | SDL3 backend, SDL3 audio |
| GLFW | zlib | GLFW backend |
| libwayland, libxkbcommon | MIT | Wayland backend |
| Xlib, XRandR, XInput2, XShm, XTest | MIT | X11 backend |
| ALSA (`libasound`) | LGPL-2.1 | ALSA audio. Link it dynamically (the default) so it can be replaced |
| libink | Apache-2.0 | everywhere (same author) |

libdecor (MIT) is only probed for its plugin directory; it is GLFW's runtime
dependency, not wma's.
