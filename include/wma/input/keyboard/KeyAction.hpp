#ifndef WMA_INPUT_KEY_ACTION_HPP
#define WMA_INPUT_KEY_ACTION_HPP

#include <functional>

#include "wma/input/InputCallback.hpp"

namespace wma
{

/**
 * @brief Binds a press and/or release callback to a key.
 *
 * The fast path (no captures) uses a raw function pointer with zero overhead.
 * Capturing lambdas are stored via std::move_only_function, allowing move-only
 * captures (e.g. std::unique_ptr) without requiring copyability.
 */
class KeyAction
{
  public:
    //! Accepting std::move_only_function lets users pass any callable — plain
    //! lambdas, capturing lambdas, move-only closures, or nullptr.
    using Callback = wma::move_only_function<void()>;

    constexpr KeyAction() = default;

    KeyAction(Callback onPress, Callback onRelease = nullptr)
        : onPress_(InputCallback::from(std::move(onPress))), onRelease_(InputCallback::from(std::move(onRelease)))
    {
    }

    //! Fast constructor: raw function pointer + userdata, zero heap allocation.
    KeyAction(void (*onPress)(void *), void *pressData, void (*onRelease)(void *) = nullptr,
              void *releaseData = nullptr) noexcept
        : onPress_(onPress, pressData), onRelease_(onRelease, releaseData)
    {
    }

    void executePress() const
    {
        onPress_();
    }
    void executeRelease() const
    {
        onRelease_();
    }

    [[nodiscard]] bool hasPressAction() const noexcept
    {
        return onPress_.valid();
    }
    [[nodiscard]] bool hasReleaseAction() const noexcept
    {
        return onRelease_.valid();
    }
    [[nodiscard]] bool isBound() const noexcept
    {
        return hasPressAction() || hasReleaseAction();
    }

  private:
    InputCallback onPress_;
    InputCallback onRelease_;
};

} // namespace wma

#endif // WMA_INPUT_KEY_ACTION_HPP
