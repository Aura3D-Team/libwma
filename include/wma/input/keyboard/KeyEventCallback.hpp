#ifndef WMA_INPUT_KEY_EVENT_CALLBACK_HPP
#define WMA_INPUT_KEY_EVENT_CALLBACK_HPP

#include <memory>

#include "wma/core/Types.hpp"
#include "wma/input/keyboard/KeyTypes.hpp"

namespace wma
{

/**
 * @brief Receives every key event, rather than one bound key.
 *
 * The binding table (@ref KeyAction) is the right shape for gameplay, where a
 * handful of keys map to named actions. It is the wrong shape for a UI layer,
 * which needs Tab, the arrows, Home/End, Backspace, Delete, Enter and Escape
 * at once and must not evict whatever the application bound to them. One of
 * these covers all of it, and coexists with the binding table -- both fire.
 *
 * Follows @ref InputCallback's representation: a raw function pointer plus
 * userdata on the fast path, with capturing lambdas kept alive in storage_.
 */
struct KeyEventCallback
{
    using Fn = void (*)(void *, const WMAKeyEvent &);

    Fn fn = nullptr;
    void *data = nullptr;
    std::shared_ptr<void> storage_;

    constexpr KeyEventCallback() = default;

    KeyEventCallback(Fn callback, void *userData) noexcept : fn(callback), data(userData)
    {
    }

    void operator()(const WMAKeyEvent &event) const noexcept(false)
    {
        if (fn) [[likely]]
            fn(data, event);
    }

    [[nodiscard]] bool valid() const noexcept
    {
        return fn != nullptr;
    }

    static KeyEventCallback from(wma::move_only_function<void(const WMAKeyEvent &)> callback);
};

} // namespace wma

#endif // WMA_INPUT_KEY_EVENT_CALLBACK_HPP
