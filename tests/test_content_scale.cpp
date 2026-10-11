/*
 * IWindowManager::getContentScale(): 1 unless a backend knows better, and
 * never zero or negative, which a consumer would divide by.
 *
 * The SDL half runs on SDL's dummy video driver, so it needs no display. It
 * cannot see a real scale: the dummy driver's display is always 1x. What it pins is that
 * the override divides the right two numbers and does not turn a 1x display into
 * something else.
 */

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>

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
#if defined(WMA_TEST_HAS_SDL)
    setenv("SDL_VIDEODRIVER", "dummy", 1);

    if (!wma::isBackendAvailable(wma::WindowBackend::SDL3))
    {
        std::printf("SDL3 backend not built\n");
        return 77;
    }

    wma::WindowDetails details;
    details.width = 320;
    details.height = 240;

    std::unique_ptr<wma::IWindowManager> window =
        wma::createWindowManager(wma::WindowBackend::SDL3, details, wma::GraphicsAPI::CPU);
    window->createWindow("content scale");

    const f32 scale = window->getContentScale();
    check(std::isfinite(scale) && scale > 0.0f, "the scale is a positive number");
    check(std::fabs(scale - 1.0f) < 1e-4f, "a 1x display is 1");

    const wma::FramebufferSize framebuffer = window->getFramebufferSize();
    check(framebuffer.width == details.width && framebuffer.height == details.height,
          "and the framebuffer still has the window's size");
#endif

    return failures == 0 ? 0 : 1;
}
