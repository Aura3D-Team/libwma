#ifndef WMA_BACKENDS_X11_MOUSE_LISTENER_HPP
#define WMA_BACKENDS_X11_MOUSE_LISTENER_HPP

#include "wma/input/mouse/MouseListener.hpp"
#include <X11/Xlib.h>

namespace wma
{

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
    Display *display_ = nullptr;
    Window x11Window_ = 0;
    Cursor invisibleCursor_;

    Cursor createInvisibleCursor(Display *display, Window window);
    i32 convertButton(i32 x11Button) const;
};

} // namespace wma

#endif // WMA_BACKENDS_X11_MOUSE_LISTENER_HPP
