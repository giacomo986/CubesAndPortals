#include "client/gfx/platform.h"

#include <raylib.h>

namespace {

int to_raylib_key(plat_key k)
{
    switch (k) {
        case PLAT_KEY_W:     return KEY_W;
        case PLAT_KEY_A:     return KEY_A;
        case PLAT_KEY_S:     return KEY_S;
        case PLAT_KEY_D:     return KEY_D;
        case PLAT_KEY_SPACE: return KEY_SPACE;
        case PLAT_KEY_SHIFT: return KEY_LEFT_SHIFT;
        case PLAT_KEY_CTRL:  return KEY_LEFT_CONTROL;
        case PLAT_KEY_ESC:   return KEY_ESCAPE;
        case PLAT_KEY_TAB:   return KEY_TAB;
        case PLAT_KEY_F:     return KEY_F;
        case PLAT_KEY_V:     return KEY_V;
        case PLAT_KEY_C:     return KEY_C;
        case PLAT_KEY_R:     return KEY_R;
        case PLAT_KEY_P:     return KEY_P;
        case PLAT_KEY_E:     return KEY_E;
        case PLAT_KEY_Q:     return KEY_Q;
        case PLAT_KEY_Z:     return KEY_Z;
        case PLAT_KEY_X:     return KEY_X;
        case PLAT_KEY_B:     return KEY_B;
        case PLAT_KEY_F1:    return KEY_F1;
        case PLAT_KEY_F2:    return KEY_F2;
        case PLAT_KEY_F3:    return KEY_F3;
        case PLAT_KEY_F4:    return KEY_F4;
        case PLAT_KEY_F8:    return KEY_F8;
        case PLAT_KEY_I:     return KEY_I;
        case PLAT_KEY_K:     return KEY_K;
        case PLAT_KEY_F5:    return KEY_F5;
        case PLAT_KEY_F6:    return KEY_F6;
        case PLAT_KEY_F7:    return KEY_F7;
        case PLAT_KEY_F11:   return KEY_F11;
        case PLAT_KEY_1:     return KEY_ONE;
        case PLAT_KEY_2:     return KEY_TWO;
        case PLAT_KEY_3:     return KEY_THREE;
        case PLAT_KEY_4:     return KEY_FOUR;
        case PLAT_KEY_5:     return KEY_FIVE;
        case PLAT_KEY_6:     return KEY_SIX;
        case PLAT_KEY_7:     return KEY_SEVEN;
        case PLAT_KEY_8:     return KEY_EIGHT;
        case PLAT_KEY_9:     return KEY_NINE;
        case PLAT_KEY_ENTER:     return KEY_ENTER;
        case PLAT_KEY_BACKSPACE: return KEY_BACKSPACE;
        case PLAT_KEY_UP:        return KEY_UP;
        case PLAT_KEY_DOWN:      return KEY_DOWN;
        default:             return KEY_NULL;
    }
}

bool g_cursor_grabbed = false;
bool g_cursor_free    = false;
bool g_text_input     = false;

bool reaches_game(plat_key key)
{
    return !g_text_input || key == PLAT_KEY_ESC || key == PLAT_KEY_ENTER ||
           key == PLAT_KEY_BACKSPACE || key == PLAT_KEY_UP || key == PLAT_KEY_DOWN;
}

} // namespace

bool plat_should_close(void)
{
    return WindowShouldClose();
}

void plat_disable_default_exit_key(void)
{
    // Otherwise WindowShouldClose() reports true on ESC, and the spike would
    // quit every time the player released the mouse.
    SetExitKey(KEY_NULL);
}

// Borderless and not exclusive: the monitor keeps its mode, so switching is
// instant and alt-tab does not blank the screen.
void plat_set_fullscreen(bool on)
{
    if (on != IsWindowState(FLAG_BORDERLESS_WINDOWED_MODE)) ToggleBorderlessWindowed();
}

float  plat_delta_time(void) { return GetFrameTime(); }
double plat_time(void)       { return GetTime(); }
int    plat_fps(void)        { return GetFPS(); }

bool plat_key_down(plat_key key)    { return reaches_game(key) && IsKeyDown(to_raylib_key(key)); }
bool plat_key_pressed(plat_key key) { return reaches_game(key) && IsKeyPressed(to_raylib_key(key)); }
bool plat_key_repeated(plat_key key)
{
    return reaches_game(key) &&
           (IsKeyPressed(to_raylib_key(key)) || IsKeyPressedRepeat(to_raylib_key(key)));
}

void plat_text_input(bool on) { g_text_input = on; }
int  plat_char_pressed(void)  { return GetCharPressed(); }

void plat_mouse_delta(float *dx, float *dy)
{
    const Vector2 d = GetMouseDelta();
    if (dx) *dx = d.x;
    if (dy) *dy = d.y;
}

bool plat_mouse_pressed(plat_mouse_button button)
{
    switch (button) {
        case PLAT_MOUSE_LEFT:   return IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
        case PLAT_MOUSE_RIGHT:  return IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);
        case PLAT_MOUSE_MIDDLE: return IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE);
    }
    return false;
}

bool plat_mouse_down(plat_mouse_button button)
{
    switch (button) {
        case PLAT_MOUSE_LEFT:   return IsMouseButtonDown(MOUSE_BUTTON_LEFT);
        case PLAT_MOUSE_RIGHT:  return IsMouseButtonDown(MOUSE_BUTTON_RIGHT);
        case PLAT_MOUSE_MIDDLE: return IsMouseButtonDown(MOUSE_BUTTON_MIDDLE);
    }
    return false;
}

bool plat_mouse_released(plat_mouse_button button)
{
    switch (button) {
        case PLAT_MOUSE_LEFT:   return IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
        case PLAT_MOUSE_RIGHT:  return IsMouseButtonReleased(MOUSE_BUTTON_RIGHT);
        case PLAT_MOUSE_MIDDLE: return IsMouseButtonReleased(MOUSE_BUTTON_MIDDLE);
    }
    return false;
}

void plat_mouse_position(float *x, float *y)
{
    const Vector2 p = GetMousePosition();
    if (x) *x = p.x;
    if (y) *y = p.y;
}

float plat_mouse_wheel(void)
{
    return GetMouseWheelMove();
}

void plat_cursor_grab(bool grabbed)
{
    if (grabbed == g_cursor_grabbed) return;
    g_cursor_grabbed = grabbed;
    if (grabbed && !g_cursor_free) DisableCursor();
    else                           EnableCursor();
}

void plat_cursor_free(bool free)
{
    if (free == g_cursor_free) return;
    g_cursor_free = free;
    if (!g_cursor_grabbed) return;
    if (free) EnableCursor();
    else      DisableCursor();
}

bool plat_cursor_grabbed(void) { return g_cursor_grabbed; }
bool plat_window_focused(void) { return IsWindowFocused(); }

void plat_debug_text(const char *text, int x, int y, int size)
{
    DrawText(text, x, y, size, RAYWHITE);
}

void plat_screenshot(const char *name)
{
    TakeScreenshot(name);
}
