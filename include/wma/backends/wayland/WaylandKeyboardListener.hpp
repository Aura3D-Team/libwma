#ifndef WMA_BACKENDS_WAYLAND_KEYBOARD_LISTENER_HPP
#define WMA_BACKENDS_WAYLAND_KEYBOARD_LISTENER_HPP

#include "wma/input/keyboard/KeyboardListener.hpp"
#include <wayland-client.h>

//! Opaque xkb types, forward-declared so xkbcommon stays out of this header
//! and therefore off every consumer's include path.
struct xkb_context;
struct xkb_keymap;
struct xkb_state;
struct xkb_compose_table;
struct xkb_compose_state;

namespace wma {

struct WindowFlags;

/**
 * @class WaylandKeyboardListener
 *
 * @brief Wayland keyboard input, with layout-correct text via xkbcommon.
 *
 * Wayland deliberately ships no keycode-to-character mapping of its own: the
 * compositor hands each client a keymap and expects the client to do its own
 * translation. This listener builds an xkb keymap and state from that
 * handover, which is what turns a raw evdev keycode into text at all -- and,
 * through xkb's compose table, what makes dead keys and the Compose key work.
 */
class WaylandKeyboardListener : public KeyboardListener {
public:
    explicit WaylandKeyboardListener(WindowFlags* flags = nullptr);
    ~WaylandKeyboardListener() override;

    void initialize(wl_keyboard* keyboard);
    void detach() noexcept;
    void setWindowFlags(WindowFlags* flags) noexcept { windowFlags_ = flags; }
    wl_keyboard* getKeyboard() const { return keyboard_; }

    //! Bookkeeping only: there is no on-screen keyboard to raise here, and the
    //! keymap is live from the moment the compositor sends it.
    void setTextInputEnabled(bool enabled) noexcept { textInputEnabled_ = enabled; }
    [[nodiscard]] bool isTextInputEnabled() const noexcept { return textInputEnabled_; }

    void handleKeymap(u32 format, i32 fd, u32 size);
    void handleEnter(u32 serial, wl_surface* surface, wl_array* keys);
    void handleLeave(u32 serial, wl_surface* surface);
    void handleKey(u32 serial, u32 time, u32 key, u32 state);
    void handleModifiers(u32 serial, u32 mods_depressed, u32 mods_latched,
                         u32 mods_locked, u32 group);
    void handleRepeatInfo(i32 rate, i32 delay);

private:
    //! The key @p xkbKeycode names under the current keymap, falling back to
    //! its us-layout position while no keymap has arrived.
    [[nodiscard]] Key keyFor(u32 xkbKeycode) const;

    /**
     * @brief Runs @p keycode through the compose table, then emits its text.
     *
     * Compose is consulted first because a dead key must produce *nothing* on
     * its own and then the composed character on the following keystroke;
     * asking xkb for the raw UTF-8 first would type both halves.
     */
    void composeAndDispatchText(u32 keycode);

    void destroyXkb() noexcept;

    wl_keyboard* keyboard_ = nullptr;

    xkb_context* xkbContext_ = nullptr;
    xkb_keymap* xkbKeymap_ = nullptr;
    xkb_state* xkbState_ = nullptr;
    //! Null when the locale defines no compose sequences, which is not an
    //! error -- text then bypasses compose entirely.
    xkb_compose_table* composeTable_ = nullptr;
    xkb_compose_state* composeState_ = nullptr;

    WindowFlags* windowFlags_ = nullptr;
    bool textInputEnabled_ = false;

    static const wl_keyboard_listener keyboardListener_;

    static void handleKeymapCallback(void* data, wl_keyboard* keyboard,
                                     u32 format, i32 fd, u32 size);
    static void handleEnterCallback(void* data, wl_keyboard* keyboard,
                                    u32 serial, wl_surface* surface, wl_array* keys);
    static void handleLeaveCallback(void* data, wl_keyboard* keyboard,
                                    u32 serial, wl_surface* surface);
    static void handleKeyCallback(void* data, wl_keyboard* keyboard,
                                  u32 serial, u32 time, u32 key, u32 state);
    static void handleModifiersCallback(void* data, wl_keyboard* keyboard,
                                        u32 serial, u32 mods_depressed,
                                        u32 mods_latched, u32 mods_locked, u32 group);
    static void handleRepeatInfoCallback(void* data, wl_keyboard* keyboard,
                                         i32 rate, i32 delay);
};

} // namespace wma

#endif // WMA_BACKENDS_WAYLAND_KEYBOARD_LISTENER_HPP
