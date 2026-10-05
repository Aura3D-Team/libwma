#ifdef WMA_ENABLE_GLFW
#include "backends/glfw/GlfwNative.hpp"

#include <GLFW/glfw3.h>

//! glfwGetPlatform() is GLFW 3.4; an older GLFW keeps its own frame instead.
#if defined(WMA_ENABLE_X11) && (GLFW_VERSION_MAJOR > 3 || (GLFW_VERSION_MAJOR == 3 && GLFW_VERSION_MINOR >= 4))
#define WMA_GLFW_MOVE_RESIZE 1
#endif

#ifdef WMA_GLFW_MOVE_RESIZE
#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3native.h>

#include "backends/x11/X11MoveResize.hpp"
#endif

namespace wma::glfw
{

bool supportsMoveResize() noexcept
{
#ifdef WMA_GLFW_MOVE_RESIZE
    return glfwGetPlatform() == GLFW_PLATFORM_X11;
#else
    return false;
#endif
}

bool startMoveResize(GLFWwindow *window, WindowHit hit) noexcept
{
#ifdef WMA_GLFW_MOVE_RESIZE
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
