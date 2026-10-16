#ifndef WMA_BACKENDS_X11_WINDOW_MANAGER_HPP
#define WMA_BACKENDS_X11_WINDOW_MANAGER_HPP

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XShm.h>
#include <array>
#include <memory>

#include "X11KeyboardListener.hpp"
#include "X11MouseListener.hpp"
#include "wma/backends/WindowHit.hpp"
#include "wma/managers/IWindowManager.hpp"

namespace wma
{

class X11Clipboard;

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
    bool minimize() noexcept override;
    bool maximize() noexcept override;
    bool restore() noexcept override;
    [[nodiscard]] bool isMaximized() const noexcept override;
    void close() noexcept override;
    bool setHitTest(HitTest hitTest) override;
    bool setTitle(const char *title) noexcept override;
    [[nodiscard]] DecorationMode getDecorationMode() const noexcept override;
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
    [[nodiscard]] bool isClipboardSupported() const noexcept override;
    [[nodiscard]] std::string getClipboardText() override;
    bool setClipboardText(std::string_view text) override;
    MouseListener &getMouseListener() noexcept override;
    [[nodiscard]] u32 displayRefreshHz() const noexcept override;
    bool shouldClose() const override;
    WindowBackend getBackendType() const override;
    GraphicsAPI getGraphicsAPI() const override;
    WmaCode destroy() override;

  private:
    friend class X11MouseListener;

    //! Takes a press the hit test assigns to the frame: resizes at once, and holds a
    //! title-bar press until dragTo() turns it into a move or the release into a click.
    [[nodiscard]] bool claimPress(const XButtonEvent &press);
    void dragTo(const XMotionEvent &motion);
    void releaseClaim() noexcept;
    [[nodiscard]] SystemCursor hoverCursor(f64 x, f64 y) const;

    Display *display_;
    Window window_;
    Colormap colormap_;
    Atom wmDeleteWindow_;

    //! Cached so per-frame queries never round-trip to the X server.
    Atom netWmState_ = 0;
    Atom netWmStateMaximizedVert_ = 0;
    Atom netWmStateMaximizedHorz_ = 0;
    Atom netWmName_ = 0;
    Atom utf8String_ = 0;
    bool maximized_ = false;

    //! Software rendering (GraphicsAPI::CPU). Two slots, so the next frame is drawn while the
    //! server still reads the last, as the Wayland backend does with its wl_shm pool.
    struct Slot
    {
        XImage *image = nullptr;
        XShmSegmentInfo shm{};
        bool busy = false; //!< The server has not yet reported it is done reading it.
    };
    GC gc_;
    std::array<Slot, 2> slots_{};
    Slot *locked_ = nullptr;
    int shmCompletion_ = -1;   //!< ShmCompletion event type; -1 without MIT-SHM
    bool shmRejected_ = false; //!< The server could not attach a segment; slots use XPutImage

    //! OpenGL via GLX (types kept opaque so this header needs no GL includes)
    void *glContext_; //!< GLXContext
    void *fbConfig_;  //!< GLXFBConfig

    WindowDetails windowDetails_;
    WindowFlags windowFlags_;
    GraphicsAPI graphicsAPI_;
    std::unique_ptr<X11KeyboardListener> keyboardListener_;
    std::unique_ptr<X11MouseListener> mouseListener_;
    //! Created with the window; the selection it owns goes with the connection, so it is released before it closes.
    std::unique_ptr<X11Clipboard> clipboard_;
    bool windowShouldClose_;
    HitTest hitTest_;
    detail::CaptionGesture caption_;
    unsigned int captionButton_ = 0;

    void handleWindowEvent(const XEvent *event);
    void createSlot(Slot &slot, i32 width, i32 height);
    [[nodiscard]] bool createShmImage(Slot &slot, i32 width, i32 height);
    void destroySlot(Slot &slot) noexcept;
    void initGL();
    bool requestMaximized(bool maximized) noexcept;
    void refreshMaximized() noexcept;
};

} // namespace wma

#endif // WMA_BACKENDS_X11_WINDOW_MANAGER_HPP
