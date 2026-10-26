#pragma once

#include <stdbool.h>

// Window, input, time. Separate from gfx.h because it is a separate concern
// with a separate migration story: gfx_* may move to sokol_gfx while this stays
// on raylib (or moves to sokol_app). The rule both share is the same -- nothing
// above these two headers includes raylib.h.

#ifdef __cplusplus
extern "C" {
#endif

typedef enum plat_key {
    PLAT_KEY_W = 0, PLAT_KEY_A, PLAT_KEY_S, PLAT_KEY_D,
    PLAT_KEY_SPACE, PLAT_KEY_SHIFT, PLAT_KEY_CTRL,
    PLAT_KEY_ESC, PLAT_KEY_TAB,
    PLAT_KEY_F, PLAT_KEY_R, PLAT_KEY_P, PLAT_KEY_E, PLAT_KEY_Q, PLAT_KEY_V, PLAT_KEY_C,
    PLAT_KEY_F1, PLAT_KEY_F2, PLAT_KEY_F3, PLAT_KEY_F5, PLAT_KEY_F6, PLAT_KEY_F7,

    // The number row. Contiguous and in order, because the hotbar reads them
    // as PLAT_KEY_1 + n rather than nine cases -- so the order here is load
    // bearing, not cosmetic.
    PLAT_KEY_1, PLAT_KEY_2, PLAT_KEY_3, PLAT_KEY_4, PLAT_KEY_5,
    PLAT_KEY_6, PLAT_KEY_7, PLAT_KEY_8, PLAT_KEY_9,

    PLAT_KEY_ENTER, PLAT_KEY_BACKSPACE,
    PLAT_KEY_UP, PLAT_KEY_DOWN,   // the chat's history
    PLAT_KEY_Z, PLAT_KEY_X,       // a track piece in hand turned left, right
    PLAT_KEY_B,                   // heard by mods only (mods/backpack opens with it)
    PLAT_KEY_F4,                  // heard by mods only (mods/territory shows its edges with it)
    PLAT_KEY_F8,                  // heard by mods only (mods/torches shows where the dark spawns)
    PLAT_KEY_I, PLAT_KEY_K,       // heard by mods only (games/arpg's panels)
    PLAT_KEY_F11,                 // full screen

    PLAT_KEY_COUNT
} plat_key;

bool   plat_should_close(void);
float  plat_delta_time(void);
double plat_time(void);
int    plat_fps(void);

// By default the platform treats ESC as "close the window". The spike binds ESC
// to releasing the mouse, so it has to be unbound first.
void plat_disable_default_exit_key(void);

// The window over the whole monitor, borderless at the desktop's resolution,
// or back to the window it was. Asking for what it already is does nothing.
void plat_set_fullscreen(bool on);

bool plat_key_down(plat_key key);
bool plat_key_pressed(plat_key key);

// Pressed, or held long enough to repeat, as a key does in a line of text.
bool plat_key_repeated(plat_key key);

// While a line of text has the keyboard: every key but ESC, ENTER, BACKSPACE
// and the arrows reads as up, so typing a name does not also open the bag,
// pick a hotbar slot or fly -- one switch here rather than a check at every key
// the game reads. What was typed comes from plat_char_pressed.
void plat_text_input(bool on);

// The next character typed this frame, as a Unicode code point, or 0.
int plat_char_pressed(void);

// Mouse movement since the previous frame, in pixels.
void plat_mouse_delta(float *dx, float *dy);

typedef enum plat_mouse_button {
    PLAT_MOUSE_LEFT = 0,
    PLAT_MOUSE_RIGHT,
    PLAT_MOUSE_MIDDLE,
} plat_mouse_button;

bool plat_mouse_pressed(plat_mouse_button button);

// Held down, and let go this frame. A drag needs all three: pressed says where
// it started, down says it is still happening and something should follow the
// cursor, released says where it ended. A drag read from `pressed` alone is a
// click that guessed.
bool plat_mouse_down(plat_mouse_button button);
bool plat_mouse_released(plat_mouse_button button);

// Where the pointer is, in window pixels, origin top-left -- the same space
// gfx_2d_* draws in. Only meaningful while the cursor is NOT grabbed: a grabbed
// cursor is warped to the middle every frame and its position says nothing.
void plat_mouse_position(float *x, float *y);

// How far the wheel turned since the last frame, in notches. Positive is away
// from the person using it, which is the direction every desktop calls "up".
// Fractional on a trackpad, so a caller that wants whole steps has to round.
float plat_mouse_wheel(void);
void plat_cursor_grab(bool grabbed);
bool plat_cursor_grabbed(void);
// A grab that leaves the cursor seen and free to point (a camera from above):
// the world still has the mouse's buttons, the mouse no longer turns the view.
void plat_cursor_free(bool free);

// Whether this window has the keyboard, which on every desktop is also whether
// it may hold the pointer. See pause_frame for why the game has to ask.
bool plat_window_focused(void);

// Debug overlay only. A real UI arrives in M7 (microui or Clay, driven by Lua);
// this exists so the M0 exit criteria can be read off the screen.
void plat_debug_text(const char *text, int x, int y, int size);

// Writes the current frame to `name` in the process working directory. The
// platform strips any directory part, so pass a bare filename and chdir first.
// Exists for the scripted bench run: the exit criteria have to be reviewable
// after the fact, not just watchable live.
void plat_screenshot(const char *name);

#ifdef __cplusplus
} // extern "C"
#endif
