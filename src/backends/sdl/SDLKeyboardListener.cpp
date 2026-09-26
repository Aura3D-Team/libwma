#ifdef WMA_ENABLE_SDL
#include "wma/backends/sdl/SDLKeyboardListener.hpp"
#include "wma/exceptions/WMAException.hpp"
#include "wma/input/keyboard/Keys.h"
#include "wma/input/keyboard/Utf8.hpp"

#include <SDL3/SDL.h>

namespace wma
{

SDLKeyboardListener::SDLKeyboardListener() : KeyboardListener(), sdlWindow_(nullptr)
{
}

void SDLKeyboardListener::initialize(SDL_Window *window)
{
    if (!window)
    {
        throw InputException("Invalid SDL window pointer");
    }
    sdlWindow_ = window;
    SDL_PropertiesID props = SDL_GetWindowProperties(window);
    SDL_SetPointerProperty(props, "KeyboardListener", this);
}

void SDLKeyboardListener::handleKeyEvent(const SDL_KeyboardEvent &keyEvent)
{
    const Key mappedKey = mapSDLKey(keyEvent.key);
    if (keyEvent.type == SDL_EVENT_KEY_DOWN)
    {
        //! SDL sets `repeat` on auto-repeated presses; passing it through is
        //! what lets held Backspace keep deleting without a bound action
        //! re-firing. See KeyboardListener::dispatchKeyPress.
        dispatchKeyPress(mappedKey, keyEvent.repeat);
    }
    else if (keyEvent.type == SDL_EVENT_KEY_UP)
    {
        dispatchKeyRelease(mappedKey);
    }
}

void SDLKeyboardListener::handleTextInputEvent(const SDL_TextInputEvent &textEvent)
{
    if (!textEvent.text)
        return;

    /*
     * One event can carry several codepoints: an IME commits a whole phrase at
     * once, and even a plain compose sequence produces a multi-byte string.
     * Each decoded scalar value is dispatched separately, so a caller only
     * ever handles one character at a time.
     */
    utf8::decode(textEvent.text,
                 [this](Codepoint codepoint)
                 {
                     dispatchText(codepoint);
                 });
}

void SDLKeyboardListener::setTextInputEnabled(bool enabled)
{
    if (!sdlWindow_ || enabled == textInputEnabled_)
        return;

    if (enabled)
        SDL_StartTextInput(sdlWindow_);
    else
        SDL_StopTextInput(sdlWindow_);

    textInputEnabled_ = enabled;
}

} // namespace wma
#endif
