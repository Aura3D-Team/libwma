#ifndef WMA_BACKENDS_SDL_KEYBOARD_LISTENER_HPP
#define WMA_BACKENDS_SDL_KEYBOARD_LISTENER_HPP

#include "wma/input/keyboard/KeyboardListener.hpp"

struct SDL_Window;
struct SDL_KeyboardEvent;
struct SDL_TextInputEvent;

namespace wma
{

class SDLKeyboardListener : public KeyboardListener
{
  public:
    SDLKeyboardListener();
    ~SDLKeyboardListener() override = default;

    void initialize(SDL_Window *window);
    void handleKeyEvent(const SDL_KeyboardEvent &keyEvent);

    //! Decodes SDL's UTF-8 commit into codepoints and dispatches each.
    void handleTextInputEvent(const SDL_TextInputEvent &textEvent);

    /**
     * @brief Starts or stops SDL's text-input machinery for this window.
     *
     * Off by default, and deliberately: on Android and iOS SDL_StartTextInput()
     * raises the on-screen keyboard, which must not appear until something
     * actually wants typing. Desktop platforms behave the same either way, so
     * a caller that always enables it stays portable.
     */
    void setTextInputEnabled(bool enabled);

    [[nodiscard]] bool isTextInputEnabled() const noexcept
    {
        return textInputEnabled_;
    }

  private:
    SDL_Window *sdlWindow_ = nullptr;
    bool textInputEnabled_ = false;
};

} // namespace wma

#endif // WMA_BACKENDS_SDL_KEYBOARD_LISTENER_HPP
