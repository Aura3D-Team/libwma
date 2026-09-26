#ifndef WMA_INPUT_TOUCH_TYPES_HPP
#define WMA_INPUT_TOUCH_TYPES_HPP

#include <ink/ink_base.hpp>

namespace wma
{

//! Stable per-finger identifier: the same value is reported for every event
//! belonging to one continuous press, so callers can track several fingers at
//! once (which synthesized mouse events cannot express -- a mouse has one
//! cursor no matter how many fingers are down).
using TouchFingerId = i64;

//! A single finger's state. Coordinates are in window pixels, matching
//! WMAMousePosition, rather than SDL's normalized 0..1 touch range.
struct WMATouchPoint
{
    TouchFingerId fingerId = 0;
    f64 x = 0.0;
    f64 y = 0.0;
    f64 deltaX = 0.0; //! Movement since this finger's previous event.
    f64 deltaY = 0.0; //! Y-up, matching WMAMousePosition's convention.

    constexpr WMATouchPoint() noexcept = default;

    constexpr WMATouchPoint(TouchFingerId fingerId, f64 x, f64 y, f64 deltaX = 0.0, f64 deltaY = 0.0) noexcept
        : fingerId(fingerId), x(x), y(y), deltaX(deltaX), deltaY(deltaY)
    {
    }
};

} // namespace wma

#endif // WMA_INPUT_TOUCH_TYPES_HPP
