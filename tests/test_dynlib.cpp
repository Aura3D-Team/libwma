/*
 * The run-time binding the Linux backends rely on: a missing library or symbol must be an answer, never a crash
 * or a half-filled table, and backend selection must fall through to something that can run.
 */

#include <array>
#include <cstdio>

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

    for (const wma::WindowBackend backend :
         {wma::WindowBackend::GLFW, wma::WindowBackend::SDL3, wma::WindowBackend::X11, wma::WindowBackend::WAYLAND})
    {
        // Whatever is asked for, the answer is something that can run here.
        check(wma::isBackendAvailable(wma::selectUsableBackend(backend)),
              "selectUsableBackend returns a usable backend");
        if (wma::isBackendAvailable(backend))
            check(wma::selectUsableBackend(backend) == backend, "a usable preference is kept");
    }

    return failures == 0 ? 0 : 1;
}
