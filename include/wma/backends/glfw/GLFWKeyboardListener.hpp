#ifndef WMA_BACKENDS_GLFW_KEYBOARD_LISTENER_HPP
#define WMA_BACKENDS_GLFW_KEYBOARD_LISTENER_HPP

#include "wma/input/keyboard/KeyboardListener.hpp"

struct GLFWwindow;

namespace wma
{

class GLFWKeyboardListener : public KeyboardListener
{
  public:
    GLFWKeyboardListener();
    ~GLFWKeyboardListener() override;

    void initialize(GLFWwindow *window);
    void handleKeyEvent(i32 key, i32 action);

    //! GLFW hands over an already-decoded Unicode scalar value, so unlike the
    //! other three backends this one needs no UTF-8 decoding step.
    void handleCharEvent(u32 codepoint);

    //! No-op beyond bookkeeping: GLFW delivers characters unconditionally and
    //! has no on-screen keyboard to raise. Present so callers can enable text
    //! input uniformly across backends.
    void setTextInputEnabled(bool enabled) noexcept
    {
        textInputEnabled_ = enabled;
    }
    [[nodiscard]] bool isTextInputEnabled() const noexcept
    {
        return textInputEnabled_;
    }

    static void glfwKeyCallback(GLFWwindow *window, i32 key, i32 scancode, i32 action, i32 mods);
    static void glfwCharCallback(GLFWwindow *window, u32 codepoint);

  private:
    GLFWwindow *glfwWindow_ = nullptr;
    bool textInputEnabled_ = false;
    static GLFWKeyboardListener *getInstanceFromWindow(GLFWwindow *window);
};

} // namespace wma

#endif // WMA_BACKENDS_GLFW_KEYBOARD_LISTENER_HPP
