#ifndef WMA_BACKENDS_X11_KEYBOARD_LISTENER_HPP
#define WMA_BACKENDS_X11_KEYBOARD_LISTENER_HPP

#include "wma/input/keyboard/KeyboardListener.hpp"
#include <X11/Xlib.h>
//! For XLookupString, the no-input-method fallback in lookupAndDispatchText().
#include <X11/Xutil.h>
#include <X11/keysym.h>

namespace wma
{

/**
 * @class X11KeyboardListener
 *
 * @brief X11 keyboard input, including layout-correct text via XIM.
 *
 * Text is produced with Xutf8LookupString against a per-window input context
 * (XIC) rather than with plain XLookupString: the latter only ever returns
 * Latin-1, so it cannot express most of the world's keyboards, and it runs no
 * dead-key or compose sequences at all. Where no input method is available the
 * listener falls back to XLookupString, which still covers ASCII typing --
 * degrading rather than leaving text input dead.
 */
class X11KeyboardListener : public KeyboardListener
{
  public:
    X11KeyboardListener();
    ~X11KeyboardListener() override;

    void initialize(Display *display);

    //! Opens the XIM input context used for text lookup. Separate from
    //! initialize() because it needs the window, which is created later.
    void attachWindow(Window window);

    void handleKeyEvent(KeySym x11Key, const XKeyEvent &xKeyEvent);

    //! Bookkeeping only: X11 has no on-screen keyboard, and the XIC is opened
    //! once in attachWindow() rather than toggled per field.
    void setTextInputEnabled(bool enabled) noexcept
    {
        textInputEnabled_ = enabled;
    }
    [[nodiscard]] bool isTextInputEnabled() const noexcept
    {
        return textInputEnabled_;
    }

    /**
     * @brief True when @p event was consumed by the input method.
     *
     * A dead key mid-sequence, or a keystroke being composed into an IME
     * candidate, must not also be handled as an ordinary key press. Callers
     * pump this before dispatching, as XFilterEvent's contract requires.
     */
    [[nodiscard]] bool filterEvent(XEvent *event) const;

  private:
    //! Decodes @p xKeyEvent into UTF-8 and dispatches each scalar value.
    void lookupAndDispatchText(const XKeyEvent &xKeyEvent);

    Display *display_ = nullptr;
    Window window_ = 0;
    XIM inputMethod_ = nullptr;
    XIC inputContext_ = nullptr;
    bool textInputEnabled_ = false;
};

} // namespace wma

#endif // WMA_BACKENDS_X11_KEYBOARD_LISTENER_HPP
