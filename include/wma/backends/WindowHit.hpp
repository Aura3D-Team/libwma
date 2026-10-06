#ifndef WMA_BACKENDS_WINDOW_HIT_HPP
#define WMA_BACKENDS_WINDOW_HIT_HPP

#include <chrono>
#include <cmath>
#include <utility>

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

/**
 * @brief A title-bar press, from button-down to the move or click it becomes.
 *
 * The press turns into a native move only once the pointer has travelled, the
 * way GTK and Windows do it. A release before that is a click, and only clicks
 * can make a double-click: after a drag the pointer sits on the same spot of the
 * moved window, so arming on the press made "drag, then grab again" maximize.
 * Interval and slop match AuraUI's own double-click.
 */
class CaptionGesture
{
  public:
    //! True when this press completes a double-click; otherwise a move is pending.
    [[nodiscard]] bool press(f64 x, f64 y) noexcept
    {
        if (armedNear(x, y))
        {
            armed_ = false;
            pending_ = false;
            return true;
        }
        pending_ = true;
        pressX_ = x;
        pressY_ = y;
        pressedAt_ = Clock::now();
        return false;
    }

    //! True once, when a pending press has travelled far enough to start the move.
    [[nodiscard]] bool moved(f64 x, f64 y) noexcept
    {
        if (!pending_ || (std::abs(x - pressX_) < kDragThreshold && std::abs(y - pressY_) < kDragThreshold))
            return false;
        pending_ = false;
        armed_ = false;
        return true;
    }

    //! A release while the press is still pending was a click.
    void released() noexcept
    {
        if (std::exchange(pending_, false))
            clicked(pressX_, pressY_, pressedAt_);
    }

    //! For SDL's own hit test, which moves on the press and reports it afterwards.
    void clicked(f64 x, f64 y) noexcept
    {
        clicked(x, y, Clock::now());
    }

    void cancel() noexcept
    {
        pending_ = false;
        armed_ = false;
    }

    [[nodiscard]] bool pending() const noexcept
    {
        return pending_;
    }

    [[nodiscard]] bool armedNear(f64 x, f64 y) const noexcept
    {
        return armed_ && Clock::now() - armedAt_ <= kInterval && std::abs(x - armedX_) <= kSlop &&
               std::abs(y - armedY_) <= kSlop;
    }

  private:
    using Clock = std::chrono::steady_clock;
    static constexpr auto kInterval = std::chrono::milliseconds(400);
    static constexpr f64 kSlop = 5.0;
    static constexpr f64 kDragThreshold = 4.0;

    void clicked(f64 x, f64 y, Clock::time_point at) noexcept
    {
        armed_ = true;
        armedAt_ = at;
        armedX_ = x;
        armedY_ = y;
    }

    Clock::time_point pressedAt_{};
    Clock::time_point armedAt_{};
    f64 pressX_ = 0.0;
    f64 pressY_ = 0.0;
    f64 armedX_ = 0.0;
    f64 armedY_ = 0.0;
    bool pending_ = false;
    bool armed_ = false;
};

} // namespace wma::detail

#endif // WMA_BACKENDS_WINDOW_HIT_HPP
