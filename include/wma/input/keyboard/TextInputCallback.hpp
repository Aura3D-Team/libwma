#ifndef WMA_INPUT_TEXT_INPUT_CALLBACK_HPP
#define WMA_INPUT_TEXT_INPUT_CALLBACK_HPP

#include <memory>

#include "wma/core/Types.hpp"
#include "wma/input/keyboard/KeyTypes.hpp"

namespace wma
{

/**
 * @brief Receives one Unicode scalar value of committed text.
 *
 * Distinct from a key binding, and deliberately so: a key event reports which
 * *physical* key moved, while this reports the *character* the platform's
 * keyboard layout, dead keys and IME decided that keystroke produced. Shift+2
 * is one key event and one of '@' or '"' depending on layout; a compose
 * sequence is several key events and one character. A text field must consume
 * this stream, never the key stream, or it would be wrong on every non-US
 * layout.
 *
 * Follows @ref InputCallback's representation: a raw function pointer plus
 * userdata on the fast path, with capturing lambdas kept alive in storage_.
 */
struct TextInputCallback
{
    using Fn = void (*)(void *, Codepoint);

    Fn fn = nullptr;
    void *data = nullptr;
    std::shared_ptr<void> storage_;

    constexpr TextInputCallback() = default;

    TextInputCallback(Fn callback, void *userData) noexcept : fn(callback), data(userData)
    {
    }

    void operator()(Codepoint codepoint) const noexcept(false)
    {
        if (fn) [[likely]]
            fn(data, codepoint);
    }

    [[nodiscard]] bool valid() const noexcept
    {
        return fn != nullptr;
    }

    static TextInputCallback from(wma::move_only_function<void(Codepoint)> callback);
};

} // namespace wma

#endif // WMA_INPUT_TEXT_INPUT_CALLBACK_HPP
