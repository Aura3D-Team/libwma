# Input

```cpp
auto& keyboard = window->getKeyboardListener();
auto& mouse    = window->getMouseListener();
```

## Key bindings

For gameplay: a named action per key.

```cpp
keyboard.addKeyAction(wma::Key::KEY_SPACE, wma::KeyAction{
    []{ jump(); },        // press
    []{ land(); }         // release, or nullptr
});

keyboard.hasKeyAction(wma::Key::KEY_SPACE);
keyboard.removeKeyAction(wma::Key::KEY_SPACE);
```

`i32` overloads exist for every one of these, for keybinds loaded from config or
network as plain numbers.

**Auto-repeat never fires a binding.** Held Space triggers `jump()` once. If you
want repeat, use the key-event stream below.

## Contexts

Same key, different meaning, without rebinding.

```cpp
const auto gameplay = keyboard.createContext();
const auto menu     = keyboard.createContext();

keyboard.addKeyAction(wma::Key::KEY_D, wma::KeyAction{[]{ moveRight(); }}, gameplay);
keyboard.addKeyAction(wma::Key::KEY_D, wma::KeyAction{[]{ selectNext(); }}, menu);

keyboard.setActiveContext(menu);      // flat switch
```

`pushContext()` / `popContext()` layer an overlay (a pause menu) on top of
whichever context is active, without disturbing that choice.
`getResolvedContext()` is what actually dispatches; `getActiveContext()` is the
base under the stack.

## Every key (UI layers)

A UI needs Tab, arrows, Home/End, Backspace, Enter at once and must not evict
the application's bindings. Both fire — this coexists with the binding table.

```cpp
keyboard.setKeyEventAction(wma::KeyEventCallback::from(
    [](const wma::WMAKeyEvent& e) {
        if (!e.isPressOrRepeat()) return;
        if (e.key == wma::KEY_BACKSPACE) deleteChar();
        if (e.key == wma::KEY_A && e.mods.ctrl) selectAll();
    }));
```

`WMAKeyEvent` carries `key`, `state` (`Pressed` / `Repeat` / `Released`) and
`mods` (`shift` `ctrl` `alt` `super`, plus `any()` and `onlyShiftOrNone()`).

Modifiers are derived from wma's own press/release tracking, not the backend's
mask — the backends disagree on layout and on whether the mask is sampled before
or after the event.

## Text input

A text field must consume this, **never** the key stream. Shift+2 is one key
event and one of `@` or `"` depending on layout; a compose sequence is several
key events and one character.

```cpp
window->setTextInputEnabled(true);     // some platforms need this to start an IME

keyboard.setTextInputAction(wma::TextInputCallback::from(
    [](wma::Codepoint c) { insert(c); }));
```

One call per Unicode scalar value — an IME phrase arrives as several. Control
characters (`< U+0020`, `U+007F`–`U+009F`) are filtered out already, so Enter and
Backspace reach you through the key stream only, never as both.

To write a codepoint back into a UTF-8 buffer:

```cpp
#include <wma/input/keyboard/Utf8.hpp>

std::array<char, 4> bytes{};
const usize n = wma::utf8::encode(codepoint, bytes);   // 0 if not a scalar value
```

## Mouse

```cpp
mouse.addButtonAction(wma::MouseButton::WMALeft, wma::MouseAction{
    []{ fire(); }, []{ stopFiring(); }});

mouse.setMoveAction(wma::MouseAction{[](const wma::WMAMousePosition& p) {
    look(p.deltaX, p.deltaY);
}});

mouse.setScrollAction(wma::MouseAction{[](const wma::WMAMouseScroll& s) {
    zoom(s.yOffset);
}});
```

```cpp
const auto pos    = mouse.getCurrentPosition();
const auto scroll = mouse.consumeScrollDelta();   // accumulated since last call
mouse.setCursorEnabled(false);                    // FPS capture
mouse.setCursorShape(wma::CursorShape::NwseResize); // CSS-named system image
mouse.setSensitivity(1.5);
```

Most getters are `[[nodiscard]]`. A shape the platform cannot show falls back to
the arrow: Emscripten's GLFW has none, and X11's core font has no diagonal
double arrows, so it uses corner arrows.

Mouse contexts work exactly like keyboard contexts, and are independent of them.
