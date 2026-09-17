#ifndef KEYS_H
#define KEYS_H

#include <ink/ink_base.hpp>

#ifdef WMA_ENABLE_GLFW
#include <GLFW/glfw3.h>
#endif

#ifdef WMA_ENABLE_SDL
#include <SDL3/SDL.h>
#endif

#ifdef WMA_ENABLE_X11
#include <X11/Xlib.h>
#include <X11/keysym.h>
#endif

#ifdef WMA_ENABLE_WAYLAND
//! Keysym constants only -- a standalone header that pulls in no client API.
#include <xkbcommon/xkbcommon-keysyms.h>
#endif

namespace wma {

enum Key : i32 {
    // Letters
    KEY_A = 0,
    KEY_B,
    KEY_C,
    KEY_D,
    KEY_E,
    KEY_F,
    KEY_G,
    KEY_H,
    KEY_I,
    KEY_J,
    KEY_K,
    KEY_L,
    KEY_M,
    KEY_N,
    KEY_O,
    KEY_P,
    KEY_Q,
    KEY_R,
    KEY_S,
    KEY_T,
    KEY_U,
    KEY_V,
    KEY_W,
    KEY_X,
    KEY_Y,
    KEY_Z,

    // Numbers
    KEY_0,
    KEY_1,
    KEY_2,
    KEY_3,
    KEY_4,
    KEY_5,
    KEY_6,
    KEY_7,
    KEY_8,
    KEY_9,

    // Function keys
    KEY_F1,
    KEY_F2,
    KEY_F3,
    KEY_F4,
    KEY_F5,
    KEY_F6,
    KEY_F7,
    KEY_F8,
    KEY_F9,
    KEY_F10,
    KEY_F11,
    KEY_F12,

    // Control keys
    KEY_ESCAPE,
    KEY_ENTER,
    KEY_TAB,
    KEY_BACKSPACE,
    KEY_INSERT,
    KEY_DELETE,
    KEY_RIGHT,
    KEY_LEFT,
    KEY_DOWN,
    KEY_UP,
    KEY_PAGE_UP,
    KEY_PAGE_DOWN,
    KEY_HOME,
    KEY_END,

    // Modifier keys
    KEY_CAPS_LOCK,
    KEY_SCROLL_LOCK,
    KEY_NUM_LOCK,
    KEY_LEFT_SHIFT,
    KEY_RIGHT_SHIFT,
    KEY_LEFT_CTRL,
    KEY_RIGHT_CTRL,
    KEY_LEFT_ALT,
    KEY_RIGHT_ALT,
    KEY_LEFT_SUPER,  // Windows/Command
    KEY_RIGHT_SUPER,

    // Symbols
    KEY_SPACE,
    KEY_MINUS,
    KEY_EQUAL,
    KEY_LEFT_BRACKET,
    KEY_RIGHT_BRACKET,
    KEY_BACKSLASH,
    KEY_SEMICOLON,
    KEY_APOSTROPHE,
    KEY_GRAVE,
    KEY_COMMA,
    KEY_PERIOD,
    KEY_SLASH,

    // Keypad
    KEY_KP_0,
    KEY_KP_1,
    KEY_KP_2,
    KEY_KP_3,
    KEY_KP_4,
    KEY_KP_5,
    KEY_KP_6,
    KEY_KP_7,
    KEY_KP_8,
    KEY_KP_9,
    KEY_KP_DECIMAL,
    KEY_KP_DIVIDE,
    KEY_KP_MULTIPLY,
    KEY_KP_SUBTRACT,
    KEY_KP_ADD,
    KEY_KP_ENTER,

