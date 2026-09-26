#ifndef WMA_INPUT_MOUSE_ACTION_HPP
#define WMA_INPUT_MOUSE_ACTION_HPP

#include <functional>

#include "wma/input/InputCallback.hpp"
#include "wma/input/mouse/MouseInputCallback.hpp"
#include "wma/input/mouse/MouseTypes.hpp"

namespace wma
{

class MouseAction
{
  public:
    using ButtonCallback = wma::move_only_function<void()>;
    using PositionCallback = wma::move_only_function<void(const WMAMousePosition &)>;
    using ScrollCallback = wma::move_only_function<void(const WMAMouseScroll &)>;

    constexpr MouseAction() = default;

    //! Button action (press + optional release).
    MouseAction(ButtonCallback onPress, ButtonCallback onRelease = nullptr)
        : onPress_(InputCallback::from(std::move(onPress))), onRelease_(InputCallback::from(std::move(onRelease)))
    {
    }

    //! Move action.
    explicit MouseAction(PositionCallback onMove) : onMove_(MoveInputCallback::from(std::move(onMove)))
    {
    }

    //! Scroll action.
    explicit MouseAction(ScrollCallback onScroll) : onScroll_(ScrollInputCallback::from(std::move(onScroll)))
    {
    }

    [[nodiscard]] bool hasPressAction() const noexcept
    {
        return onPress_.valid();
    }
    [[nodiscard]] bool hasReleaseAction() const noexcept
    {
        return onRelease_.valid();
    }
    [[nodiscard]] bool hasMoveAction() const noexcept
    {
        return onMove_.valid();
    }
    [[nodiscard]] bool hasScrollAction() const noexcept
    {
        return onScroll_.valid();
    }
    [[nodiscard]] bool isBound() const noexcept
    {
        return hasPressAction() || hasReleaseAction() || hasMoveAction() || hasScrollAction();
    }

    void executePress() const
    {
        if (onPress_.valid())
            onPress_();
    }
    void executeRelease() const
    {
        if (onRelease_.valid())
            onRelease_();
    }
    void executeMove(const WMAMousePosition &p) const
    {
        if (onMove_.valid())
            onMove_(p);
    }
    void executeScroll(const WMAMouseScroll &s) const
    {
        if (onScroll_.valid())
            onScroll_(s);
    }

  private:
    InputCallback onPress_;
    InputCallback onRelease_;
    MoveInputCallback onMove_;
    ScrollInputCallback onScroll_;
};

namespace MouseButton
{
constexpr i32 WMALeft = 0;
constexpr i32 WMARight = 1;
constexpr i32 WMAMiddle = 2;
constexpr i32 WMAButton4 = 3;
constexpr i32 WMAButton5 = 4;
constexpr i32 WMAButton6 = 5;
constexpr i32 WMAButton7 = 6;
constexpr i32 WMAButton8 = 7;
} // namespace MouseButton

} // namespace wma

#endif // WMA_INPUT_MOUSE_ACTION_HPP
