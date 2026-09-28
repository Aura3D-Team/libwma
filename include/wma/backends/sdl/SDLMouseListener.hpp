#ifndef WMA_BACKENDS_SDL_MOUSE_LISTENER_HPP
#define WMA_BACKENDS_SDL_MOUSE_LISTENER_HPP

#include "wma/input/mouse/MouseListener.hpp"

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

  protected:
    void updateCursorState() override;

  private:
    SDL_Window *sdlWindow_ = nullptr;
    i32 convertButton(i32 sdlButton) const;
};

} // namespace wma

#endif // WMA_BACKENDS_SDL_MOUSE_LISTENER_HPP
