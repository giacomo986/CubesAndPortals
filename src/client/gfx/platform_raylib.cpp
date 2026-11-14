#include "client/gfx/platform.h"

#include <raylib.h>

#include <cstring>

namespace {

int default_key(plat_key k)
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
        case PLAT_KEY_H:         return KEY_H;
        default:             return KEY_NULL;
    }
}

// Which key each of the game's keys is now: the defaults above until the
// Controls page or the options file says otherwise. KEY_NULL is none.
int  g_bound[PLAT_KEY_COUNT];
bool g_bound_ready = false;

int to_raylib_key(plat_key k)
{
    if (!g_bound_ready) {
        for (int i = 0; i < PLAT_KEY_COUNT; ++i) g_bound[i] = default_key((plat_key)i);
        g_bound_ready = true;
    }
    return k >= 0 && k < PLAT_KEY_COUNT ? g_bound[k] : KEY_NULL;
}

// The names a key goes by in the options file's neighbours, the Controls page
// and core.register_key: what is printed on the key, short.
struct KeyName { int code; const char *name; };
const KeyName kNames[] = {
    {KEY_SPACE, "SPACE"}, {KEY_ESCAPE, "ESC"}, {KEY_ENTER, "ENTER"}, {KEY_TAB, "TAB"},
    {KEY_BACKSPACE, "BACKSPACE"}, {KEY_INSERT, "INSERT"}, {KEY_DELETE, "DELETE"},
    {KEY_RIGHT, "RIGHT"}, {KEY_LEFT, "LEFT"}, {KEY_DOWN, "DOWN"}, {KEY_UP, "UP"},
    {KEY_PAGE_UP, "PAGEUP"}, {KEY_PAGE_DOWN, "PAGEDOWN"}, {KEY_HOME, "HOME"}, {KEY_END, "END"},
    {KEY_CAPS_LOCK, "CAPSLOCK"}, {KEY_LEFT_SHIFT, "SHIFT"}, {KEY_RIGHT_SHIFT, "RSHIFT"},
    {KEY_LEFT_CONTROL, "CTRL"}, {KEY_RIGHT_CONTROL, "RCTRL"}, {KEY_LEFT_ALT, "ALT"},
    {KEY_RIGHT_ALT, "RALT"}, {KEY_APOSTROPHE, "'"}, {KEY_COMMA, ","}, {KEY_MINUS, "-"},
    {KEY_PERIOD, "."}, {KEY_SLASH, "/"}, {KEY_SEMICOLON, ";"}, {KEY_EQUAL, "="},
    {KEY_LEFT_BRACKET, "["}, {KEY_BACKSLASH, "\\"}, {KEY_RIGHT_BRACKET, "]"}, {KEY_GRAVE, "`"},
    {KEY_KP_0, "KP0"}, {KEY_KP_1, "KP1"}, {KEY_KP_2, "KP2"}, {KEY_KP_3, "KP3"}, {KEY_KP_4, "KP4"},
    {KEY_KP_5, "KP5"}, {KEY_KP_6, "KP6"}, {KEY_KP_7, "KP7"}, {KEY_KP_8, "KP8"}, {KEY_KP_9, "KP9"},
    {KEY_KP_DECIMAL, "KP."}, {KEY_KP_DIVIDE, "KP/"}, {KEY_KP_MULTIPLY, "KP*"},
    {KEY_KP_SUBTRACT, "KP-"}, {KEY_KP_ADD, "KP+"}, {KEY_KP_ENTER, "KPENTER"},
};

