#include "wma/backends/wayland/WaylandKeyboardListener.hpp"
#include "wma/input/keyboard/Keys.h"
#include "wma/input/keyboard/Utf8.hpp"
#include "wma/exceptions/WMAException.hpp"

#include "wma/core/WindowFlags.hpp"

#include <array>
#include <cstdlib>
#include <string_view>
#include <vector>

#include <unistd.h>
#include <sys/mman.h>
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-compose.h>

namespace wma {

const wl_keyboard_listener WaylandKeyboardListener::keyboardListener_ = {
    .keymap = handleKeymapCallback,
    .enter = handleEnterCallback,
    .leave = handleLeaveCallback,
    .key = handleKeyCallback,
    .modifiers = handleModifiersCallback,
    .repeat_info = handleRepeatInfoCallback
};

WaylandKeyboardListener::WaylandKeyboardListener(WindowFlags* flags)
    : KeyboardListener()
    , keyboard_(nullptr)
    , windowFlags_(flags)
{
}

WaylandKeyboardListener::~WaylandKeyboardListener()
{
    detach();
}

void WaylandKeyboardListener::detach() noexcept
{
    //! The window manager owns the seat proxy; this listener only borrows it.
    keyboard_ = nullptr;
    releaseAllKeys();
    destroyXkb();
}

void WaylandKeyboardListener::destroyXkb() noexcept
{
    //! Reverse construction order: the states reference the tables they were
    //! built from, and both reference the context.
    if (composeState_) 
    {
        xkb_compose_state_unref(composeState_);
        composeState_ = nullptr;
    }
    if (composeTable_) 
    {
        xkb_compose_table_unref(composeTable_);
        composeTable_ = nullptr;
    }
    if (xkbState_) 
    {
        xkb_state_unref(xkbState_);
        xkbState_ = nullptr;
    }
    if (xkbKeymap_) 
    {
        xkb_keymap_unref(xkbKeymap_);
        xkbKeymap_ = nullptr;
    }
    if (xkbContext_) 
    {
        xkb_context_unref(xkbContext_);
        xkbContext_ = nullptr;
    }
}

void WaylandKeyboardListener::initialize(wl_keyboard* keyboard)
{
    if (!keyboard) {
        throw InputException("Invalid Wayland keyboard pointer");
    }

    //! Idempotent: the seat's capabilities callback subscribes as soon as the
    //! keyboard exists, and setupInputDevices() still calls this for the case
    //! where it has not fired yet. libwayland rejects a second listener on the
    //! same proxy, so the repeat has to be absorbed here.
    if (keyboard_ == keyboard)
        return;

    keyboard_ = keyboard;
    wl_keyboard_add_listener(keyboard_, &keyboardListener_, this);
}

void WaylandKeyboardListener::handleKeymap(u32 format, i32 fd, u32 size)
{
    /*
     * The fd belongs to us the moment it arrives, so it is closed on every
     * path out of here -- including the early ones. Compositors resend the
     * keymap whenever the layout changes, so a leak here is per-switch rather
     * than one-off.
     */
    if (fd < 0)
        return;

    if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) 
    {
        close(fd);
        return;
    }

    /*
     * MAP_PRIVATE, not MAP_SHARED: the keymap is the compositor's memory and
     * every client maps the same object. A shared mapping would let one
     * client's write corrupt every other client's keymap, which is why the
     * protocol requires private mapping here.
     */
    void* mapped = mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (mapped == MAP_FAILED) 
    {
        close(fd);
        return;
    }

    //! A layout switch resends the keymap, so whatever was built from the
    //! previous one is torn down before being replaced.
    destroyXkb();

    xkbContext_ = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    if (!xkbContext_) 
    {
        munmap(mapped, size);
        close(fd);
        return;
    }

    xkbKeymap_ = xkb_keymap_new_from_string(xkbContext_,
                                            static_cast<const char*>(mapped),
                                            XKB_KEYMAP_FORMAT_TEXT_V1,
                                            XKB_KEYMAP_COMPILE_NO_FLAGS);

    munmap(mapped, size);
    close(fd);

    if (!xkbKeymap_) 
    {
        destroyXkb();
        return;
    }

    xkbState_ = xkb_state_new(xkbKeymap_);
    if (!xkbState_) 
    {
        destroyXkb();
        return;
    }

    /*
     * Compose is a per-locale table rather than part of the keymap: it is what
     * turns a dead key followed by a letter into a single accented character.
     * Its absence is normal -- a locale may define no sequences -- so a failure
     * here leaves composeState_ null and text simply bypasses compose.
     */
    const char* locale = std::getenv("LC_ALL");
    if (!locale || *locale == '\0') 
        locale = std::getenv("LC_CTYPE");
    if (!locale || *locale == '\0') 
        locale = std::getenv("LANG");
    if (!locale || *locale == '\0') 
        locale = "C";

