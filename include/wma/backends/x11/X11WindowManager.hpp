#ifndef WMA_BACKENDS_X11_WINDOW_MANAGER_HPP
#define WMA_BACKENDS_X11_WINDOW_MANAGER_HPP

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <memory>

#include "X11KeyboardListener.hpp"
#include "X11MouseListener.hpp"
#include "wma/managers/IWindowManager.hpp"

namespace wma
{

class X11WindowManager : public IWindowManager
{
  public:
    explicit X11WindowManager(const WindowDetails &windowDetails, GraphicsAPI graphicsAPI = GraphicsAPI::Vulkan);
    ~X11WindowManager() override;

    X11WindowManager(const X11WindowManager &) = delete;
    X11WindowManager &operator=(const X11WindowManager &) = delete;
    X11WindowManager(X11WindowManager &&) noexcept;
    X11WindowManager &operator=(X11WindowManager &&) noexcept;

    void createWindow(const char *windowName) override;
    void pollEvents() override;
    void waitEvents(int timeoutMs) override;
    void swapBuffers() override;
    void *getWindowInstance() override;
    void *getNativeDisplayHandle() const noexcept override;
    void *getGLProcAddress(const char *name) const override;
    SoftwareFramebuffer lockFramebuffer() override;
    void presentFramebuffer() override;
    WindowFlags *getWindowFlags() noexcept override;
    const WindowDetails *getWindowDetails() noexcept override;
    const std::vector<const char *> getVulkanExtensions() const override;
    KeyboardListener &getKeyboardListener() noexcept override;
    void setTextInputEnabled(bool enabled) noexcept override;
    [[nodiscard]] bool isTextInputEnabled() const noexcept override;
    MouseListener &getMouseListener() noexcept override;
    bool shouldClose() const override;
    WindowBackend getBackendType() const override;
    GraphicsAPI getGraphicsAPI() const override;
    WmaCode destroy() override;

  private:
    Display *display_;
    Window window_;
    Colormap colormap_;
    Atom wmDeleteWindow_;

    //! Software rendering (GraphicsAPI::CPU)
    GC gc_;
    XImage *image_;

    //! OpenGL via GLX (types kept opaque so this header needs no GL includes)
    void *glContext_; //!< GLXContext
    void *fbConfig_;  //!< GLXFBConfig

    WindowDetails windowDetails_;
    WindowFlags windowFlags_;
    GraphicsAPI graphicsAPI_;
    std::unique_ptr<X11KeyboardListener> keyboardListener_;
    std::unique_ptr<X11MouseListener> mouseListener_;
    bool windowShouldClose_;

    void handleWindowEvent(const XEvent *event);
    void allocateSoftwareImage(i32 width, i32 height);
    void destroySoftwareImage();
    void initGL();
};

} // namespace wma

#endif // WMA_BACKENDS_X11_WINDOW_MANAGER_HPP
