/*
 * The Wayland keyboard's translation from an evdev keycode to a wma::Key.
 *
 * A compositor hands every client its own keymap, and the client is expected
 * to translate through it. A client that instead reads the keycode as a fixed
 * QWERTY position gets the right answer on a us layout and the wrong one on
 * every other -- and on a virtual keyboard, which assigns whatever keysyms it
 * likes to whatever keycodes it likes, it gets nonsense.
 *
 * xkbcommon compiles a keymap from a string, so the whole path is testable
 * with no compositor and no seat.
 */

#include <cstdio>
#include <cstring>
#include <string>

#include <sys/mman.h>
#include <unistd.h>

#include <wayland-client-protocol.h>

#include "wma/backends/wayland/WaylandKeyboardListener.hpp"
#include "wma/input/keyboard/KeyEventCallback.hpp"

namespace {

int failures = 0;

void check(bool condition, const char* description)
{
    std::printf("[%s] %s\n", condition ? "PASS" : "FAIL", description);
    if (!condition)
        ++failures;
}

/**
 * @brief A keymap whose Escape *position* types the letter a.
 *
 * Deliberately perverse, and exactly what a virtual keyboard produces: wtype
 * builds a keymap holding only the symbols it is about to send and assigns
 * them from the lowest keycodes up, so keycode 9 -- the one a us layout calls
 * Escape -- is whatever character came first.
 */
constexpr const char* kKeymap = R"(xkb_keymap {
  xkb_keycodes { minimum = 8; maximum = 12;
    <K9>  = 9;
    <K10> = 10;
  };
  xkb_types { include "basic" };
  xkb_compat { include "basic" };
  xkb_symbols {
    key <K9>  { [ a ] };
    key <K10> { [ Escape ] };
  };
};)";

/// Hands @p text to the listener the way a compositor does: through a
/// read-only file descriptor plus its size.
void sendKeymap(wma::WaylandKeyboardListener& listener, const char* text)
{
    const std::string keymap(text);
    const usize size = keymap.size() + 1;

    const int fd = ::memfd_create("wma-test-keymap", MFD_CLOEXEC);
    if (fd < 0)
    {
        check(false, "memfd_create for the keymap");
        return;
    }

    if (::ftruncate(fd, static_cast<off_t>(size)) != 0)
    {
        ::close(fd);
        check(false, "sizing the keymap descriptor");
        return;
    }

    void* mapped = ::mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (mapped == MAP_FAILED)
    {
        ::close(fd);
        check(false, "mapping the keymap descriptor");
        return;
    }

    std::memcpy(mapped, keymap.c_str(), size);
    ::munmap(mapped, size);

    //! handleKeymap() takes ownership of the descriptor, as the callback does.
    listener.handleKeymap(WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1, fd, static_cast<u32>(size));
}

/// The key the listener reports for evdev code @p evdev, or KEY_UNKNOWN.
[[nodiscard]] wma::Key pressed(wma::WaylandKeyboardListener& listener, u32 evdev)
{
    wma::Key seen = wma::KEY_UNKNOWN;

    listener.setKeyEventAction(wma::KeyEventCallback::from(
        [&seen](const wma::WMAKeyEvent& event) {
            if (event.state == wma::KeyState::Pressed)
                seen = event.key;
        }));

    listener.handleKey(0, 0, evdev, WL_KEYBOARD_KEY_STATE_PRESSED);
    listener.handleKey(0, 0, evdev, WL_KEYBOARD_KEY_STATE_RELEASED);

    return seen;
}

void testKeymapDecidesTheKey()
{
    wma::WaylandKeyboardListener listener;
    sendKeymap(listener, kKeymap);

    check(pressed(listener, 1) == wma::KEY_A,
          "the keycode at Escape's position reports the letter its keymap assigns");

    check(pressed(listener, 2) == wma::KEY_ESCAPE,
          "and the keycode the keymap gives Escape reports Escape");
}

void testUsLayoutStillMapsAsExpected()
{
    static constexpr const char* kUsLayout = R"(xkb_keymap {
      xkb_keycodes { include "evdev" };
      xkb_types    { include "complete" };
      xkb_compat   { include "complete" };
      xkb_symbols  { include "pc+us" };
    };)";

    wma::WaylandKeyboardListener listener;
    sendKeymap(listener, kUsLayout);

    //! The ordinary case has to keep working: these are the evdev codes for
    //! Escape, Enter, Tab, W and the up arrow on a us layout.
    check(pressed(listener, 1) == wma::KEY_ESCAPE, "us layout: Escape");
    check(pressed(listener, 28) == wma::KEY_ENTER, "us layout: Enter");
    check(pressed(listener, 15) == wma::KEY_TAB, "us layout: Tab");
    check(pressed(listener, 17) == wma::KEY_W, "us layout: W");
    check(pressed(listener, 103) == wma::KEY_UP, "us layout: Up");
}

void testNoKeymapFallsBackToPosition()
{
    //! Before the compositor sends a keymap there is nothing to translate
    //! through, and reading the keycode as a us position is the best guess
    //! available -- better than reporting every key as unknown.
    wma::WaylandKeyboardListener listener;

    check(pressed(listener, 1) == wma::KEY_ESCAPE,
          "with no keymap yet, the evdev position is used");
}

} // namespace

int main()
{
    testKeymapDecidesTheKey();
    testUsLayoutStillMapsAsExpected();
    testNoKeymapFallsBackToPosition();

    return failures == 0 ? 0 : 1;
}
