/*
 * A CPU window that asks for vsync must not run uncapped: process() paces at 60 Hz wherever the present does not
 * block for the display, and leaves the loop alone where it does.
 *
 * The decision is checked as a pure function; one loop on SDL's dummy video driver (no display needed) confirms
 * process() applies it, with margins wide enough for a loaded runner.
 */

#include <chrono>
#include <cstdio>
#include <cstdlib>

#include "wma/core/FramePacing.hpp"
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

wma::WindowDetails details(i32 targetFPS, bool vsync)
{
    wma::WindowDetails result;
    result.targetFPS = targetFPS;
    result.vsync = vsync;
    return result;
}

void testDecision()
{
    using wma::GraphicsAPI;
    using wma::detail::effectiveTargetFPS;
    using wma::detail::kUnpacedVsyncFPS;

    check(effectiveTargetFPS(details(0, true), GraphicsAPI::CPU, false) == kUnpacedVsyncFPS,
          "uncapped CPU vsync the present cannot honour falls back");
    check(effectiveTargetFPS(details(0, true), GraphicsAPI::CPU, true) == 0, "a present that blocks is left alone");
    check(effectiveTargetFPS(details(0, false), GraphicsAPI::CPU, false) == 0, "no vsync asked, no cap");
    check(effectiveTargetFPS(details(0, true), GraphicsAPI::OpenGL, false) == 0, "GPU windows are left alone");
    check(effectiveTargetFPS(details(0, true), GraphicsAPI::CPU, false, 144) == 144,
          "the display's own rate replaces 60 when the backend knows it");
    check(effectiveTargetFPS(details(0, true), GraphicsAPI::CPU, true, 144) == 0,
          "and does not matter where the present waits");
    check(effectiveTargetFPS(details(30, true), GraphicsAPI::CPU, false, 144) == 30, "an explicit rate wins");
    check(effectiveTargetFPS(details(-1, true), GraphicsAPI::CPU, false) == kUnpacedVsyncFPS,
          "a negative rate means uncapped, like zero");
}

//! Wall time of a fixed number of passes through process().
std::chrono::duration<double> timeFrames(bool vsync, bool &presentWaits)
{
    auto window = wma::createWindowManager(wma::WindowBackend::SDL3, wma::WindowDetails{160, 100, true, 0, vsync},
                                           wma::GraphicsAPI::CPU);
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

void testLoop()
{
    bool waits = false;
    const auto unsynced = timeFrames(false, waits);
    check(!waits, "a window that did not ask for vsync is not held to the display");

    const auto synced = timeFrames(true, waits);
    if (waits)
    {
        std::puts("the surface paces itself; the fallback loop is not exercised");
        return;
    }
    // 12 frames at 60 Hz take 200 ms; compared with the same loop unpaced rather than to a fixed budget.
    check(synced.count() > 0.15 && synced > unsynced, "process() applies the 60 Hz cap");
}

} // namespace

int main()
{
    testDecision();

    setenv("SDL_VIDEODRIVER", "dummy", 1);
    if (!wma::isBackendAvailable(wma::WindowBackend::SDL3))
    {
        std::puts("SDL3 backend not built; loop check skipped");
        return failures == 0 ? 0 : 1;
    }

    try
    {
        testLoop();
    }
    catch (const std::exception &error)
    {
        std::printf("[FAIL] exception: %s\n", error.what());
        return 1;
    }

    return failures == 0 ? 0 : 1;
}
