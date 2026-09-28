#ifndef WMA_BACKENDS_SDL_TOUCH_LISTENER_HPP
#define WMA_BACKENDS_SDL_TOUCH_LISTENER_HPP

#include <unordered_map>

#include "wma/input/touch/TouchListener.hpp"

struct SDL_Window;
union SDL_Event;

namespace wma
{

class SDLTouchListener : public TouchListener
{
  public:
    SDLTouchListener() = default;
    ~SDLTouchListener() override = default;

    void initialize(SDL_Window *window);
    void handleEvent(const SDL_Event &event);

  private:
    SDL_Window *sdlWindow_ = nullptr;

    //! SDL reports absolute normalized positions per finger, not deltas, so
    //! keep each active finger's last position to derive them. Keyed by
    //! SDL_FingerID; entries are erased on finger-up so a recycled id never
    //! inherits a stale origin.
    std::unordered_map<TouchFingerId, WMATouchPoint> activeFingers_;
};

} // namespace wma

#endif // WMA_BACKENDS_SDL_TOUCH_LISTENER_HPP
