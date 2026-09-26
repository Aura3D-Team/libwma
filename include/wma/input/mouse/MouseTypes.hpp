#ifndef WMA_INPUT_MOUSE_TYPES_HPP
#define WMA_INPUT_MOUSE_TYPES_HPP

#include <ink/ink_base.hpp>

namespace wma
{

struct WMAMousePosition
{
    f64 x = 0.0;
    f64 y = 0.0;
    f64 deltaX = 0.0;
    f64 deltaY = 0.0;

    constexpr WMAMousePosition() noexcept = default;

    constexpr WMAMousePosition(f64 x, f64 y, f64 deltaX = 0.0, f64 deltaY = 0.0) noexcept
        : x(x), y(y), deltaX(deltaX), deltaY(deltaY)
    {
    }
};

struct WMAMouseScroll
{
    f64 xOffset = 0.0;
    f64 yOffset = 0.0;

    constexpr WMAMouseScroll() noexcept = default;

    constexpr WMAMouseScroll(f64 xOffset, f64 yOffset) noexcept : xOffset(xOffset), yOffset(yOffset)
    {
    }
};

} // namespace wma

#endif // WMA_INPUT_MOUSE_TYPES_HPP
