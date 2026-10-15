/*
 * A CPU window that asks for vsync must not run uncapped: process() paces at 60 Hz wherever the present does not
 * wait for the display, and leaves the loop alone where it does.
 *
 * Runs on SDL's dummy video driver, so it needs no display.
 */

#include <chrono>
#include <cstdio>
#include <cstdlib>

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

//! Wall time of @p frames passes through process().
std::chrono::duration<double> timeFrames(bool vsync, bool &presentWaits)
{
    wma::WindowDetails details;
    details.width = 160;
    details.height = 100;
    details.vsync = vsync;
    details.targetFPS = 0;
    auto window = wma::createWindowManager(wma::WindowBackend::SDL3, details, wma::GraphicsAPI::CPU);
    window->createWindow("wma pacing");
    presentWaits = window->presentWaitsForDisplay();

    constexpr int kFrames = 12;
    int frame = 0;
    const auto start = std::chrono::steady_clock::now();
    window->process(
        [&]
        {
            if (++frame >= kFrames)
                window->close();
        });
    return std::chrono::steady_clock::now() - start;
}

} // namespace

int main()
{
    setenv("SDL_VIDEODRIVER", "dummy", 1);

    if (!wma::isBackendAvailable(wma::WindowBackend::SDL3))
    {
        std::puts("SDL3 backend not built; skipped");
        return 77;
    }

    try
    {
        bool waits = false;
        const auto unsynced = timeFrames(false, waits);
        check(!waits, "a window that did not ask for vsync is not held to the display");
        check(unsynced.count() < 0.1, "and runs as fast as the loop does");

        const auto synced = timeFrames(true, waits);
        if (waits)
            std::puts("the surface paces itself; the fallback is not exercised");
        else
            check(synced.count() > 0.15, "vsync the present cannot honour is paced at 60 Hz instead of spinning");
    }
    catch (const std::exception &error)
    {
        std::printf("[FAIL] exception: %s\n", error.what());
        return 1;
    }

    return failures == 0 ? 0 : 1;
}
