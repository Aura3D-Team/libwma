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

    //! A grab outlives focus, so it has to follow it or an alt-tab leaves the pointer trapped.
    void setFocused(bool focused);

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
    //! XInput2's opcode, or 0 without it, in which case capture only confines the pointer.
    int xiOpcode_ = 0;
    bool focused_ = false;
    bool captured_ = false;
    Cursor invisibleCursor_;
    //! Created on first use; freed with the display connection.
    std::array<Cursor, SYSTEM_CURSOR_COUNT> shapeCursors_{};

    Cursor createInvisibleCursor(Display *display, Window window);
    //! Grabs and confines the pointer while the cursor is disabled and the window focused.
    void updateCapture();
    void selectRawMotion(bool enabled) const;
    void handleRawMotion(XGenericEventCookie &cookie);
    i32 convertButton(i32 x11Button) const;
};

} // namespace wma

#endif // WMA_BACKENDS_X11_MOUSE_LISTENER_HPP
