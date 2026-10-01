#ifndef WMA_BACKENDS_SDL_MOUSE_LISTENER_HPP
#define WMA_BACKENDS_SDL_MOUSE_LISTENER_HPP

#include <array>

#include "wma/input/mouse/MouseListener.hpp"

struct SDL_Cursor;
struct SDL_Window;
union SDL_Event;

namespace wma
{

class SDLMouseListener : public MouseListener
{
  public:
    SDLMouseListener();
    ~SDLMouseListener() override = default;

    void initialize(SDL_Window *window);
    void handleEvent(const SDL_Event &event);
    //! SDL_Quit() frees every cursor, so the window manager calls this first.
    void releaseCursors() noexcept;

  protected:
    void updateCursorState() override;

  private:
    SDL_Window *sdlWindow_ = nullptr;
    std::array<SDL_Cursor *, SYSTEM_CURSOR_COUNT> cursors_{};
    i32 convertButton(i32 sdlButton) const;
};

} // namespace wma

#endif // WMA_BACKENDS_SDL_MOUSE_LISTENER_HPP
