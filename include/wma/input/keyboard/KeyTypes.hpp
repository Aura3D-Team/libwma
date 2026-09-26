#ifndef WMA_INPUT_KEY_TYPES_HPP
#define WMA_INPUT_KEY_TYPES_HPP

#include <ink/ink_base.hpp>

#include "wma/input/keyboard/Keys.h"

namespace wma
{

/**
 * @brief Whether a key event is a fresh press, an auto-repeat, or a release.
 *
 * Repeat is distinguished from Pressed rather than folded into it because the
 * two mean different things to a caller: a text field wants Backspace to keep
 * deleting while held (so it treats both alike), while a jump action must fire
 * once per physical press (so it ignores Repeat entirely). Collapsing them
 * would make the second case impossible to express.
 */
enum class KeyState : u8
{
    Released = 0,
    Pressed,
    Repeat
};

/**
 * @brief Modifier keys held at the moment an event was produced.
 *
 * Derived by @ref KeyboardListener from the press/release stream it already
 * tracks, rather than read from each backend's own modifier mask: the four
 * backends disagree on both the bit layout and on when the mask is sampled
 * (X11 reports the state *before* the event, GLFW the state after), and a
 * caller comparing `ctrl` across platforms should not have to know that.
 */
struct KeyModifiers
{
    bool shift = false;
    bool ctrl = false;
    bool alt = false;
    bool super = false; //! Windows / Command.

    [[nodiscard]] constexpr bool any() const noexcept
    {
        return shift || ctrl || alt || super;
    }

    //! True when no modifier other than Shift is held. The usual test for
    //! "this keystroke should insert a character".
    [[nodiscard]] constexpr bool onlyShiftOrNone() const noexcept
    {
        return !ctrl && !alt && !super;
    }

    friend constexpr bool operator==(const KeyModifiers &, const KeyModifiers &) noexcept = default;
};

/**
 * @brief One keyboard event, as delivered to a @ref KeyEventCallback.
 *
 * The whole-keyboard counterpart to @ref KeyAction's per-key bindings. A UI
 * layer needs to see every key without claiming a binding slot for each one
 * (and without displacing the application's own bindings), which is what this
 * exists for; gameplay code should keep using the binding table.
 */
struct WMAKeyEvent
{
    Key key = KEY_UNKNOWN;
    KeyState state = KeyState::Released;
    KeyModifiers mods{};

    constexpr WMAKeyEvent() noexcept = default;

    constexpr WMAKeyEvent(Key key, KeyState state, KeyModifiers mods) noexcept : key(key), state(state), mods(mods)
    {
    }

    //! True for a fresh press or an auto-repeat -- the test an editing action
    //! (backspace, cursor movement) wants.
    [[nodiscard]] constexpr bool isPressOrRepeat() const noexcept
    {
        return state == KeyState::Pressed || state == KeyState::Repeat;
    }
};

/**
 * @brief A single Unicode scalar value produced by the platform's text input.
 *
 * Deliberately a decoded codepoint rather than the UTF-8 bytes each backend
 * happens to hand over: it is fixed-size (so a callback never has to reason
 * about the lifetime of a borrowed buffer), and it is what an editing caret
 * moves over. Backends that produce UTF-8 (SDL, X11, xkb) decode before
 * dispatching; a multi-codepoint IME commit arrives as several calls.
 */
using Codepoint = u32;

} // namespace wma

#endif // WMA_INPUT_KEY_TYPES_HPP
