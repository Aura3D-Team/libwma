#ifndef WMA_BACKENDS_GLFW_MOUSE_LISTENER_HPP
#define WMA_BACKENDS_GLFW_MOUSE_LISTENER_HPP

#include <array>

#include "wma/input/mouse/MouseListener.hpp"

struct GLFWcursor;
struct GLFWwindow;

namespace wma
{

class GLFWMouseListener : public MouseListener
{
  public:
    GLFWMouseListener();
    ~GLFWMouseListener() override;

    void initialize(GLFWwindow *window);
    //! glfwTerminate() frees every cursor, so the window manager calls this first.
    void releaseCursors() noexcept;

    static void glfwMouseButtonCallback(GLFWwindow *window, i32 button, i32 action, i32 mods);
    static void glfwCursorPosCallback(GLFWwindow *window, f64 xpos, f64 ypos);
    static void glfwScrollCallback(GLFWwindow *window, f64 xoffset, f64 yoffset);

  protected:
    void updateCursorState() override;

  private:
    GLFWwindow *glfwWindow_ = nullptr;
    std::array<GLFWcursor *, SYSTEM_CURSOR_COUNT> cursors_{};
    //! The release of a press handed to the window manager goes to its grab too.
    bool pressClaimed_ = false;

    void handleButtonEvent(i32 button, i32 action, i32 mods);
    void handlePositionEvent(f64 xpos, f64 ypos);
    void handleScrollEvent(f64 xoffset, f64 yoffset);

    i32 convertButton(i32 glfwButton) const;
    [[nodiscard]] class GlfwWindowManager *owner() const noexcept;
    static GLFWMouseListener *getInstanceFromWindow(GLFWwindow *window);
};

} // namespace wma

#endif // WMA_BACKENDS_GLFW_MOUSE_LISTENER_HPP
