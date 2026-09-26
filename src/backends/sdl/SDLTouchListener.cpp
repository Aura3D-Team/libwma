#ifdef WMA_ENABLE_SDL
#include "wma/backends/sdl/SDLTouchListener.hpp"

#include <SDL3/SDL.h>

namespace wma
{

void SDLTouchListener::initialize(SDL_Window *window)
{
    sdlWindow_ = window;
    activeFingers_.clear();
}

void SDLTouchListener::handleEvent(const SDL_Event &event)
{
    switch (event.type)
    {
    case SDL_EVENT_FINGER_DOWN:
    case SDL_EVENT_FINGER_MOTION:
    case SDL_EVENT_FINGER_UP:
    {
        // SDL reports touch positions normalized to 0..1; convert to window
        // pixels so callers can share coordinate math with the mouse path
        // (see WMATouchPoint's doc comment).
        int windowW = 0;
        int windowH = 0;
        if (sdlWindow_)
            SDL_GetWindowSize(sdlWindow_, &windowW, &windowH);

        const f64 x = static_cast<f64>(event.tfinger.x) * static_cast<f64>(windowW);
        const f64 y = static_cast<f64>(event.tfinger.y) * static_cast<f64>(windowH);
        const auto fingerId = static_cast<TouchFingerId>(event.tfinger.fingerID);

        if (event.type == SDL_EVENT_FINGER_DOWN)
        {
            WMATouchPoint point(fingerId, x, y);
            activeFingers_[fingerId] = point;
            dispatchDown(point);
            break;
        }

        // Derive the delta from this finger's own previous position. A finger
        // we never saw go down (e.g. one already held when bindings were
        // installed) starts with a zero delta rather than a jump from 0,0.
        f64 deltaX = 0.0;
        f64 deltaY = 0.0;
        if (const auto it = activeFingers_.find(fingerId); it != activeFingers_.end())
        {
            deltaX = x - it->second.x;
            //! Y-up, matching WMAMousePosition (SDL's y grows downward).
            deltaY = it->second.y - y;
        }

        const WMATouchPoint point(fingerId, x, y, deltaX, deltaY);

        if (event.type == SDL_EVENT_FINGER_UP)
        {
            activeFingers_.erase(fingerId);
            dispatchUp(point);
        }
        else
        {
            activeFingers_[fingerId] = WMATouchPoint(fingerId, x, y, deltaX, deltaY);
            dispatchMove(point);
        }
        break;
    }
    default:
        break;
    }
}

} // namespace wma

#endif // WMA_ENABLE_SDL
