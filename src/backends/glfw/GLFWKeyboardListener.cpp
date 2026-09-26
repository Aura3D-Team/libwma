#ifdef WMA_ENABLE_GLFW
#include "wma/backends/glfw/GLFWKeyboardListener.hpp"
#include "wma/backends/glfw/GlfwWindowManager.hpp"
#include "wma/exceptions/WMAException.hpp"
#include "wma/input/keyboard/Keys.h"

#include <GLFW/glfw3.h>

namespace wma
{

GLFWKeyboardListener::GLFWKeyboardListener() : KeyboardListener(), glfwWindow_(nullptr)
{
}

GLFWKeyboardListener::~GLFWKeyboardListener()
{
    if (glfwWindow_)
    {
        glfwSetKeyCallback(glfwWindow_, nullptr);
        glfwSetCharCallback(glfwWindow_, nullptr);
    }
}

void GLFWKeyboardListener::initialize(GLFWwindow *window)
{
    if (!window)
    {
        throw InputException("Invalid GLFW window pointer");
    }
    glfwWindow_ = window;
    auto *userData = static_cast<GlfwUserData *>(glfwGetWindowUserPointer(window));
    if (userData)
    {
        userData->keyboardListener = this;
    }
    glfwSetKeyCallback(window, glfwKeyCallback);
    glfwSetCharCallback(window, glfwCharCallback);
}

void GLFWKeyboardListener::handleKeyEvent(i32 key, i32 action)
{
    const Key mappedKey = mapGLFWKey(key);
    if (action == GLFW_PRESS)
    {
        dispatchKeyPress(mappedKey, /*repeat=*/false);
    }
    else if (action == GLFW_REPEAT)
    {
        dispatchKeyPress(mappedKey, /*repeat=*/true);
    }
    else if (action == GLFW_RELEASE)
    {
        dispatchKeyRelease(mappedKey);
    }
}

void GLFWKeyboardListener::handleCharEvent(u32 codepoint)
{
    dispatchText(static_cast<Codepoint>(codepoint));
}

void GLFWKeyboardListener::glfwKeyCallback(GLFWwindow *window, i32 key, i32 scancode, i32 action, i32 mods)
{
    auto *listener = getInstanceFromWindow(window);
    if (listener)
    {
        listener->handleKeyEvent(key, action);
    }
}

void GLFWKeyboardListener::glfwCharCallback(GLFWwindow *window, u32 codepoint)
{
    auto *listener = getInstanceFromWindow(window);
    if (listener)
    {
        listener->handleCharEvent(codepoint);
    }
}

GLFWKeyboardListener *GLFWKeyboardListener::getInstanceFromWindow(GLFWwindow *window)
{
    if (!window)
        return nullptr;
    auto *userData = static_cast<GlfwUserData *>(glfwGetWindowUserPointer(window));
    if (!userData || !userData->keyboardListener)
        return nullptr;
    return static_cast<GLFWKeyboardListener *>(userData->keyboardListener);
}

} // namespace wma
#endif
