#ifndef WMA_SRC_BACKENDS_SDL_FRAME_HIT_TEST_HPP
#define WMA_SRC_BACKENDS_SDL_FRAME_HIT_TEST_HPP

#include <memory>

#include <SDL3/SDL.h>

#include "wma/backends/WindowHit.hpp"

namespace wma::sdl
{

/**
 * @brief Bridges IWindowManager::setHitTest() to SDL_SetWindowHitTest().
 *
 * SDL moves or resizes from the hovered region and swallows that press, so the
 * application never sees the first click of a title-bar double-click. This
 * watches for it -- SDL_EVENT_WINDOW_HIT_TEST on X11, a private wl_pointer on
 * Wayland, which SDL does not notify -- and lets a second click on the same
 * spot through as an ordinary press, which then toggles maximize.
 *
 * Heap-owned by the window manager so the address SDL calls back with survives
 * moving the manager.
 */
class FrameHitTest
{
  public:
    FrameHitTest(SDL_Window *window, HitTest hitTest);
    ~FrameHitTest();

    FrameHitTest(const FrameHitTest &) = delete;
    FrameHitTest &operator=(const FrameHitTest &) = delete;

    //! False where SDL has no hit testing (the browser, Android).
    [[nodiscard]] bool installed() const noexcept
    {
        return installed_;
    }

    //! After SDL_PumpEvents(): records title-bar presses read from the compositor.
    void pump();

    //! True when @p event belongs to a title-bar double-click and must not reach
    //! the application; the second click has then toggled @p window's maximize.
    bool consume(const SDL_Event &event, IWindowManager &window);

  private:
    struct WaylandPresses;

    static SDL_HitTestResult SDLCALL thunk(SDL_Window *window, const SDL_Point *point, void *data);

    void captionPressed(f64 x, f64 y);

    SDL_Window *window_;
    HitTest hitTest_;
    detail::CaptionClicks clicks_;
    std::unique_ptr<WaylandPresses> wayland_;
    bool installed_ = false;
    bool releasePending_ = false;
};

} // namespace wma::sdl

#endif // WMA_SRC_BACKENDS_SDL_FRAME_HIT_TEST_HPP
