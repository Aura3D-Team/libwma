#ifndef WMA_SRC_BACKENDS_SDL_FRAME_HIT_TEST_HPP
#define WMA_SRC_BACKENDS_SDL_FRAME_HIT_TEST_HPP

#include <memory>

#include <SDL3/SDL.h>

#include "wma/backends/WindowHit.hpp"

namespace wma::sdl
{

/**
 * @brief Bridges IWindowManager::setHitTest() to SDL.
 *
 * Borders go through SDL_SetWindowHitTest(), which resizes on the press. The
 * title bar needs a click to stay a click, so a double-click can maximize:
 *  - On Wayland SDL delivers caption presses, and this starts the compositor move
 *    once the pointer travels, with a press serial from its own wl_pointer. SDL
 *    releases its held button when the move takes the pointer away.
 *  - Elsewhere SDL moves on the press itself and reports it afterwards
 *    (SDL_EVENT_WINDOW_HIT_TEST); a window that then moved was dragged, not clicked.
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

    //! Once per poll: dispatches the private Wayland queue so it never grows.
    void pump();

    //! True when @p event belongs to the title bar and must not reach the application.
    bool consume(const SDL_Event &event, IWindowManager &window);

  private:
    struct WaylandSeats;

    static SDL_HitTestResult SDLCALL thunk(SDL_Window *window, const SDL_Point *point, void *data);

    [[nodiscard]] bool ownsWindow(SDL_WindowID id) const noexcept;
    void startMove();

    SDL_Window *window_;
    HitTest hitTest_;
    detail::CaptionGesture caption_;
    std::unique_ptr<WaylandSeats> wayland_;
    bool installed_ = false;
    bool releasePending_ = false;
};

//! False on SDL's Wayland driver when the compositor has no xdg-decoration: SDL's
//! frame there is libdecor's, which draws nothing without a plugin installed.
[[nodiscard]] bool serverDecorationsAvailable() noexcept;

} // namespace wma::sdl

#endif // WMA_SRC_BACKENDS_SDL_FRAME_HIT_TEST_HPP
