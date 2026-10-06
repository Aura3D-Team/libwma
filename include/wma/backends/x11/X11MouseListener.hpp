#ifndef WMA_BACKENDS_X11_MOUSE_LISTENER_HPP
#define WMA_BACKENDS_X11_MOUSE_LISTENER_HPP

#include "wma/input/mouse/MouseListener.hpp"
#include <X11/Xlib.h>
#include <array>

namespace wma
{

class X11WindowManager;

class X11MouseListener : public MouseListener
{
  public:
    X11MouseListener();
    ~X11MouseListener() override = default;

    void initialize(Display *display, Window window);
    void handleEvent(const XEvent *event);

  protected:
    void updateCursorState() override;

  private:
    friend class X11WindowManager;

    //! Claims title-bar and border presses before the application sees them.
    X11WindowManager *owner_ = nullptr;
    //! The release of a claimed press goes to the window manager's grab too.
    bool pressClaimed_ = false;

    Display *display_ = nullptr;
    Window x11Window_ = 0;
    Cursor invisibleCursor_;
    //! Created on first use; freed with the display connection.
    std::array<Cursor, SYSTEM_CURSOR_COUNT> shapeCursors_{};

    Cursor createInvisibleCursor(Display *display, Window window);
    i32 convertButton(i32 x11Button) const;
};

} // namespace wma

#endif // WMA_BACKENDS_X11_MOUSE_LISTENER_HPP