    KEY_UNKNOWN = -1
};

constexpr usize KEY_COUNT = static_cast<usize>(KEY_KP_ENTER) + 1u;

#ifdef WMA_ENABLE_GLFW

//! Map keys to GLFW
inline Key mapGLFWKey(int glfwKey) {
    switch (glfwKey) {
    // Letters
    case GLFW_KEY_A: return Key::KEY_A;
    case GLFW_KEY_B: return Key::KEY_B;
    case GLFW_KEY_C: return Key::KEY_C;
    case GLFW_KEY_D: return Key::KEY_D;
    case GLFW_KEY_E: return Key::KEY_E;
    case GLFW_KEY_F: return Key::KEY_F;
    case GLFW_KEY_G: return Key::KEY_G;
    case GLFW_KEY_H: return Key::KEY_H;
    case GLFW_KEY_I: return Key::KEY_I;
    case GLFW_KEY_J: return Key::KEY_J;
    case GLFW_KEY_K: return Key::KEY_K;
    case GLFW_KEY_L: return Key::KEY_L;
    case GLFW_KEY_M: return Key::KEY_M;
    case GLFW_KEY_N: return Key::KEY_N;
    case GLFW_KEY_O: return Key::KEY_O;
    case GLFW_KEY_P: return Key::KEY_P;
    case GLFW_KEY_Q: return Key::KEY_Q;
    case GLFW_KEY_R: return Key::KEY_R;
    case GLFW_KEY_S: return Key::KEY_S;
    case GLFW_KEY_T: return Key::KEY_T;
    case GLFW_KEY_U: return Key::KEY_U;
    case GLFW_KEY_V: return Key::KEY_V;
    case GLFW_KEY_W: return Key::KEY_W;
    case GLFW_KEY_X: return Key::KEY_X;
    case GLFW_KEY_Y: return Key::KEY_Y;
    case GLFW_KEY_Z: return Key::KEY_Z;

        // Numbers
    case GLFW_KEY_0: return Key::KEY_0;
    case GLFW_KEY_1: return Key::KEY_1;
    case GLFW_KEY_2: return Key::KEY_2;
    case GLFW_KEY_3: return Key::KEY_3;
    case GLFW_KEY_4: return Key::KEY_4;
    case GLFW_KEY_5: return Key::KEY_5;
    case GLFW_KEY_6: return Key::KEY_6;
    case GLFW_KEY_7: return Key::KEY_7;
    case GLFW_KEY_8: return Key::KEY_8;
    case GLFW_KEY_9: return Key::KEY_9;

        // Function keys
    case GLFW_KEY_F1: return Key::KEY_F1;
    case GLFW_KEY_F2: return Key::KEY_F2;
    case GLFW_KEY_F3: return Key::KEY_F3;
    case GLFW_KEY_F4: return Key::KEY_F4;
    case GLFW_KEY_F5: return Key::KEY_F5;
    case GLFW_KEY_F6: return Key::KEY_F6;
    case GLFW_KEY_F7: return Key::KEY_F7;
    case GLFW_KEY_F8: return Key::KEY_F8;
    case GLFW_KEY_F9: return Key::KEY_F9;
    case GLFW_KEY_F10: return Key::KEY_F10;
    case GLFW_KEY_F11: return Key::KEY_F11;
    case GLFW_KEY_F12: return Key::KEY_F12;

        // Controls
    case GLFW_KEY_ESCAPE: return Key::KEY_ESCAPE;
    case GLFW_KEY_ENTER: return Key::KEY_ENTER;
    case GLFW_KEY_TAB: return Key::KEY_TAB;
    case GLFW_KEY_BACKSPACE: return Key::KEY_BACKSPACE;
    case GLFW_KEY_INSERT: return Key::KEY_INSERT;
    case GLFW_KEY_DELETE: return Key::KEY_DELETE;
    case GLFW_KEY_RIGHT: return Key::KEY_RIGHT;
    case GLFW_KEY_LEFT: return Key::KEY_LEFT;
    case GLFW_KEY_DOWN: return Key::KEY_DOWN;
    case GLFW_KEY_UP: return Key::KEY_UP;
    case GLFW_KEY_PAGE_UP: return Key::KEY_PAGE_UP;
    case GLFW_KEY_PAGE_DOWN: return Key::KEY_PAGE_DOWN;
    case GLFW_KEY_HOME: return Key::KEY_HOME;
    case GLFW_KEY_END: return Key::KEY_END;

        // Modifiers
    case GLFW_KEY_LEFT_SHIFT: return Key::KEY_LEFT_SHIFT;
    case GLFW_KEY_RIGHT_SHIFT: return Key::KEY_RIGHT_SHIFT;
    case GLFW_KEY_LEFT_CONTROL: return Key::KEY_LEFT_CTRL;
    case GLFW_KEY_RIGHT_CONTROL: return Key::KEY_RIGHT_CTRL;
    case GLFW_KEY_LEFT_ALT: return Key::KEY_LEFT_ALT;
    case GLFW_KEY_RIGHT_ALT: return Key::KEY_RIGHT_ALT;
    case GLFW_KEY_LEFT_SUPER: return Key::KEY_LEFT_SUPER;
    case GLFW_KEY_RIGHT_SUPER: return Key::KEY_RIGHT_SUPER;
    case GLFW_KEY_CAPS_LOCK: return Key::KEY_CAPS_LOCK;
    case GLFW_KEY_SCROLL_LOCK: return Key::KEY_SCROLL_LOCK;
    case GLFW_KEY_NUM_LOCK: return Key::KEY_NUM_LOCK;

        // Symbols
    case GLFW_KEY_SPACE: return Key::KEY_SPACE;
    case GLFW_KEY_MINUS: return Key::KEY_MINUS;
    case GLFW_KEY_EQUAL: return Key::KEY_EQUAL;
    case GLFW_KEY_LEFT_BRACKET: return Key::KEY_LEFT_BRACKET;
    case GLFW_KEY_RIGHT_BRACKET: return Key::KEY_RIGHT_BRACKET;
    case GLFW_KEY_BACKSLASH: return Key::KEY_BACKSLASH;
    case GLFW_KEY_SEMICOLON: return Key::KEY_SEMICOLON;
    case GLFW_KEY_APOSTROPHE: return Key::KEY_APOSTROPHE;
    case GLFW_KEY_GRAVE_ACCENT: return Key::KEY_GRAVE;
    case GLFW_KEY_COMMA: return Key::KEY_COMMA;
    case GLFW_KEY_PERIOD: return Key::KEY_PERIOD;
    case GLFW_KEY_SLASH: return Key::KEY_SLASH;

    // Keypad
    case GLFW_KEY_KP_0: return Key::KEY_KP_0;
    case GLFW_KEY_KP_1: return Key::KEY_KP_1;
    case GLFW_KEY_KP_2: return Key::KEY_KP_2;
    case GLFW_KEY_KP_3: return Key::KEY_KP_3;
    case GLFW_KEY_KP_4: return Key::KEY_KP_4;
    case GLFW_KEY_KP_5: return Key::KEY_KP_5;
    case GLFW_KEY_KP_6: return Key::KEY_KP_6;
    case GLFW_KEY_KP_7: return Key::KEY_KP_7;
    case GLFW_KEY_KP_8: return Key::KEY_KP_8;
    case GLFW_KEY_KP_9: return Key::KEY_KP_9;
    case GLFW_KEY_KP_DECIMAL: return Key::KEY_KP_DECIMAL;
    case GLFW_KEY_KP_DIVIDE: return Key::KEY_KP_DIVIDE;
    case GLFW_KEY_KP_MULTIPLY: return Key::KEY_KP_MULTIPLY;
    case GLFW_KEY_KP_SUBTRACT: return Key::KEY_KP_SUBTRACT;
    case GLFW_KEY_KP_ADD: return Key::KEY_KP_ADD;
    case GLFW_KEY_KP_ENTER: return Key::KEY_KP_ENTER;

    default: return Key::KEY_UNKNOWN;
    }
}

#endif


#ifdef WMA_ENABLE_SDL
//! Map keys to SDL3
inline Key mapSDLKey(SDL_Keycode sdlKey) {
    switch (sdlKey) {
    // Letters
    case SDLK_A: return Key::KEY_A;
    case SDLK_B: return Key::KEY_B;
    case SDLK_C: return Key::KEY_C;
    case SDLK_D: return Key::KEY_D;
    case SDLK_E: return Key::KEY_E;
    case SDLK_F: return Key::KEY_F;
    case SDLK_G: return Key::KEY_G;
    case SDLK_H: return Key::KEY_H;
    case SDLK_I: return Key::KEY_I;
    case SDLK_J: return Key::KEY_J;
    case SDLK_K: return Key::KEY_K;
    case SDLK_L: return Key::KEY_L;
    case SDLK_M: return Key::KEY_M;
    case SDLK_N: return Key::KEY_N;
    case SDLK_O: return Key::KEY_O;
    case SDLK_P: return Key::KEY_P;
    case SDLK_Q: return Key::KEY_Q;
    case SDLK_R: return Key::KEY_R;
    case SDLK_S: return Key::KEY_S;
    case SDLK_T: return Key::KEY_T;
    case SDLK_U: return Key::KEY_U;
    case SDLK_V: return Key::KEY_V;
    case SDLK_W: return Key::KEY_W;
    case SDLK_X: return Key::KEY_X;
    case SDLK_Y: return Key::KEY_Y;
    case SDLK_Z: return Key::KEY_Z;

        // Numbers
    case SDLK_0: return Key::KEY_0;
    case SDLK_1: return Key::KEY_1;
    case SDLK_2: return Key::KEY_2;
    case SDLK_3: return Key::KEY_3;
    case SDLK_4: return Key::KEY_4;
    case SDLK_5: return Key::KEY_5;
    case SDLK_6: return Key::KEY_6;
    case SDLK_7: return Key::KEY_7;
    case SDLK_8: return Key::KEY_8;
    case SDLK_9: return Key::KEY_9;

        // Function keys
    case SDLK_F1: return Key::KEY_F1;
    case SDLK_F2: return Key::KEY_F2;
    case SDLK_F3: return Key::KEY_F3;
    case SDLK_F4: return Key::KEY_F4;
    case SDLK_F5: return Key::KEY_F5;
    case SDLK_F6: return Key::KEY_F6;
    case SDLK_F7: return Key::KEY_F7;
    case SDLK_F8: return Key::KEY_F8;
    case SDLK_F9: return Key::KEY_F9;
    case SDLK_F10: return Key::KEY_F10;
    case SDLK_F11: return Key::KEY_F11;
    case SDLK_F12: return Key::KEY_F12;

        // Controls
    case SDLK_ESCAPE: return Key::KEY_ESCAPE;
    case SDLK_RETURN: return Key::KEY_ENTER;
    case SDLK_TAB: return Key::KEY_TAB;
    case SDLK_BACKSPACE: return Key::KEY_BACKSPACE;
    case SDLK_INSERT: return Key::KEY_INSERT;
    case SDLK_DELETE: return Key::KEY_DELETE;
    case SDLK_RIGHT: return Key::KEY_RIGHT;
    case SDLK_LEFT: return Key::KEY_LEFT;
    case SDLK_DOWN: return Key::KEY_DOWN;
    case SDLK_UP: return Key::KEY_UP;
    case SDLK_PAGEUP: return Key::KEY_PAGE_UP;
    case SDLK_PAGEDOWN: return Key::KEY_PAGE_DOWN;
    case SDLK_HOME: return Key::KEY_HOME;
    case SDLK_END: return Key::KEY_END;

        // Modifiers
    case SDLK_LSHIFT: return Key::KEY_LEFT_SHIFT;
    case SDLK_RSHIFT: return Key::KEY_RIGHT_SHIFT;
    case SDLK_LCTRL: return Key::KEY_LEFT_CTRL;
    case SDLK_RCTRL: return Key::KEY_RIGHT_CTRL;
    case SDLK_LALT: return Key::KEY_LEFT_ALT;
    case SDLK_RALT: return Key::KEY_RIGHT_ALT;
    case SDLK_LGUI: return Key::KEY_LEFT_SUPER;
    case SDLK_RGUI: return Key::KEY_RIGHT_SUPER;
    case SDLK_CAPSLOCK: return Key::KEY_CAPS_LOCK;
    case SDLK_NUMLOCKCLEAR: return Key::KEY_NUM_LOCK;
    case SDLK_SCROLLLOCK: return Key::KEY_SCROLL_LOCK;

        // Symbols
    case SDLK_SPACE: return Key::KEY_SPACE;
    case SDLK_MINUS: return Key::KEY_MINUS;
    case SDLK_EQUALS: return Key::KEY_EQUAL;
    case SDLK_LEFTBRACKET: return Key::KEY_LEFT_BRACKET;
    case SDLK_RIGHTBRACKET: return Key::KEY_RIGHT_BRACKET;
    case SDLK_BACKSLASH: return Key::KEY_BACKSLASH;
    case SDLK_SEMICOLON: return Key::KEY_SEMICOLON;
    case SDLK_APOSTROPHE: return Key::KEY_APOSTROPHE;
    case SDLK_GRAVE: return Key::KEY_GRAVE;
    case SDLK_COMMA: return Key::KEY_COMMA;
    case SDLK_PERIOD: return Key::KEY_PERIOD;
    case SDLK_SLASH: return Key::KEY_SLASH;

    // Keypad
    case SDLK_KP_0: return Key::KEY_KP_0;
    case SDLK_KP_1: return Key::KEY_KP_1;
    case SDLK_KP_2: return Key::KEY_KP_2;
    case SDLK_KP_3: return Key::KEY_KP_3;
    case SDLK_KP_4: return Key::KEY_KP_4;
    case SDLK_KP_5: return Key::KEY_KP_5;
    case SDLK_KP_6: return Key::KEY_KP_6;
    case SDLK_KP_7: return Key::KEY_KP_7;
    case SDLK_KP_8: return Key::KEY_KP_8;
    case SDLK_KP_9: return Key::KEY_KP_9;
    case SDLK_KP_PERIOD: return Key::KEY_KP_DECIMAL;
    case SDLK_KP_DIVIDE: return Key::KEY_KP_DIVIDE;
    case SDLK_KP_MULTIPLY: return Key::KEY_KP_MULTIPLY;
    case SDLK_KP_MINUS: return Key::KEY_KP_SUBTRACT;
    case SDLK_KP_PLUS: return Key::KEY_KP_ADD;
    case SDLK_KP_ENTER: return Key::KEY_KP_ENTER;

    default: return Key::KEY_UNKNOWN;
    }
}

#endif

#ifdef WMA_ENABLE_X11
//! Map keys to X11 KeySym
inline Key mapX11Key(KeySym x11Key) {
    switch (x11Key) {
    // Letters
    case XK_a: return Key::KEY_A;
    case XK_b: return Key::KEY_B;
    case XK_c: return Key::KEY_C;
    case XK_d: return Key::KEY_D;
    case XK_e: return Key::KEY_E;
    case XK_f: return Key::KEY_F;
    case XK_g: return Key::KEY_G;
    case XK_h: return Key::KEY_H;
    case XK_i: return Key::KEY_I;
    case XK_j: return Key::KEY_J;
    case XK_k: return Key::KEY_K;
    case XK_l: return Key::KEY_L;
    case XK_m: return Key::KEY_M;
    case XK_n: return Key::KEY_N;
    case XK_o: return Key::KEY_O;
    case XK_p: return Key::KEY_P;
    case XK_q: return Key::KEY_Q;
    case XK_r: return Key::KEY_R;
    case XK_s: return Key::KEY_S;
    case XK_t: return Key::KEY_T;
    case XK_u: return Key::KEY_U;
    case XK_v: return Key::KEY_V;
    case XK_w: return Key::KEY_W;
    case XK_x: return Key::KEY_X;
    case XK_y: return Key::KEY_Y;
    case XK_z: return Key::KEY_Z;

        // Numbers
    case XK_0: return Key::KEY_0;
    case XK_1: return Key::KEY_1;
    case XK_2: return Key::KEY_2;
    case XK_3: return Key::KEY_3;
    case XK_4: return Key::KEY_4;
    case XK_5: return Key::KEY_5;
    case XK_6: return Key::KEY_6;
    case XK_7: return Key::KEY_7;
    case XK_8: return Key::KEY_8;
    case XK_9: return Key::KEY_9;

        // Function keys
    case XK_F1: return Key::KEY_F1;
    case XK_F2: return Key::KEY_F2;
    case XK_F3: return Key::KEY_F3;
    case XK_F4: return Key::KEY_F4;
    case XK_F5: return Key::KEY_F5;
    case XK_F6: return Key::KEY_F6;
    case XK_F7: return Key::KEY_F7;
    case XK_F8: return Key::KEY_F8;
    case XK_F9: return Key::KEY_F9;
    case XK_F10: return Key::KEY_F10;
    case XK_F11: return Key::KEY_F11;
    case XK_F12: return Key::KEY_F12;

        // Controls
    case XK_Escape: return Key::KEY_ESCAPE;
    case XK_Return: return Key::KEY_ENTER; // Main Enter key
    case XK_Tab: return Key::KEY_TAB;
    case XK_BackSpace: return Key::KEY_BACKSPACE;
    case XK_Insert: return Key::KEY_INSERT;
    case XK_Delete: return Key::KEY_DELETE;
    case XK_Right: return Key::KEY_RIGHT;
    case XK_Left: return Key::KEY_LEFT;
    case XK_Down: return Key::KEY_DOWN;
    case XK_Up: return Key::KEY_UP;
    case XK_Page_Up: return Key::KEY_PAGE_UP;
    case XK_Page_Down: return Key::KEY_PAGE_DOWN;
    case XK_Home: return Key::KEY_HOME;
    case XK_End: return Key::KEY_END;

        // Modifiers
    case XK_Shift_L: return Key::KEY_LEFT_SHIFT;
    case XK_Shift_R: return Key::KEY_RIGHT_SHIFT;
    case XK_Control_L: return Key::KEY_LEFT_CTRL;
    case XK_Control_R: return Key::KEY_RIGHT_CTRL;
    case XK_Alt_L: return Key::KEY_LEFT_ALT;
    case XK_Alt_R: return Key::KEY_RIGHT_ALT;
    case XK_Super_L: return Key::KEY_LEFT_SUPER;
    case XK_Super_R: return Key::KEY_RIGHT_SUPER;
    case XK_Caps_Lock: return Key::KEY_CAPS_LOCK;
    case XK_Scroll_Lock: return Key::KEY_SCROLL_LOCK;
    case XK_Num_Lock: return Key::KEY_NUM_LOCK;

        // Symbols
    case XK_space: return Key::KEY_SPACE;
    case XK_minus: return Key::KEY_MINUS;
    case XK_equal: return Key::KEY_EQUAL;
    case XK_bracketleft: return Key::KEY_LEFT_BRACKET;
    case XK_bracketright: return Key::KEY_RIGHT_BRACKET;
    case XK_backslash: return Key::KEY_BACKSLASH;
    case XK_semicolon: return Key::KEY_SEMICOLON;
    case XK_apostrophe: return Key::KEY_APOSTROPHE;
    case XK_grave: return Key::KEY_GRAVE;
    case XK_comma: return Key::KEY_COMMA;
    case XK_period: return Key::KEY_PERIOD;
    case XK_slash: return Key::KEY_SLASH;

        // Keypad
    case XK_KP_0: return Key::KEY_KP_0;
    case XK_KP_1: return Key::KEY_KP_1;
    case XK_KP_2: return Key::KEY_KP_2;
    case XK_KP_3: return Key::KEY_KP_3;
    case XK_KP_4: return Key::KEY_KP_4;
    case XK_KP_5: return Key::KEY_KP_5;
    case XK_KP_6: return Key::KEY_KP_6;
    case XK_KP_7: return Key::KEY_KP_7;
    case XK_KP_8: return Key::KEY_KP_8;
    case XK_KP_9: return Key::KEY_KP_9;
    case XK_KP_Decimal: return Key::KEY_KP_DECIMAL;
    case XK_KP_Divide: return Key::KEY_KP_DIVIDE;
    case XK_KP_Multiply: return Key::KEY_KP_MULTIPLY;
    case XK_KP_Subtract: return Key::KEY_KP_SUBTRACT;
    case XK_KP_Add: return Key::KEY_KP_ADD;
    case XK_KP_Enter: return Key::KEY_KP_ENTER; // Keypad Enter key

    default: return Key::KEY_UNKNOWN;
    }
}
#endif

#ifdef WMA_ENABLE_WAYLAND
/**
 * @brief The key a keysym names, for the Wayland backend.
 *
 * The same table @ref mapX11Key uses: xkbcommon reuses X11's keysym values, so
 * only the constant prefix differs. Kept separate rather than shared so a
 * Wayland-only build needs no X11 headers.
 */
inline Key mapWaylandKeysym(u32 keysym) {
    switch (keysym) {
    // Letters
    case XKB_KEY_a: return Key::KEY_A;
    case XKB_KEY_b: return Key::KEY_B;
    case XKB_KEY_c: return Key::KEY_C;
    case XKB_KEY_d: return Key::KEY_D;
    case XKB_KEY_e: return Key::KEY_E;
    case XKB_KEY_f: return Key::KEY_F;
    case XKB_KEY_g: return Key::KEY_G;
    case XKB_KEY_h: return Key::KEY_H;
    case XKB_KEY_i: return Key::KEY_I;
    case XKB_KEY_j: return Key::KEY_J;
    case XKB_KEY_k: return Key::KEY_K;
    case XKB_KEY_l: return Key::KEY_L;
    case XKB_KEY_m: return Key::KEY_M;
    case XKB_KEY_n: return Key::KEY_N;
    case XKB_KEY_o: return Key::KEY_O;
    case XKB_KEY_p: return Key::KEY_P;
    case XKB_KEY_q: return Key::KEY_Q;
    case XKB_KEY_r: return Key::KEY_R;
    case XKB_KEY_s: return Key::KEY_S;
    case XKB_KEY_t: return Key::KEY_T;
    case XKB_KEY_u: return Key::KEY_U;
    case XKB_KEY_v: return Key::KEY_V;
    case XKB_KEY_w: return Key::KEY_W;
    case XKB_KEY_x: return Key::KEY_X;
    case XKB_KEY_y: return Key::KEY_Y;
    case XKB_KEY_z: return Key::KEY_Z;

        // Numbers
    case XKB_KEY_0: return Key::KEY_0;
    case XKB_KEY_1: return Key::KEY_1;
    case XKB_KEY_2: return Key::KEY_2;
    case XKB_KEY_3: return Key::KEY_3;
    case XKB_KEY_4: return Key::KEY_4;
    case XKB_KEY_5: return Key::KEY_5;
    case XKB_KEY_6: return Key::KEY_6;
    case XKB_KEY_7: return Key::KEY_7;
    case XKB_KEY_8: return Key::KEY_8;
    case XKB_KEY_9: return Key::KEY_9;

        // Function keys
    case XKB_KEY_F1: return Key::KEY_F1;
    case XKB_KEY_F2: return Key::KEY_F2;
    case XKB_KEY_F3: return Key::KEY_F3;
    case XKB_KEY_F4: return Key::KEY_F4;
    case XKB_KEY_F5: return Key::KEY_F5;
    case XKB_KEY_F6: return Key::KEY_F6;
    case XKB_KEY_F7: return Key::KEY_F7;
    case XKB_KEY_F8: return Key::KEY_F8;
    case XKB_KEY_F9: return Key::KEY_F9;
    case XKB_KEY_F10: return Key::KEY_F10;
    case XKB_KEY_F11: return Key::KEY_F11;
    case XKB_KEY_F12: return Key::KEY_F12;

        // Controls
    case XKB_KEY_Escape: return Key::KEY_ESCAPE;
    case XKB_KEY_Return: return Key::KEY_ENTER; // Main Enter key
    case XKB_KEY_Tab: return Key::KEY_TAB;
    case XKB_KEY_BackSpace: return Key::KEY_BACKSPACE;
    case XKB_KEY_Insert: return Key::KEY_INSERT;
    case XKB_KEY_Delete: return Key::KEY_DELETE;
    case XKB_KEY_Right: return Key::KEY_RIGHT;
    case XKB_KEY_Left: return Key::KEY_LEFT;
    case XKB_KEY_Down: return Key::KEY_DOWN;
    case XKB_KEY_Up: return Key::KEY_UP;
    case XKB_KEY_Page_Up: return Key::KEY_PAGE_UP;
    case XKB_KEY_Page_Down: return Key::KEY_PAGE_DOWN;
    case XKB_KEY_Home: return Key::KEY_HOME;
    case XKB_KEY_End: return Key::KEY_END;

        // Modifiers
    case XKB_KEY_Shift_L: return Key::KEY_LEFT_SHIFT;
    case XKB_KEY_Shift_R: return Key::KEY_RIGHT_SHIFT;
    case XKB_KEY_Control_L: return Key::KEY_LEFT_CTRL;
    case XKB_KEY_Control_R: return Key::KEY_RIGHT_CTRL;
    case XKB_KEY_Alt_L: return Key::KEY_LEFT_ALT;
    case XKB_KEY_Alt_R: return Key::KEY_RIGHT_ALT;
    case XKB_KEY_Super_L: return Key::KEY_LEFT_SUPER;
    case XKB_KEY_Super_R: return Key::KEY_RIGHT_SUPER;
    case XKB_KEY_Caps_Lock: return Key::KEY_CAPS_LOCK;
    case XKB_KEY_Scroll_Lock: return Key::KEY_SCROLL_LOCK;
    case XKB_KEY_Num_Lock: return Key::KEY_NUM_LOCK;

        // Symbols
    case XKB_KEY_space: return Key::KEY_SPACE;
    case XKB_KEY_minus: return Key::KEY_MINUS;
    case XKB_KEY_equal: return Key::KEY_EQUAL;
    case XKB_KEY_bracketleft: return Key::KEY_LEFT_BRACKET;
    case XKB_KEY_bracketright: return Key::KEY_RIGHT_BRACKET;
    case XKB_KEY_backslash: return Key::KEY_BACKSLASH;
    case XKB_KEY_semicolon: return Key::KEY_SEMICOLON;
    case XKB_KEY_apostrophe: return Key::KEY_APOSTROPHE;
    case XKB_KEY_grave: return Key::KEY_GRAVE;
    case XKB_KEY_comma: return Key::KEY_COMMA;
    case XKB_KEY_period: return Key::KEY_PERIOD;
    case XKB_KEY_slash: return Key::KEY_SLASH;

        // Keypad
    case XKB_KEY_KP_0: return Key::KEY_KP_0;
    case XKB_KEY_KP_1: return Key::KEY_KP_1;
    case XKB_KEY_KP_2: return Key::KEY_KP_2;
    case XKB_KEY_KP_3: return Key::KEY_KP_3;
    case XKB_KEY_KP_4: return Key::KEY_KP_4;
    case XKB_KEY_KP_5: return Key::KEY_KP_5;
    case XKB_KEY_KP_6: return Key::KEY_KP_6;
    case XKB_KEY_KP_7: return Key::KEY_KP_7;
    case XKB_KEY_KP_8: return Key::KEY_KP_8;
    case XKB_KEY_KP_9: return Key::KEY_KP_9;
    case XKB_KEY_KP_Decimal: return Key::KEY_KP_DECIMAL;
    case XKB_KEY_KP_Divide: return Key::KEY_KP_DIVIDE;
    case XKB_KEY_KP_Multiply: return Key::KEY_KP_MULTIPLY;
    case XKB_KEY_KP_Subtract: return Key::KEY_KP_SUBTRACT;
    case XKB_KEY_KP_Add: return Key::KEY_KP_ADD;
    case XKB_KEY_KP_Enter: return Key::KEY_KP_ENTER; // Keypad Enter key

    default: return Key::KEY_UNKNOWN;
    }
}

/**
 * @brief The key at an evdev position, assuming a us layout.
 *
 * A fallback, not the normal path. The compositor hands every client a keymap
 * and expects it to translate through that; reading the keycode as a fixed
 * position is right only on a us layout, and on a virtual keyboard -- which
 * assigns whatever keysyms it likes to whatever keycodes it likes -- it is
 * nonsense. Used only until the keymap arrives.
 *
 * The handler adds 8 to get the xkbKeycode, so subtract 8 to get back to the
 * standard Linux evdev keycode.
 */
inline Key mapWaylandKey(u32 xkbKeycode) {
    u32 evdevKey = xkbKeycode - 8;

    switch (evdevKey) {
    // Letters (Physical QWERTY positions)
    case 30: return Key::KEY_A;
    case 48: return Key::KEY_B;
    case 46: return Key::KEY_C;
    case 32: return Key::KEY_D;
    case 18: return Key::KEY_E;
    case 33: return Key::KEY_F;
    case 34: return Key::KEY_G;
    case 35: return Key::KEY_H;
    case 23: return Key::KEY_I;
    case 36: return Key::KEY_J;
    case 37: return Key::KEY_K;
    case 38: return Key::KEY_L;
    case 50: return Key::KEY_M;
    case 49: return Key::KEY_N;
    case 24: return Key::KEY_O;
    case 25: return Key::KEY_P;
    case 16: return Key::KEY_Q;
    case 19: return Key::KEY_R;
    case 31: return Key::KEY_S;
    case 20: return Key::KEY_T;
    case 22: return Key::KEY_U;
    case 47: return Key::KEY_V;
    case 17: return Key::KEY_W;
    case 45: return Key::KEY_X;
    case 21: return Key::KEY_Y;
    case 44: return Key::KEY_Z;

    // Numbers
    case 11: return Key::KEY_0;
    case 2:  return Key::KEY_1;
    case 3:  return Key::KEY_2;
    case 4:  return Key::KEY_3;
    case 5:  return Key::KEY_4;
    case 6:  return Key::KEY_5;
    case 7:  return Key::KEY_6;
    case 8:  return Key::KEY_7;
    case 9:  return Key::KEY_8;
    case 10: return Key::KEY_9;

    // Function keys
    case 59: return Key::KEY_F1;
    case 60: return Key::KEY_F2;
    case 61: return Key::KEY_F3;
    case 62: return Key::KEY_F4;
    case 63: return Key::KEY_F5;
    case 64: return Key::KEY_F6;
    case 65: return Key::KEY_F7;
    case 66: return Key::KEY_F8;
    case 67: return Key::KEY_F9;
    case 68: return Key::KEY_F10;
    case 87: return Key::KEY_F11;
    case 88: return Key::KEY_F12;

    // Controls
    case 1:   return Key::KEY_ESCAPE;
    case 28:  return Key::KEY_ENTER; // Main Enter key
    case 15:  return Key::KEY_TAB;
    case 14:  return Key::KEY_BACKSPACE;
    case 110: return Key::KEY_INSERT;
    case 111: return Key::KEY_DELETE;
    case 106: return Key::KEY_RIGHT;
    case 105: return Key::KEY_LEFT;
    case 108: return Key::KEY_DOWN;
    case 103: return Key::KEY_UP;
    case 104: return Key::KEY_PAGE_UP;
    case 109: return Key::KEY_PAGE_DOWN;
    case 102: return Key::KEY_HOME;
    case 107: return Key::KEY_END;

    // Modifiers
    case 42:  return Key::KEY_LEFT_SHIFT;
    case 54:  return Key::KEY_RIGHT_SHIFT;
    case 29:  return Key::KEY_LEFT_CTRL;
    case 97:  return Key::KEY_RIGHT_CTRL;
    case 56:  return Key::KEY_LEFT_ALT;
    case 100: return Key::KEY_RIGHT_ALT;
    case 125: return Key::KEY_LEFT_SUPER;
    case 126: return Key::KEY_RIGHT_SUPER;
    case 58:  return Key::KEY_CAPS_LOCK;
    case 70:  return Key::KEY_SCROLL_LOCK;
    case 69:  return Key::KEY_NUM_LOCK;

    // Symbols
    case 57: return Key::KEY_SPACE;
    case 12: return Key::KEY_MINUS;
    case 13: return Key::KEY_EQUAL;
    case 26: return Key::KEY_LEFT_BRACKET;
    case 27: return Key::KEY_RIGHT_BRACKET;
    case 43: return Key::KEY_BACKSLASH;
    case 39: return Key::KEY_SEMICOLON;
    case 40: return Key::KEY_APOSTROPHE;
    case 41: return Key::KEY_GRAVE;
    case 51: return Key::KEY_COMMA;
    case 52: return Key::KEY_PERIOD;
    case 53: return Key::KEY_SLASH;

    // Keypad
    case 82: return Key::KEY_KP_0;
    case 79: return Key::KEY_KP_1;
    case 80: return Key::KEY_KP_2;
    case 81: return Key::KEY_KP_3;
    case 75: return Key::KEY_KP_4;
    case 76: return Key::KEY_KP_5;
    case 77: return Key::KEY_KP_6;
    case 71: return Key::KEY_KP_7;
    case 72: return Key::KEY_KP_8;
    case 73: return Key::KEY_KP_9;
    case 83: return Key::KEY_KP_DECIMAL;
    case 98: return Key::KEY_KP_DIVIDE;
    case 55: return Key::KEY_KP_MULTIPLY;
    case 74: return Key::KEY_KP_SUBTRACT;
    case 78: return Key::KEY_KP_ADD;
    case 96: return Key::KEY_KP_ENTER; // Keypad Enter key

    default: return Key::KEY_UNKNOWN;
    }
}
#endif

} // namespace wma

#endif // KEYS_H
