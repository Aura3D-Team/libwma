#ifndef FRAMETIMER_H
#define FRAMETIMER_H

#include <chrono>
#include <ink/ink_base.hpp>

#ifndef __EMSCRIPTEN__
#include <thread>
#endif

#include "WindowFlags.hpp"

//! Clamp for the smallest measurable frame time (ms) so 1000/dt never divides by 0.
#define LIMIT_TARGET_FPS_TOLERANCE 1e-5
//! When within this many ms of the target, stop sleeping and busy-spin for accuracy.
#define LIMIT_SPIN_LOOP_DURATION 2e-3

namespace wma
{

/**
 * @brief Measures per-frame delta time / FPS and (on desktop) paces the loop.
 *
 * On WASM endFrame() only updates timing -- pacing is compiled out since the
 * browser already paces the loop via requestAnimationFrame and sleeping/spinning
 * the single browser thread would freeze the tab.
 */
class FrameTimer
{
  public:
    using clock = std::chrono::steady_clock;

    explicit FrameTimer(WindowFlags &wFlags) noexcept
        : windowFlags_(wFlags), lastFrameStart_(clock::now()), frameStart_(lastFrameStart_)
    {
    }

    void setTargetFPS(u32 fps) noexcept
    {
        targetFrameTime_ = std::chrono::duration<f64, std::milli>(fps > 0 ? 1000.0 / fps : 0.0);
    }

    //! Mark the start of a frame. Call before polling/updating.
    void beginFrame() noexcept
    {
        frameStart_ = clock::now();
    }

    //! Close the frame: update deltaTime/fps and (off WASM) pace the loop.
    void endFrame()
    {
        std::chrono::duration<f64, std::milli> elapsed = frameStart_ - lastFrameStart_;

        // Instantaneous delta time (needed every frame for physics/movement)
        windowFlags_.deltaTime = INK_MAX(elapsed.count(), LIMIT_TARGET_FPS_TOLERANCE);
        lastFrameStart_ = frameStart_;

        // Accumulate for smoothed FPS (updates every 500ms)
        accumulatedTime_ += windowFlags_.deltaTime;
        frameCount_++;

        if (accumulatedTime_ >= 500.0)
        {
            windowFlags_.fps = (frameCount_ * 1000.0) / accumulatedTime_;

            // Reset for the next 0.5s window
            accumulatedTime_ = 0.0;
            frameCount_ = 0;
        }

        // Pacing logic
#ifdef __EMSCRIPTEN__
        return; // the browser paces via requestAnimationFrame
#else

        if (targetFrameTime_.count() <= 0.0)
            return;

        std::chrono::duration<f64, std::milli> remaining = targetFrameTime_ - (clock::now() - frameStart_);

        if (remaining.count() > LIMIT_SPIN_LOOP_DURATION)
            std::this_thread::sleep_for(remaining - std::chrono::duration<f64, std::milli>(LIMIT_SPIN_LOOP_DURATION));

        while ((clock::now() - frameStart_) < targetFrameTime_)
        {
            //! Busy-wait the final sub-millisecond for frame-time accuracy.
        }
#endif
    }

  private:
    WindowFlags &windowFlags_;
    std::chrono::duration<f64, std::milli> targetFrameTime_{0.0}; //!< 0 => unlimited
    clock::time_point lastFrameStart_;
    clock::time_point frameStart_;

    // Smoothing metrics
    f64 accumulatedTime_{0.0};
    u32 frameCount_{0};
};

} // namespace wma

#endif // FRAMETIMER_H