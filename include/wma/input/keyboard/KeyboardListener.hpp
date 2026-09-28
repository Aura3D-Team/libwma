#ifndef WMA_INPUT_KEYBOARD_LISTENER_HPP
#define WMA_INPUT_KEYBOARD_LISTENER_HPP

#include <array>
#include <vector>

#include "wma/core/Types.hpp"
#include "wma/input/InputBindingTable.hpp"
#include "wma/input/InputContextStack.hpp"
#include "wma/input/InputTypes.hpp"
#include "wma/input/keyboard/KeyAction.hpp"
#include "wma/input/keyboard/KeyEventCallback.hpp"
#include "wma/input/keyboard/KeyTypes.hpp"
#include "wma/input/keyboard/Keys.h"
#include "wma/input/keyboard/TextInputCallback.hpp"

namespace wma
{

class KeyboardListener
{
  public:
    KeyboardListener();
    virtual ~KeyboardListener() = default;

    KeyboardListener(const KeyboardListener &) = delete;
    KeyboardListener &operator=(const KeyboardListener &) = delete;
    KeyboardListener(KeyboardListener &&) = default;
    KeyboardListener &operator=(KeyboardListener &&) = default;

    //! Context management.
    [[nodiscard]] InputContextId createContext();
    void setActiveContext(InputContextId context);
    void pushContext(InputContextId context);
    void popContext();
    [[nodiscard]] InputContextId getActiveContext() const;
    [[nodiscard]] InputContextId getResolvedContext() const;

    //! Binding — default context (resolved at call time).
    void addKeyAction(Key key, KeyAction action);
    void addKeyAction(i32 key, KeyAction action);

    //! Binding — explicit context.
    void addKeyAction(Key key, KeyAction action, InputContextId context);
    void addKeyAction(i32 key, KeyAction action, InputContextId context);

    void removeKeyAction(Key key);
    void removeKeyAction(i32 key);
    void removeKeyAction(Key key, InputContextId context);
    void removeKeyAction(i32 key, InputContextId context);

    void clearKeyActions();
    void clearKeyActions(InputContextId context);

    [[nodiscard]] bool hasKeyAction(Key key) const;
    [[nodiscard]] bool hasKeyAction(i32 key) const;
    [[nodiscard]] bool hasKeyAction(Key key, InputContextId context) const;
    [[nodiscard]] bool hasKeyAction(i32 key, InputContextId context) const;

    /**
     * @brief Subscribes to *every* key event, alongside the binding table.
     *
     * Both fire: installing this does not disturb any addKeyAction() binding,
     * which is the point -- a UI layer needs Tab/arrows/Home/End/Backspace
     * without evicting whatever the application bound to them. See
     * @ref KeyEventCallback.
     */
    void setKeyEventAction(KeyEventCallback action);
    void setKeyEventAction(KeyEventCallback action, InputContextId context);

    /**
     * @brief Subscribes to committed text, as Unicode scalar values.
     *
     * Layout-, dead-key- and IME-correct, which the key stream is not; a text
     * field must read this rather than translating keycodes itself. See
     * @ref TextInputCallback.
     *
     * Requires text input to be enabled on the window -- see
     * IWindowManager::setTextInputEnabled(), which some platforms need in
     * order to raise an on-screen keyboard.
     */
    void setTextInputAction(TextInputCallback action);
    void setTextInputAction(TextInputCallback action, InputContextId context);

    void clearKeyEventAction();
    void clearTextInputAction();

    //! Whether @p key is held right now. Tracked from the dispatched press and
    //! release stream, so it is correct on every backend without polling.
    [[nodiscard]] bool isKeyDown(Key key) const noexcept;

    //! Modifier keys held right now, derived from isKeyDown(); see
    //! @ref KeyModifiers for why this is not read from the backend's mask.
    [[nodiscard]] KeyModifiers modifiers() const noexcept;

    /**
     * @brief Forgets every held key.
     *
     * Called when the window loses focus: keys released while another window
     * had focus never produce a release event here, so without this a modifier
     * held during an Alt-Tab would stay stuck down forever.
     */
    void releaseAllKeys() noexcept;

  protected:
    void dispatchKeyPress(Key key);
    void dispatchKeyRelease(Key key);

    //! As above, but distinguishing auto-repeat -- the form backends should
    //! prefer. dispatchKeyPress(key) is equivalent to repeat == false.
    void dispatchKeyPress(Key key, bool repeat);

    //! Dispatches one Unicode scalar value to the text-input callback.
    void dispatchText(Codepoint codepoint);

    void ensureContextCapacity(InputContextId context);

    InputContextStack contexts_;
    std::vector<InputBindingTable<KeyAction, KEY_COUNT>> keyBindings_;
    std::vector<KeyEventCallback> keyEventActions_;
    std::vector<TextInputCallback> textInputActions_;

    //! Physical key state, maintained by dispatchKeyPress/Release so that
    //! modifiers() needs no per-backend mask. std::array rather than
    //! std::vector<bool>: KEY_COUNT is a compile-time constant and this is
    //! read on the event path.
    std::array<bool, KEY_COUNT> keyDown_{};
};

} // namespace wma

#endif // WMA_INPUT_KEYBOARD_LISTENER_HPP