    composeTable_ = xkb_compose_table_new_from_locale(xkbContext_, locale,
                                                      XKB_COMPOSE_COMPILE_NO_FLAGS);
    if (composeTable_)
        composeState_ = xkb_compose_state_new(composeTable_, XKB_COMPOSE_STATE_NO_FLAGS);
}

void WaylandKeyboardListener::handleEnter(u32, wl_surface*, wl_array*)
{
    if (windowFlags_)
        windowFlags_->focused = true;
}

void WaylandKeyboardListener::handleLeave(u32, wl_surface*)
{
    if (windowFlags_)
        windowFlags_->focused = false;
    //! Releases that land while another surface holds focus never reach us, so
    //! anything still marked held would stay stuck down -- most visibly a
    //! modifier held across a focus switch.
    releaseAllKeys();
}

namespace {

/**
 * @brief Runs an xkbcommon UTF-8 getter and decodes exactly what it produced.
 *
 * Both xkb_state_key_get_utf8 and xkb_compose_state_get_utf8 follow snprintf:
 * they truncate to the buffer and return the length the string *would* have
 * needed. Taking that return value as the size of what landed in the buffer
 * reads past its end on any commit longer than it -- an IME phrase, or a
 * compose sequence with a multi-character result -- which is undefined
 * behaviour, so the length is clamped and an oversized string is re-fetched
 * into an exactly-sized buffer rather than being cut off mid-sequence.
 *
 * @param fill Invoked as fill(char*, usize) -> int, the xkb getter's shape.
 * @param emit Receives each decoded scalar value.
 */
template <typename Fill, typename Emit>
void decodeXkbUtf8(Fill&& fill, Emit&& emit)
{
    std::array<char, 64> stackBuffer{};

    const int required = fill(stackBuffer.data(), stackBuffer.size());
    if (required <= 0)
        return;

    const auto length = static_cast<usize>(required);

    //! Strictly less: the getter spends one byte of the buffer on the NUL.
    if (length < stackBuffer.size()) {
        utf8::decode(std::string_view{stackBuffer.data(), length}, emit);
        return;
    }

    std::vector<char> heapBuffer(length + 1u, '\0');
    const int written = fill(heapBuffer.data(), heapBuffer.size());
    if (written <= 0)
        return;

    utf8::decode(std::string_view{heapBuffer.data(),
                                  (static_cast<usize>(written) < length)
                                      ? static_cast<usize>(written)
                                      : length},
                 emit);
}

} // namespace

void WaylandKeyboardListener::composeAndDispatchText(u32 keycode)
{
    if (!xkbState_)
        return;

    const xkb_keysym_t keysym = xkb_state_key_get_one_sym(xkbState_, keycode);

    /*
     * Compose is consulted before any text is produced. A dead key must emit
     * nothing on its own and then the composed character on the keystroke that
     * follows; asking xkb for this key's raw UTF-8 first would type both the
     * accent and the base letter.
     */
    if (composeState_ && keysym != XKB_KEY_NoSymbol)
    {
        xkb_compose_state_feed(composeState_, keysym);

        switch (xkb_compose_state_get_status(composeState_))
        {
            case XKB_COMPOSE_COMPOSING:
                //! Mid-sequence: swallow the keystroke entirely.
                return;

            case XKB_COMPOSE_COMPOSED: {
                //! Reset only after the text is out: the retry inside
                //! decodeXkbUtf8 needs the composed state still standing.
                decodeXkbUtf8(
                    [this](char* buffer, usize size) {
                        return xkb_compose_state_get_utf8(composeState_, buffer, size);
                    },
                    [this](Codepoint codepoint) { dispatchText(codepoint); });

                xkb_compose_state_reset(composeState_);
                return;
            }

            case XKB_COMPOSE_CANCELLED:
                //! An invalid sequence. Reset and fall through, so the key that
                //! broke the sequence still types itself.
                xkb_compose_state_reset(composeState_);
                break;

            case XKB_COMPOSE_NOTHING:
            default:
                break;
        }
    }

    decodeXkbUtf8(
        [this, keycode](char* buffer, usize size) {
            return xkb_state_key_get_utf8(xkbState_, keycode, buffer, size);
        },
        [this](Codepoint codepoint) { dispatchText(codepoint); });
}

