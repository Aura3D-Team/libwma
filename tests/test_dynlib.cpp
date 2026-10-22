/*
 * The run-time binding the Linux backends rely on: a missing library or symbol must be an answer, never a crash
 * or a half-filled table, and backend selection must fall through to something that can run.
 */

#include <array>
#include <cstdio>
#include <cstdlib>

#include "platform/DynLib.hpp"
#include "wma/wma.hpp"

namespace
{

int failures = 0;

void check(bool condition, const char *description)
{
    std::printf("[%s] %s\n", condition ? "PASS" : "FAIL", description);
    if (!condition)
        ++failures;
}

} // namespace

int main()
{
    constexpr std::array<const char *, 1> missing{"libwma-no-such-library.so.9"};
    check(wma::dynlib::open(missing) == nullptr, "a library that is not installed opens as nullptr");

    constexpr std::array<const char *, 2> fallback{"libwma-no-such-library.so.9", "libm.so.6"};
    void *libm = wma::dynlib::open(fallback);
    check(libm != nullptr, "the first soname that loads wins");

    void *cosine = nullptr;
    void *sine = nullptr;
    const std::array<wma::dynlib::Binding, 2> good{{{"cos", &cosine}, {"sin", &sine}}};
    check(wma::dynlib::bind(libm, good) && cosine && sine, "every symbol present: bound");

    const std::array<wma::dynlib::Binding, 2> partial{{{"cos", &cosine}, {"wma_no_such_symbol", &sine}}};
    check(!wma::dynlib::bind(libm, partial), "one symbol missing: bind fails");
    check(cosine == nullptr && sine == nullptr, "one symbol missing: no slot is left half-filled");

    check(!wma::dynlib::bind(nullptr, good) && !cosine && !sine, "no library: bind fails and clears the slots");
    check(wma::dynlib::symbol(nullptr, "cos") == nullptr, "symbol() on no library is nullptr");

    // The session is pinned both ways, so the answer does not depend on the machine running the test.
    constexpr std::array<wma::WindowBackend, 4> backends{wma::WindowBackend::GLFW, wma::WindowBackend::SDL3,
                                                         wma::WindowBackend::X11, wma::WindowBackend::WAYLAND};
    setenv("DISPLAY", ":wma-test", 1);
    setenv("WAYLAND_DISPLAY", "wma-test", 1);
    for (const wma::WindowBackend backend : backends)
    {
        check(wma::isBackendAvailable(wma::selectUsableBackend(backend)),
              "with a display: selectUsableBackend returns a usable backend");
        if (wma::isBackendAvailable(backend))
            check(wma::selectUsableBackend(backend) == backend, "with a display: a usable preference is kept");
    }

    for (const char *name : {"DISPLAY", "WAYLAND_DISPLAY", "WAYLAND_SOCKET", "XDG_RUNTIME_DIR"})
        unsetenv(name);
    for (const wma::WindowBackend backend : {wma::WindowBackend::GLFW, wma::WindowBackend::X11,
                                             wma::WindowBackend::WAYLAND})
    {
        if (wma::isBackendAvailable(wma::WindowBackend::SDL3))
        {
            check(wma::selectUsableBackend(backend) == wma::WindowBackend::SDL3, "headless: falls back to SDL3");
            continue;
        }
        try
        {
            (void)wma::selectUsableBackend(backend);
            check(false, "headless without SDL3: throws");
        }
        catch (const wma::WMAException &)
        {
            check(true, "headless without SDL3: throws");
        }
    }

    return failures == 0 ? 0 : 1;
}
