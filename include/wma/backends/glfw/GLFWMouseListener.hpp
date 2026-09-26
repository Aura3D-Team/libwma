#ifndef WMA_BACKENDS_GLFW_MOUSE_LISTENER_HPP
#define WMA_BACKENDS_GLFW_MOUSE_LISTENER_HPP

#include "wma/input/mouse/MouseListener.hpp"

struct GLFWwindow;

namespace wma
{

class GLFWMouseListener : public MouseListener
{
  public:
    GLFWMouseListener();
    ~GLFWMouseListener() override;

    void initialize(GLFWwindow *window);

    static void glfwMouseButtonCallback(GLFWwindow *window, i32 button, i32 action, i32 mods);
    static void glfwCursorPosCallback(GLFWwindow *window, f64 xpos, f64 ypos);
    static void glfwScrollCallback(GLFWwindow *window, f64 xoffset, f64 yoffset);

  protected:
    void updateCursorState() override;

  private:
    GLFWwindow *glfwWindow_ = nullptr;

    void handleButtonEvent(i32 button, i32 action, i32 mods);
    void handlePositionEvent(f64 xpos, f64 ypos);
    void handleScrollEvent(f64 xoffset, f64 yoffset);

    i32 convertButton(i32 glfwButton) const;
    static GLFWMouseListener *getInstanceFromWindow(GLFWwindow *window);
};

} // namespace wma

#endif // WMA_BACKENDS_GLFW_MOUSE_LISTENER_HPP
