#ifdef WMA_ENABLE_GLFW
#include "backends/glfw/GlfwNative.hpp"

#include <GLFW/glfw3.h>

#ifdef WMA_ENABLE_X11
#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3native.h>

#include "backends/x11/X11MoveResize.hpp"
#endif

namespace wma::glfw
{

bool supportsMoveResize() noexcept
{
#ifdef WMA_ENABLE_X11
    return glfwGetPlatform() == GLFW_PLATFORM_X11;
#else
    return false;
#endif
}

bool startMoveResize(GLFWwindow *window, WindowHit hit) noexcept
{
#ifdef WMA_ENABLE_X11
    if (window && supportsMoveResize())
        return x11::startMoveResize(glfwGetX11Display(), glfwGetX11Window(window), hit);
#else
    (void)window;
    (void)hit;
#endif
    return false;
}

} // namespace wma::glfw
#endif // WMA_ENABLE_GLFW
