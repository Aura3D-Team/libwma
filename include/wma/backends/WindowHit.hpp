#ifndef WMA_BACKENDS_WINDOW_HIT_HPP
#define WMA_BACKENDS_WINDOW_HIT_HPP

#include <chrono>
#include <cmath>

#include "wma/managers/IWindowManager.hpp"

//! Shared by the backends that act on IWindowManager::setHitTest() themselves.
namespace wma::detail
{

[[nodiscard]] constexpr SystemCursor cursorFor(WindowHit hit) noexcept
{
    switch (hit)
    {
    case WindowHit::Top:
    case WindowHit::Bottom:
        return SystemCursor::NsResize;
    case WindowHit::Left:
    case WindowHit::Right:
        return SystemCursor::EwResize;
    case WindowHit::TopLeft:
    case WindowHit::BottomRight:
        return SystemCursor::NwseResize;
    case WindowHit::TopRight:
    case WindowHit::BottomLeft:
        return SystemCursor::NeswResize;
    case WindowHit::Client:
    case WindowHit::Caption:
        break;
    }
    return SystemCursor::Default;
}

inline void toggleMaximized(IWindowManager &window) noexcept
{
    if (window.isMaximized())
        window.restore();
    else
        window.maximize();
}

//! A caption is a client surface, so no window manager sees a double-click on
//! it. Interval and slop match AuraUI's own double-click.
class CaptionClicks
{
  public:
    //! True for the second press of a double-click, which also ends the sequence.
    [[nodiscard]] bool press(f64 x, f64 y) noexcept
    {
        const bool second = armedNear(x, y);
        armed_ = !second;
        last_ = Clock::now();
        x_ = x;
        y_ = y;
        return second;
    }

    [[nodiscard]] bool armedNear(f64 x, f64 y) const noexcept
    {
        return armed_ && Clock::now() - last_ <= kInterval && std::abs(x - x_) <= kSlop && std::abs(y - y_) <= kSlop;
    }

  private:
    using Clock = std::chrono::steady_clock;
    static constexpr auto kInterval = std::chrono::milliseconds(400);
    static constexpr f64 kSlop = 5.0;

    Clock::time_point last_{};
    f64 x_ = 0.0;
    f64 y_ = 0.0;
    bool armed_ = false;
};

} // namespace wma::detail

#endif // WMA_BACKENDS_WINDOW_HIT_HPP
