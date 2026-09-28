#ifdef WMA_ENABLE_SDL
#include "wma/backends/sdl/SDLMouseListener.hpp"
#include "wma/core/Types.hpp"
#include "wma/exceptions/WMAException.hpp"

#include <SDL3/SDL.h>

namespace wma
{

SDLMouseListener::SDLMouseListener() : MouseListener(), sdlWindow_(nullptr)
{
}

void SDLMouseListener::initialize(SDL_Window *window)
{
    if (!window)
        throw InputException("Invalid SDL window pointer");

    sdlWindow_ = window;
    SDL_PropertiesID props = SDL_GetWindowProperties(window);
    SDL_SetPointerProperty(props, "MouseListener", this);

    float x, y;
    SDL_GetMouseState(&x, &y);
    currentPosition_ = WMAMousePosition(static_cast<f64>(x), static_cast<f64>(y));
    lastPosition_ = currentPosition_;
    firstMouse_ = true;
    updateCursorState();
}

void SDLMouseListener::handleEvent(const SDL_Event &event)
{
    switch (event.type)
    {
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
    {
        const i32 unifiedButton = convertButton(event.button.button);
        if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN)
        {
            dispatchButtonPress(unifiedButton);
        }
        else
        {
            dispatchButtonRelease(unifiedButton);
        }
        break;
    }
    case SDL_EVENT_MOUSE_MOTION:
    {
        f64 xpos = static_cast<f64>(event.motion.x);
        f64 ypos = static_cast<f64>(event.motion.y);

        if (firstMouse_)
        {
            // The first relative-motion event after enabling relative mode
            // can report a spurious warp-to-center delta; discard it rather
            // than rotate the camera on it.
            lastPosition_ = WMAMousePosition(xpos, ypos);
            firstMouse_ = false;
            break;
        }

        // xrel/yrel are SDL's own relative-mouse-mode deltas, not a diff of
        // two absolute positions: under relative mode the cursor is hidden
        // and confined/warped by the platform, so event.motion.x/y is not a
        // free, unbounded coordinate -- diffing it stalls at zero once the
        // platform stops moving the confined cursor further (rotation gets
        // stuck well short of a full look-around). xrel/yrel keep reporting
        // the true motion regardless.
        f64 deltaX = static_cast<f64>(event.motion.xrel) * sensitivity_;
        f64 deltaY = -static_cast<f64>(event.motion.yrel) * sensitivity_;
        currentPosition_ = WMAMousePosition(xpos, ypos, deltaX, deltaY);
        dispatchMove(currentPosition_);
        lastPosition_ = WMAMousePosition(xpos, ypos);
        break;
    }
    case SDL_EVENT_MOUSE_WHEEL:
    {
        WMAMouseScroll scroll(static_cast<f64>(event.wheel.x), static_cast<f64>(event.wheel.y));
        dispatchScroll(scroll);
        break;
    }
    default:
        break;
    }
}

void SDLMouseListener::updateCursorState()
{
    if (sdlWindow_)
    {
        if (cursorEnabled_)
        {
            SDL_ShowCursor();
            SDL_SetWindowRelativeMouseMode(sdlWindow_, false);
        }
        else
        {
            SDL_HideCursor();
            SDL_SetWindowRelativeMouseMode(sdlWindow_, true);
        }
    }
}

i32 SDLMouseListener::convertButton(i32 sdlButton) const
{
    switch (sdlButton)
    {
    case SDL_BUTTON_LEFT:
        return MouseButton::WMALeft;
    case SDL_BUTTON_RIGHT:
        return MouseButton::WMARight;
    case SDL_BUTTON_MIDDLE:
        return MouseButton::WMAMiddle;
    case SDL_BUTTON_X1:
        return MouseButton::WMAButton4;
    case SDL_BUTTON_X2:
        return MouseButton::WMAButton5;
    default:
        return sdlButton;
    }
}

} // namespace wma
#endif
