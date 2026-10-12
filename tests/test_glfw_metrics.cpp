/*
 * GlfwWindowManager keeps the framebuffer size and content scale GLFW reported instead of asking GLFW again on
 * every call, which is a round trip to the server on X11. What has to hold is that the kept values are GLFW's:
 * at creation, and after a resize.
 *
 * Needs a display. Runs against $WMA_TEST_DISPLAY, a disposable X server, never the desktop's $DISPLAY; skipped
 * without one.
 */

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>

//! The GL entry points are not used here, and GLFW's own GL header would only pull in a library that is not.
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "wma/wma.hpp"

namespace
{

constexpr int kSkipped = 77;

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
    // Never $DISPLAY: this opens a window, which is not for a live desktop.
    const char *testDisplay = std::getenv("WMA_TEST_DISPLAY");
    if (!testDisplay)
    {
        std::puts("WMA_TEST_DISPLAY unset; skipped");
        return kSkipped;
    }
    setenv("DISPLAY", testDisplay, 1);
    unsetenv("WAYLAND_DISPLAY");

    wma::WindowDetails details;
    details.width = 320;
    details.height = 240;

    std::unique_ptr<wma::IWindowManager> window;
    try
    {
        window = wma::createWindowManager(wma::WindowBackend::GLFW, details, wma::GraphicsAPI::OpenGL);
        window->createWindow("wma glfw metrics");
    }
    catch (const std::exception &error)
    {
        std::printf("no GLFW window on this display (%s); skipped\n", error.what());
        return kSkipped;
    }
    for (int i = 0; i < 20; ++i)
        window->waitEvents(5);

    auto *handle = static_cast<GLFWwindow *>(window->getWindowInstance());
    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(handle, &width, &height);

    wma::FramebufferSize size = window->getFramebufferSize();
    check(size.width == width && size.height == height, "the framebuffer size is GLFW's");

    const f32 scale = window->getContentScale();
    check(std::isfinite(scale) && scale > 0.0f, "the content scale is a positive number");
    check(window->getContentScale() == scale, "and the same one until GLFW says it changed");

    // A resize reaches the kept size.
    glfwSetWindowSize(handle, 400, 300);
    for (int i = 0; i < 50 && window->getFramebufferSize().width == size.width; ++i)
        window->waitEvents(10);
    glfwGetFramebufferSize(handle, &width, &height);
    size = window->getFramebufferSize();
    check(size.width == width && size.height == height, "after a resize it is the new size");
    check(size.width != 320, "which is not the old one");

    const f32 resized = window->getContentScale();
    check(std::isfinite(resized) && resized > 0.0f, "and the content scale is still a positive number");

    window->destroy();
    check(window->getFramebufferSize().width >= 1, "a destroyed window still reports a size, never zero");

    return failures == 0 ? 0 : 1;
}
