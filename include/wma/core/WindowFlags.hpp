#ifndef WMA_CORE_WINDOW_FLAGS_HPP
#define WMA_CORE_WINDOW_FLAGS_HPP

#include <utility>

#include <ink/ink_base.hpp>

namespace wma
{

/**
 * @brief Runtime flags and state information for the window
 */
struct WindowFlags
{
    //! How many times the window's size has changed, never reset: a resize, maximize, restore, fullscreen toggle,
    //! decoration drag or pixel-density change each counts. A counter rather than a flag, so no reader can clear
    //! the event before another sees it; each keeps the count it last handled, which is what ResizeWatch does.
    u64 resizeCount;
    //! How many times the platform discarded what the window showed and asked for it to be drawn again, never
    //! reset: a window restored from minimized, or uncovered where nothing kept its pixels (X11 without a
    //! compositor). A renderer that only draws on change must draw after one; ExposeWatch tells it.
    u64 exposeCount;
    bool minimized;
    bool focused;
    f64 deltaTime;
    f64 fps;

    //! Set when the platform's underlying native surface was torn down and
    //! replaced with a different one -- distinct from a resize (same
    //! surface, new size). Android does this around Activity
    //! backgrounding/foregrounding: the app process and SDL_Window both
    //! survive, but the ANativeWindow identity does not. A renderer must
    //! rebuild its surface object (not just its swapchain) in response, or
    //! it is holding a handle to something the platform already destroyed.
    //! Sticky: sees repeated churn as one bit until a poller clears it.
    bool surfaceLost;

    //! The platform closed the on-screen keyboard itself (Android's back gesture), so text input is off though the
    //! app never asked. Sticky until a poller clears it; the app should stop editing, or it would re-open at once.
    bool textInputDismissed;

    WindowFlags()
        : resizeCount(0), exposeCount(0), minimized(false), focused(true), deltaTime(0), fps(0.0), surfaceLost(false),
          textInputDismissed(false)
    {
    }

    //! Backends call this whenever the window's size or its size in pixels changes.
    void markResized() noexcept
    {
        ++resizeCount;
    }
    //! Backends call this when the platform asks for the window's contents to be drawn again.
    void markExposed() noexcept
    {
        ++exposeCount;
    }
};

/**
 * @brief One reader's view of a WindowFlags event counter (resizeCount, exposeCount).
 *
 * Each party that reacts to the event -- the renderer, a UI, the app -- owns one, so each sees every event whatever
 * the others do. A burst of events between two checks reads as one.
 *
 * @code
 * wma::ResizeWatch resizes;
 * window->process([&] {
 *     if (resizes.consume(*window->getWindowFlags()))
 *         relayout(*window->getWindowDetails());
 * });
 * @endcode
 */
template <u64 WindowFlags::*Counter> class CounterWatch
{
  public:
    //! Starts behind every event already counted: the first consume() after one returns true.
    CounterWatch() noexcept = default;

    //! Starts at @p flags' count: only events after this point are reported.
    explicit CounterWatch(const WindowFlags &flags) noexcept : seen_(flags.*Counter)
    {
    }

    //! True when the event happened since this watch last consumed; marks those events handled.
    [[nodiscard]] bool consume(const WindowFlags &flags) noexcept
    {
        return std::exchange(seen_, flags.*Counter) != flags.*Counter;
    }

    //! The same question, marking nothing handled.
    [[nodiscard]] bool pending(const WindowFlags &flags) const noexcept
    {
        return seen_ != flags.*Counter;
    }

  private:
    u64 seen_ = 0;
};

using ResizeWatch = CounterWatch<&WindowFlags::resizeCount>;
using ExposeWatch = CounterWatch<&WindowFlags::exposeCount>;

} // namespace wma

#endif // WMA_CORE_WINDOW_FLAGS_HPP