Key WaylandKeyboardListener::keyFor(u32 xkbKeycode) const
{
    /*
     * The compositor's keymap decides, not the keycode's position. A position
     * table is right only on a us layout: on Dvorak or AZERTY it names the
     * wrong key, and on a virtual keyboard -- which assigns whatever keysyms
     * it likes to whatever keycodes it likes -- keycode 1 is not Escape at
     * all. Falling back to the position keeps the pre-keymap window usable.
     */
    if (!xkbState_)
        return mapWaylandKey(xkbKeycode);

    const xkb_keysym_t keysym = xkb_state_key_get_one_sym(xkbState_, xkbKeycode);
    const Key named = mapWaylandKeysym(static_cast<u32>(keysym));

    //! A keysym this table does not name -- a media key, a layout's own
    //! symbol -- still has a position, and a caller binding physical keys is
    //! better served by that than by KEY_UNKNOWN.
    return named != Key::KEY_UNKNOWN ? named : mapWaylandKey(xkbKeycode);
}

void WaylandKeyboardListener::handleKey(u32, u32, u32 key, u32 state)
{
    //! Wayland reports evdev keycodes; xkb numbers the same keys 8 higher.
    const u32 xkbKeycode = key + 8;
    const Key mappedKey = keyFor(xkbKeycode);

    if (state == WL_KEYBOARD_KEY_STATE_PRESSED) {
        /*
         * Wayland sends no repeat events of its own: repeat_info carries the
         * rate and delay and the client is expected to synthesize them. Nothing
         * does yet, so every press that arrives here is a genuine one.
         */
        dispatchKeyPress(mappedKey, /*repeat=*/false);
        composeAndDispatchText(xkbKeycode);
    } else if (state == WL_KEYBOARD_KEY_STATE_RELEASED) {
        dispatchKeyRelease(mappedKey);
    }
}

void WaylandKeyboardListener::handleModifiers(u32, u32 mods_depressed, u32 mods_latched,
                                              u32 mods_locked, u32 group)
{
    /*
     * Feeding this back into xkb is what makes Shift and AltGr produce the
     * right characters: xkb_state_key_get_utf8() resolves a keycode through
     * the modifier state held here, so without it every key would type its
     * unshifted form. KeyboardListener::modifiers() is derived from the
     * press/release stream instead and needs nothing from this.
     */
    if (xkbState_)
    {
        xkb_state_update_mask(xkbState_,
                              mods_depressed, mods_latched, mods_locked,
                              0, 0, group);
    }
}

void WaylandKeyboardListener::handleRepeatInfo(i32, i32)
{
}

void WaylandKeyboardListener::handleKeymapCallback(void* data, wl_keyboard*,
                                                   u32 format, i32 fd, u32 size)
{
    auto* listener = static_cast<WaylandKeyboardListener*>(data);
    if (listener) 
    {
        listener->handleKeymap(format, fd, size);
    } 
    else if (fd >= 0) 
    {
        //! Still our descriptor to close, even with nowhere to put the keymap.
        close(fd);
    }
}

void WaylandKeyboardListener::handleEnterCallback(void* data, wl_keyboard*,
                                                  u32 serial, wl_surface* surface, wl_array* keys)
{
    auto* listener = static_cast<WaylandKeyboardListener*>(data);
    if (listener) listener->handleEnter(serial, surface, keys);
}

void WaylandKeyboardListener::handleLeaveCallback(void* data, wl_keyboard*,
                                                  u32 serial, wl_surface* surface)
{
    auto* listener = static_cast<WaylandKeyboardListener*>(data);
    if (listener) listener->handleLeave(serial, surface);
}

void WaylandKeyboardListener::handleKeyCallback(void* data, wl_keyboard*,
                                                u32 serial, u32 time,
                                                u32 key, u32 state)
{
    auto* listener = static_cast<WaylandKeyboardListener*>(data);
    if (listener) listener->handleKey(serial, time, key, state);
}

void WaylandKeyboardListener::handleModifiersCallback(void* data, wl_keyboard*,
                                                      u32 serial, u32 mods_depressed,
                                                      u32 mods_latched, u32 mods_locked,
                                                      u32 group)
{
    auto* listener = static_cast<WaylandKeyboardListener*>(data);
    if (listener) listener->handleModifiers(serial, mods_depressed, mods_latched, mods_locked, group);
}

void WaylandKeyboardListener::handleRepeatInfoCallback(void* data, wl_keyboard*,
                                                       i32 rate, i32 delay)
{
    auto* listener = static_cast<WaylandKeyboardListener*>(data);
    if (listener) listener->handleRepeatInfo(rate, delay);
}

} // namespace wma