// A mouse button in a key's place: its code is kMouseCode + the button
// (2 the middle, 3 and 4 the side ones), past any key's.
constexpr int kMouseCode = 1000;
bool code_down(int code)
{
    if (code >= kMouseCode) return IsMouseButtonDown(code - kMouseCode);
    return code > 0 && IsKeyDown(code);
}
bool code_pressed(int code)
{
    if (code >= kMouseCode) return IsMouseButtonPressed(code - kMouseCode);
    return code > 0 && IsKeyPressed(code);
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
void plat_monitor_size(int *w, int *h)
{
    const int m = GetCurrentMonitor();
    *w = GetMonitorWidth(m);
    *h = GetMonitorHeight(m);
}

void plat_set_display(int mode)
{
    static int have_mode = 0, back_w = 0, back_h = 0;   // the window to come back to from full screen
    if (mode == have_mode) return;
    have_mode = mode;

    if (IsWindowFullscreen()) ToggleFullscreen();
    if (IsWindowState(FLAG_BORDERLESS_WINDOWED_MODE)) ToggleBorderlessWindowed();
    if (IsWindowMaximized()) RestoreWindow();   // or the manager keeps it filling the screen
    int mw = 0, mh = 0;
    plat_monitor_size(&mw, &mh);
    if (mode == 1) {
        ToggleBorderlessWindowed();
    } else if (mode == 2) {
        // ToggleFullscreen asks the monitor for the window's size.
        back_w = GetScreenWidth(), back_h = GetScreenHeight();
        SetWindowSize(mw, mh);
        ToggleFullscreen();
    } else if (back_w > 0) {
        SetWindowSize(back_w, back_h);
        SetWindowPosition((mw - back_w) / 2, (mh - back_h) / 2);
        back_w = 0;
    }
}

void plat_window_size(int *w, int *h)
{
    *w = GetScreenWidth();
    *h = GetScreenHeight();
}

void plat_set_window_size(int w, int h)
{
    int mw = 0, mh = 0;
    plat_monitor_size(&mw, &mh);
    if (w > mw) w = mw;
    if (h > mh) h = mh;
    SetWindowSize(w, h);
    SetWindowPosition((mw - w) / 2, (mh - h) / 2);
}

void plat_set_vsync(bool on)
{
    static bool have = false;   // gfx_init opens the window without it
    if (on == have) return;
    have = on;
    if (on) SetWindowState(FLAG_VSYNC_HINT);
    else ClearWindowState(FLAG_VSYNC_HINT);
}

void plat_set_fps_limit(int fps)
{
    static int have = 0;
    if (fps != have) SetTargetFPS(have = fps);
}

float  plat_delta_time(void) { return GetFrameTime(); }
double plat_time(void)       { return GetTime(); }
int    plat_fps(void)        { return GetFPS(); }

bool plat_key_down(plat_key key)    { return reaches_game(key) && code_down(to_raylib_key(key)); }
bool plat_key_pressed(plat_key key) { return reaches_game(key) && code_pressed(to_raylib_key(key)); }
bool plat_key_repeated(plat_key key)
{
    const int code = to_raylib_key(key);
    return reaches_game(key) &&
           (code_pressed(code) || (code < kMouseCode && code > 0 && IsKeyPressedRepeat(code)));
}

int  plat_key_code(plat_key key)    { return to_raylib_key(key); }
int  plat_key_default(plat_key key) { return default_key(key); }
void plat_bind_key(plat_key key, int code)
{
    to_raylib_key(key);   // the table filled first
    if (key >= 0 && key < PLAT_KEY_COUNT) g_bound[key] = code;
}

bool plat_code_down(int code)
{
    return code != KEY_NULL && !g_text_input && code_down(code);
}

int plat_code_pressed(void)
{
    if (const int k = GetKeyPressed(); k != 0) return k;
    // The middle and side buttons; the left and right are the menus' clicks.
    for (int b = 2; b <= 4; ++b)
        if (IsMouseButtonPressed(b)) return kMouseCode + b;
    return 0;
}

const char *plat_code_name(int code)
{
    static char mouse[8] = "MOUSE0";
    if (code >= kMouseCode && code <= kMouseCode + 6) {
        mouse[5] = (char)('1' + code - kMouseCode);
        return mouse;
    }
    static char one[2];
    if ((code >= KEY_A && code <= KEY_Z) || (code >= KEY_ZERO && code <= KEY_NINE)) {
        one[0] = (char)code, one[1] = '\0';
        return one;
    }
    static char fn[4];
    if (code >= KEY_F1 && code <= KEY_F12) {
        fn[0] = 'F';
        const int n = code - KEY_F1 + 1;
        if (n < 10) fn[1] = (char)('0' + n), fn[2] = '\0';
        else fn[1] = '1', fn[2] = (char)('0' + n - 10), fn[3] = '\0';
        return fn;
    }
    for (const KeyName &k : kNames)
        if (k.code == code) return k.name;
    return "";
}

int plat_code_from_name(const char *name)
{
    if (name == nullptr || name[0] == '\0') return KEY_NULL;
    char up[16];
    size_t n = 0;
    for (; name[n] != '\0' && n + 1 < sizeof(up); ++n)
        up[n] = (name[n] >= 'a' && name[n] <= 'z') ? (char)(name[n] - 32) : name[n];
    up[n] = '\0';
    if (n == 1 && ((up[0] >= 'A' && up[0] <= 'Z') || (up[0] >= '0' && up[0] <= '9'))) return up[0];
    if (n == 6 && std::strncmp(up, "MOUSE", 5) == 0 && up[5] >= '1' && up[5] <= '7') return kMouseCode + (up[5] - '1');
    if (up[0] == 'F' && n >= 2 && n <= 3) {
        int k = 0;
        for (size_t i = 1; i < n; ++i) {
            if (up[i] < '0' || up[i] > '9') { k = -1; break; }
            k = k * 10 + (up[i] - '0');
        }
        if (k >= 1 && k <= 12) return KEY_F1 + k - 1;
    }
    for (const KeyName &k : kNames)
        if (std::strcmp(k.name, up) == 0) return k.code;
    return KEY_NULL;
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

static FilePathList g_dropped = {};

extern "C" int plat_take_dropped(void)
{
    if (g_dropped.paths != nullptr) UnloadDroppedFiles(g_dropped);
    g_dropped = FilePathList{};
    if (!IsFileDropped()) return 0;
    g_dropped = LoadDroppedFiles();
    return (int)g_dropped.count;
}

extern "C" const char *plat_dropped_path(int i)
{
    return i >= 0 && (unsigned)i < g_dropped.count ? g_dropped.paths[i] : "";
}

