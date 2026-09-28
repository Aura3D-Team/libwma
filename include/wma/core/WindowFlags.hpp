#ifndef WMA_CORE_WINDOW_FLAGS_HPP
#define WMA_CORE_WINDOW_FLAGS_HPP

#include <ink/ink_base.hpp>

namespace wma
{

/**
 * @brief Runtime flags and state information for the window
 */
struct WindowFlags
{
    bool resized;
    bool minimized;
    bool focused;
    f64 deltaTime;
    f64 fps;

    //! Set when the platform's underlying native surface was torn down and
    //! replaced with a different one -- distinct from `resized` (same
    //! surface, new size). Android does this around Activity
    //! backgrounding/foregrounding: the app process and SDL_Window both
    //! survive, but the ANativeWindow identity does not. A renderer must
    //! rebuild its surface object (not just its swapchain) in response, or
    //! it is holding a handle to something the platform already destroyed.
    //! Sticky like `resized`: sees repeated churn as one bit until a poller
    //! clears it.
    bool surfaceLost;

    WindowFlags() : resized(false), minimized(false), focused(true), deltaTime(0), fps(0.0), surfaceLost(false)
    {
    }
};

} // namespace wma

#endif // WMA_CORE_WINDOW_FLAGS_HPP
