#include "wma/input/keyboard/KeyboardListener.hpp"

#include <algorithm>
#include <utility>

namespace wma
{

KeyboardListener::KeyboardListener()
{
    keyBindings_.emplace_back();
    keyEventActions_.emplace_back();
    textInputActions_.emplace_back();
}

InputContextId KeyboardListener::createContext()
{
    const InputContextId context = contexts_.createContext();
    ensureContextCapacity(context);
    return context;
}

void KeyboardListener::setActiveContext(InputContextId context)
{
    contexts_.setActiveContext(context);
}

void KeyboardListener::pushContext(InputContextId context)
{
    contexts_.pushContext(context);
}

void KeyboardListener::popContext()
{
    contexts_.popContext();
}

InputContextId KeyboardListener::getActiveContext() const
{
    return contexts_.activeContext();
}

InputContextId KeyboardListener::getResolvedContext() const
{
    return contexts_.resolved();
}

void KeyboardListener::addKeyAction(Key key, KeyAction action)
{
    addKeyAction(key, std::move(action), contexts_.resolved());
}

void KeyboardListener::addKeyAction(Key key, KeyAction action, InputContextId context)
{
    const auto k = std::to_underlying(key);
    if (k < 0 || static_cast<usize>(k) >= KEY_COUNT) [[unlikely]]
        return;
    ensureContextCapacity(context);
    keyBindings_[context][static_cast<usize>(k)] = std::move(action);
}

void KeyboardListener::addKeyAction(i32 key, KeyAction action)
{
    addKeyAction(static_cast<Key>(key), std::move(action));
}

void KeyboardListener::addKeyAction(i32 key, KeyAction action, InputContextId context)
{
    addKeyAction(static_cast<Key>(key), std::move(action), context);
}

void KeyboardListener::removeKeyAction(Key key)
{
    removeKeyAction(key, contexts_.resolved());
}

void KeyboardListener::removeKeyAction(Key key, InputContextId context)
{
    const auto k = std::to_underlying(key);
    if (k < 0 || static_cast<usize>(k) >= KEY_COUNT) [[unlikely]]
        return;
    ensureContextCapacity(context);
    keyBindings_[context].clearSlot(static_cast<usize>(k));
}

void KeyboardListener::removeKeyAction(i32 key)
{
    removeKeyAction(static_cast<Key>(key));
}

void KeyboardListener::removeKeyAction(i32 key, InputContextId context)
{
    removeKeyAction(static_cast<Key>(key), context);
}

void KeyboardListener::clearKeyActions()
{
    std::ranges::for_each(keyBindings_,
                          [](auto &t)
                          {
                              t.clear();
                          });
}

void KeyboardListener::clearKeyActions(InputContextId context)
{
    ensureContextCapacity(context);
    keyBindings_[context].clear();
}

bool KeyboardListener::hasKeyAction(Key key) const
{
    return hasKeyAction(key, contexts_.resolved());
}

bool KeyboardListener::hasKeyAction(Key key, InputContextId context) const
{
    const auto k = std::to_underlying(key);
    if (k < 0 || static_cast<usize>(k) >= KEY_COUNT) [[unlikely]]
        return false;
    if (context >= keyBindings_.size())
        return false;
    return keyBindings_[context].has(static_cast<usize>(k));
}

bool KeyboardListener::hasKeyAction(i32 key) const
{
    return hasKeyAction(static_cast<Key>(key));
}

bool KeyboardListener::hasKeyAction(i32 key, InputContextId context) const
{
    return hasKeyAction(static_cast<Key>(key), context);
}

void KeyboardListener::setKeyEventAction(KeyEventCallback action)
{
    setKeyEventAction(std::move(action), contexts_.resolved());
}

void KeyboardListener::setKeyEventAction(KeyEventCallback action, InputContextId context)
{
    ensureContextCapacity(context);
    keyEventActions_[context] = std::move(action);
}

void KeyboardListener::setTextInputAction(TextInputCallback action)
{
    setTextInputAction(std::move(action), contexts_.resolved());
}

void KeyboardListener::setTextInputAction(TextInputCallback action, InputContextId context)
{
    ensureContextCapacity(context);
    textInputActions_[context] = std::move(action);
}

void KeyboardListener::clearKeyEventAction()
{
    const InputContextId ctx = contexts_.resolved();
    if (ctx < keyEventActions_.size())
        keyEventActions_[ctx] = KeyEventCallback{};
}

void KeyboardListener::clearTextInputAction()
{
    const InputContextId ctx = contexts_.resolved();
    if (ctx < textInputActions_.size())
        textInputActions_[ctx] = TextInputCallback{};
}

bool KeyboardListener::isKeyDown(Key key) const noexcept
{
    const auto k = std::to_underlying(key);
    if (k < 0 || static_cast<usize>(k) >= KEY_COUNT) [[unlikely]]
        return false;
    return keyDown_[static_cast<usize>(k)];
}

KeyModifiers KeyboardListener::modifiers() const noexcept
{
    return KeyModifiers{
        .shift = isKeyDown(KEY_LEFT_SHIFT) || isKeyDown(KEY_RIGHT_SHIFT),
        .ctrl = isKeyDown(KEY_LEFT_CTRL) || isKeyDown(KEY_RIGHT_CTRL),
        .alt = isKeyDown(KEY_LEFT_ALT) || isKeyDown(KEY_RIGHT_ALT),
        .super = isKeyDown(KEY_LEFT_SUPER) || isKeyDown(KEY_RIGHT_SUPER),
    };
}

void KeyboardListener::releaseAllKeys() noexcept
{
    keyDown_.fill(false);
}

void KeyboardListener::dispatchKeyPress(Key key)
{
    dispatchKeyPress(key, /*repeat=*/false);
}

void KeyboardListener::dispatchKeyPress(Key key, bool repeat)
{
    const auto k = std::to_underlying(key);
    if (k < 0 || static_cast<usize>(k) >= KEY_COUNT) [[unlikely]]
        return;
    [[assume(k >= 0)]];
    [[assume(static_cast<usize>(k) < KEY_COUNT)]];

    //! Recorded before the callbacks run, so a handler that queries
    //! modifiers() (or isKeyDown() for the very key that fired) sees the
    //! state the event describes rather than the one preceding it.
    keyDown_[static_cast<usize>(k)] = true;

    const InputContextId ctx = contexts_.resolved();
    if (ctx >= keyBindings_.size()) [[unlikely]]
        return;
    [[assume(ctx < keyBindings_.size())]];

    /*
     * An auto-repeat is deliberately not delivered to the binding table: a
     * KeyAction models "this action happened", and firing it at the OS repeat
     * rate would make a bound jump or fire re-trigger while the key is merely
     * held. Callers that do want repeat (text editing) read the key-event
     * stream below, which reports it faithfully.
     */
    if (!repeat)
        keyBindings_[ctx][static_cast<usize>(k)].executePress();

    if (ctx < keyEventActions_.size() && keyEventActions_[ctx].valid())
    {
        keyEventActions_[ctx](WMAKeyEvent{key, repeat ? KeyState::Repeat : KeyState::Pressed, modifiers()});
    }
}

void KeyboardListener::dispatchKeyRelease(Key key)
{
    const auto k = std::to_underlying(key);
    if (k < 0 || static_cast<usize>(k) >= KEY_COUNT) [[unlikely]]
        return;
    [[assume(k >= 0)]];
    [[assume(static_cast<usize>(k) < KEY_COUNT)]];

    keyDown_[static_cast<usize>(k)] = false;

    const InputContextId ctx = contexts_.resolved();
    if (ctx >= keyBindings_.size()) [[unlikely]]
        return;
    [[assume(ctx < keyBindings_.size())]];

    keyBindings_[ctx][static_cast<usize>(k)].executeRelease();

    if (ctx < keyEventActions_.size() && keyEventActions_[ctx].valid())
        keyEventActions_[ctx](WMAKeyEvent{key, KeyState::Released, modifiers()});
}

void KeyboardListener::dispatchText(Codepoint codepoint)
{
    /*
     * Control characters are dropped here rather than in each backend: SDL and
     * X11 both report Enter/Tab/Backspace as text (U+000D, U+0009, U+0008) as
     * well as key events, and a text field that consumed both would insert a
     * literal control character *and* act on the keystroke. The key stream is
     * the authority for those; this stream carries printable text only.
     *
     * U+007F (Delete) is excluded for the same reason. Everything at or above
     * U+00A0 passes, which keeps NBSP and all non-Latin text.
     */
    if (codepoint < 0x20u || (codepoint >= 0x7Fu && codepoint < 0xA0u))
        return;

    const InputContextId ctx = contexts_.resolved();
    if (ctx >= textInputActions_.size()) [[unlikely]]
        return;

    if (textInputActions_[ctx].valid())
        textInputActions_[ctx](codepoint);
}

void KeyboardListener::ensureContextCapacity(InputContextId context)
{
    while (keyBindings_.size() <= context)
    {
        keyBindings_.emplace_back();
    }
    while (keyEventActions_.size() <= context)
    {
        keyEventActions_.emplace_back();
    }
    while (textInputActions_.size() <= context)
    {
        textInputActions_.emplace_back();
    }
}

} // namespace wma
