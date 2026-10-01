#ifdef WMA_ENABLE_GLFW
#include "wma/backends/glfw/GLFWMouseListener.hpp"
#include "wma/backends/glfw/GlfwWindowManager.hpp"
#include "wma/core/Types.hpp"
#include "wma/exceptions/WMAException.hpp"

#include <GLFW/glfw3.h>
#include <utility>

namespace wma
{
namespace
{

[[nodiscard]] int toStandardCursor(SystemCursor shape) noexcept
{
    switch (shape)
    {
    case SystemCursor::NsResize:
        return GLFW_VRESIZE_CURSOR;
    case SystemCursor::EwResize:
        return GLFW_HRESIZE_CURSOR;
//! Diagonal shapes arrived in GLFW 3.4; Emscripten's port predates them.
#ifdef GLFW_RESIZE_NWSE_CURSOR
    case SystemCursor::NwseResize:
        return GLFW_RESIZE_NWSE_CURSOR;
    case SystemCursor::NeswResize:
        return GLFW_RESIZE_NESW_CURSOR;
#else
    case SystemCursor::NwseResize:
    case SystemCursor::NeswResize:
#endif
    case SystemCursor::Default:
        break;
    }
    return GLFW_ARROW_CURSOR;
}

} // namespace

GLFWMouseListener::GLFWMouseListener() : MouseListener(), glfwWindow_(nullptr)
{
}

GLFWMouseListener::~GLFWMouseListener()
{
    if (glfwWindow_)
    {
        glfwSetMouseButtonCallback(glfwWindow_, nullptr);
        glfwSetCursorPosCallback(glfwWindow_, nullptr);
        glfwSetScrollCallback(glfwWindow_, nullptr);
    }
}

void GLFWMouseListener::initialize(GLFWwindow *window)
{
    if (!window)
    {
        throw InputException("Invalid GLFW window pointer");
    }
    glfwWindow_ = window;
    f64 xpos, ypos;
    glfwGetCursorPos(window, &xpos, &ypos);
    currentPosition_ = WMAMousePosition(xpos, ypos);
    lastPosition_ = currentPosition_;
    firstMouse_ = true;
    updateCursorState();
    glfwSetMouseButtonCallback(window, glfwMouseButtonCallback);
    glfwSetCursorPosCallback(window, glfwCursorPosCallback);
    glfwSetScrollCallback(window, glfwScrollCallback);
}

void GLFWMouseListener::handleButtonEvent(i32 button, i32 action, i32 mods)
{
    (void)mods;
    const i32 unifiedButton = convertButton(button);
    const bool primary = button == GLFW_MOUSE_BUTTON_LEFT;
    if (action == GLFW_PRESS)
    {
        if (primary && owner() && owner()->claimPress())
        {
            pressClaimed_ = true;
            return;
        }
        dispatchButtonPress(unifiedButton);
    }
    else if (action == GLFW_RELEASE)
    {
        if (primary && std::exchange(pressClaimed_, false))
            return;
        dispatchButtonRelease(unifiedButton);
    }
}

GlfwWindowManager *GLFWMouseListener::owner() const noexcept
{
    const auto *userData = glfwWindow_ ? static_cast<GlfwUserData *>(glfwGetWindowUserPointer(glfwWindow_)) : nullptr;
    return userData ? userData->windowManager : nullptr;
}

void GLFWMouseListener::handlePositionEvent(f64 xpos, f64 ypos)
{
    if (firstMouse_)
    {
        lastPosition_ = WMAMousePosition(xpos, ypos);
        firstMouse_ = false;
    }
    f64 deltaX = (xpos - lastPosition_.x) * sensitivity_;
    f64 deltaY = (lastPosition_.y - ypos) * sensitivity_;
    currentPosition_ = WMAMousePosition(xpos, ypos, deltaX, deltaY);
    dispatchMove(currentPosition_);
    lastPosition_ = WMAMousePosition(xpos, ypos);
    setHitCursor(owner() ? owner()->hoverCursor(xpos, ypos) : SystemCursor::Default);
}

void GLFWMouseListener::handleScrollEvent(f64 xoffset, f64 yoffset)
{
    dispatchScroll(WMAMouseScroll(xoffset, yoffset));
}

void GLFWMouseListener::glfwMouseButtonCallback(GLFWwindow *window, i32 button, i32 action, i32 mods)
{
    auto *listener = getInstanceFromWindow(window);
    if (listener)
        listener->handleButtonEvent(button, action, mods);
}

void GLFWMouseListener::glfwCursorPosCallback(GLFWwindow *window, f64 xpos, f64 ypos)
{
    auto *listener = getInstanceFromWindow(window);
    if (listener)
        listener->handlePositionEvent(xpos, ypos);
}

void GLFWMouseListener::glfwScrollCallback(GLFWwindow *window, f64 xoffset, f64 yoffset)
{
    auto *listener = getInstanceFromWindow(window);
    if (listener)
        listener->handleScrollEvent(xoffset, yoffset);
}

GLFWMouseListener *GLFWMouseListener::getInstanceFromWindow(GLFWwindow *window)
{
    if (!window)
        return nullptr;
    auto *userData = static_cast<GlfwUserData *>(glfwGetWindowUserPointer(window));
    if (!userData || !userData->mouseListener)
        return nullptr;
    return static_cast<GLFWMouseListener *>(userData->mouseListener);
}

void GLFWMouseListener::releaseCursors() noexcept
{
    for (GLFWcursor *&cursor : cursors_)
    {
        if (cursor)
            glfwDestroyCursor(cursor);
        cursor = nullptr;
    }
}

void GLFWMouseListener::updateCursorState()
{
    if (!glfwWindow_)
        return;

    glfwSetInputMode(glfwWindow_, GLFW_CURSOR, cursorEnabled_ ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
    if (!cursorEnabled_)
        return;

    const SystemCursor shape = effectiveSystemCursor();
    GLFWcursor *&cursor = cursors_[static_cast<usize>(shape)];
    if (!cursor && shape != SystemCursor::Default)
        cursor = glfwCreateStandardCursor(toStandardCursor(shape));
    //! Null restores the arrow, and is also what Emscripten's GLFW returns.
    glfwSetCursor(glfwWindow_, cursor);
}

i32 GLFWMouseListener::convertButton(i32 glfwButton) const
{
    switch (glfwButton)
    {
    case GLFW_MOUSE_BUTTON_LEFT:
        return MouseButton::WMALeft;
    case GLFW_MOUSE_BUTTON_RIGHT:
        return MouseButton::WMARight;
    case GLFW_MOUSE_BUTTON_MIDDLE:
        return MouseButton::WMAMiddle;
    case GLFW_MOUSE_BUTTON_4:
        return MouseButton::WMAButton4;
    case GLFW_MOUSE_BUTTON_5:
        return MouseButton::WMAButton5;
    case GLFW_MOUSE_BUTTON_6:
        return MouseButton::WMAButton6;
    case GLFW_MOUSE_BUTTON_7:
        return MouseButton::WMAButton7;
    case GLFW_MOUSE_BUTTON_8:
        return MouseButton::WMAButton8;
    default:
        return glfwButton;
    }
}

} // namespace wma
#endif
