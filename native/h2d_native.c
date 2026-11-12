#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_mixer.h>
#include <SDL2/SDL2_gfxPrimitives.h>
#include <SDL2/SDL_ttf.h>

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* ── fixed-size handle tables ──────────────────────────────────────── */

#define H2D_MAX_TEXTURES 4096
#define H2D_MAX_SOUNDS   1024
#define H2D_MAX_MUSIC    64
#define H2D_MAX_KEYS     256

static SDL_Window   *g_window   = NULL;
static SDL_Renderer *g_renderer = NULL;

static SDL_Texture *g_textures[H2D_MAX_TEXTURES];
static int          g_texture_w[H2D_MAX_TEXTURES];
static int          g_texture_h[H2D_MAX_TEXTURES];
static int          g_texture_next = 1; /* 0 is reserved for "invalid handle" */

static Mix_Chunk *g_sounds[H2D_MAX_SOUNDS];
static int        g_sound_next = 1;

static Mix_Music *g_music[H2D_MAX_MUSIC];
static int        g_music_next = 1;

static int g_audio_ready = 0;

#define H2D_MAX_FONTS    64
#define H2D_MAX_PADS     4
#define H2D_TEXT_CACHE   128
#define H2D_CHAR_QUEUE   64

static TTF_Font *g_fonts[H2D_MAX_FONTS];
static int       g_font_next = 1;

typedef struct { int font; char *text; SDL_Texture *tex; int w, h; uint64_t used; } TextCacheEntry;
static TextCacheEntry g_text_cache[H2D_TEXT_CACHE];

static int g_logical_w = 0, g_logical_h = 0;
static int g_blend = 1;                 /* 0 none, 1 alpha, 2 add, 3 mod */
static int g_mouse_dx = 0, g_mouse_dy = 0;
static int g_char_queue[H2D_CHAR_QUEUE];
static int g_char_head = 0, g_char_tail = 0;
static int g_first_key_pressed = -1;
/* Edge latches: a press AND release inside one frame (fast tap, low fps,
 * touch) must still register as "pressed"/"released" for that frame. */
static uint8_t g_key_down_edge[512], g_key_up_edge[512]; /* >= H2D_KEY_COUNT */
static int g_mouse_down_edge = 0, g_mouse_up_edge = 0;

static SDL_GameController *g_pads[H2D_MAX_PADS];
static uint32_t g_pad_btn_now[H2D_MAX_PADS], g_pad_btn_prev[H2D_MAX_PADS];

static uint64_t g_frame_count = 0;
static uint64_t g_max_frames = 0;       /* env H2D_MAX_FRAMES: auto-quit (CI / smoke tests) */
static char     g_shot_path[512] = "";  /* env H2D_SCREENSHOT: PNG written on the last frame */
static double   g_fps_smooth = 0.0;

/* Keyboard state, snapshotted once per h2d_begin_frame() call so
 * is_key_pressed/is_key_released can compare "this frame" vs "last
 * frame" without H# itself having to track it. Indexed by our own
 * stable H2D_KEY_* codes (see h2d_key_to_scancode below), not raw SDL
 * scancodes, so the H# side never has to know an SDL enum value. */
static uint8_t g_keys_now[H2D_MAX_KEYS];
static uint8_t g_keys_prev[H2D_MAX_KEYS];

static int g_mouse_x = 0, g_mouse_y = 0;
static int g_mouse_buttons_now = 0;  /* bitmask: 1=left 2=middle 4=right */
static int g_mouse_buttons_prev = 0;
static double g_mouse_wheel = 0.0;

static int g_should_close = 0;
static int g_target_fps = 60;
static double g_frame_time = 0.0;
static uint64_t g_perf_freq = 0;
static uint64_t g_frame_start_counter = 0;
static uint64_t g_app_start_counter = 0;

/* ── stable key codes — MUST match src/h2d_keys.h#'s key_*() functions
 *    one-for-one; if you add a key here, add it there too. ──────────── */
enum {
    H2D_KEY_A = 0, H2D_KEY_B, H2D_KEY_C, H2D_KEY_D, H2D_KEY_E, H2D_KEY_F,
    H2D_KEY_G, H2D_KEY_H, H2D_KEY_I, H2D_KEY_J, H2D_KEY_K, H2D_KEY_L,
    H2D_KEY_M, H2D_KEY_N, H2D_KEY_O, H2D_KEY_P, H2D_KEY_Q, H2D_KEY_R,
    H2D_KEY_S, H2D_KEY_T, H2D_KEY_U, H2D_KEY_V, H2D_KEY_W, H2D_KEY_X,
    H2D_KEY_Y, H2D_KEY_Z,
    H2D_KEY_0, H2D_KEY_1, H2D_KEY_2, H2D_KEY_3, H2D_KEY_4,
    H2D_KEY_5, H2D_KEY_6, H2D_KEY_7, H2D_KEY_8, H2D_KEY_9,
    H2D_KEY_UP, H2D_KEY_DOWN, H2D_KEY_LEFT, H2D_KEY_RIGHT,
    H2D_KEY_SPACE, H2D_KEY_ENTER, H2D_KEY_ESCAPE, H2D_KEY_TAB,
    H2D_KEY_LSHIFT, H2D_KEY_RSHIFT, H2D_KEY_LCTRL, H2D_KEY_RCTRL,
    H2D_KEY_BACKSPACE,
    H2D_KEY_F1, H2D_KEY_F2, H2D_KEY_F3, H2D_KEY_F4, H2D_KEY_F5, H2D_KEY_F6,
    H2D_KEY_F7, H2D_KEY_F8, H2D_KEY_F9, H2D_KEY_F10, H2D_KEY_F11, H2D_KEY_F12,
    H2D_KEY_LALT, H2D_KEY_RALT, H2D_KEY_DELETE, H2D_KEY_INSERT,
    H2D_KEY_HOME, H2D_KEY_END, H2D_KEY_PAGEUP, H2D_KEY_PAGEDOWN,
    H2D_KEY_MINUS, H2D_KEY_EQUALS, H2D_KEY_COMMA, H2D_KEY_PERIOD, H2D_KEY_SLASH,
    H2D_KEY_SEMICOLON, H2D_KEY_APOSTROPHE, H2D_KEY_LBRACKET, H2D_KEY_RBRACKET,
    H2D_KEY_BACKSLASH, H2D_KEY_GRAVE,
    H2D_KEY_KP0, H2D_KEY_KP1, H2D_KEY_KP2, H2D_KEY_KP3, H2D_KEY_KP4,
    H2D_KEY_KP5, H2D_KEY_KP6, H2D_KEY_KP7, H2D_KEY_KP8, H2D_KEY_KP9,
    H2D_KEY_COUNT
};

static SDL_Scancode h2d_key_to_scancode(int key) {
    switch (key) {
        case H2D_KEY_A: return SDL_SCANCODE_A;
        case H2D_KEY_B: return SDL_SCANCODE_B;
        case H2D_KEY_C: return SDL_SCANCODE_C;
        case H2D_KEY_D: return SDL_SCANCODE_D;
        case H2D_KEY_E: return SDL_SCANCODE_E;
        case H2D_KEY_F: return SDL_SCANCODE_F;
        case H2D_KEY_G: return SDL_SCANCODE_G;
        case H2D_KEY_H: return SDL_SCANCODE_H;
        case H2D_KEY_I: return SDL_SCANCODE_I;
        case H2D_KEY_J: return SDL_SCANCODE_J;
        case H2D_KEY_K: return SDL_SCANCODE_K;
        case H2D_KEY_L: return SDL_SCANCODE_L;
        case H2D_KEY_M: return SDL_SCANCODE_M;
        case H2D_KEY_N: return SDL_SCANCODE_N;
        case H2D_KEY_O: return SDL_SCANCODE_O;
        case H2D_KEY_P: return SDL_SCANCODE_P;
        case H2D_KEY_Q: return SDL_SCANCODE_Q;
        case H2D_KEY_R: return SDL_SCANCODE_R;
        case H2D_KEY_S: return SDL_SCANCODE_S;
        case H2D_KEY_T: return SDL_SCANCODE_T;
        case H2D_KEY_U: return SDL_SCANCODE_U;
        case H2D_KEY_V: return SDL_SCANCODE_V;
        case H2D_KEY_W: return SDL_SCANCODE_W;
        case H2D_KEY_X: return SDL_SCANCODE_X;
        case H2D_KEY_Y: return SDL_SCANCODE_Y;
        case H2D_KEY_Z: return SDL_SCANCODE_Z;
        case H2D_KEY_0: return SDL_SCANCODE_0;
        case H2D_KEY_1: return SDL_SCANCODE_1;
        case H2D_KEY_2: return SDL_SCANCODE_2;
        case H2D_KEY_3: return SDL_SCANCODE_3;
        case H2D_KEY_4: return SDL_SCANCODE_4;
        case H2D_KEY_5: return SDL_SCANCODE_5;
        case H2D_KEY_6: return SDL_SCANCODE_6;
        case H2D_KEY_7: return SDL_SCANCODE_7;
        case H2D_KEY_8: return SDL_SCANCODE_8;
        case H2D_KEY_9: return SDL_SCANCODE_9;
        case H2D_KEY_UP: return SDL_SCANCODE_UP;
        case H2D_KEY_DOWN: return SDL_SCANCODE_DOWN;
        case H2D_KEY_LEFT: return SDL_SCANCODE_LEFT;
        case H2D_KEY_RIGHT: return SDL_SCANCODE_RIGHT;
        case H2D_KEY_SPACE: return SDL_SCANCODE_SPACE;
        case H2D_KEY_ENTER: return SDL_SCANCODE_RETURN;
        case H2D_KEY_ESCAPE: return SDL_SCANCODE_ESCAPE;
        case H2D_KEY_TAB: return SDL_SCANCODE_TAB;
        case H2D_KEY_LSHIFT: return SDL_SCANCODE_LSHIFT;
        case H2D_KEY_RSHIFT: return SDL_SCANCODE_RSHIFT;
        case H2D_KEY_LCTRL: return SDL_SCANCODE_LCTRL;
        case H2D_KEY_RCTRL: return SDL_SCANCODE_RCTRL;
        case H2D_KEY_BACKSPACE: return SDL_SCANCODE_BACKSPACE;
        case H2D_KEY_F1: return SDL_SCANCODE_F1;
        case H2D_KEY_F2: return SDL_SCANCODE_F2;
        case H2D_KEY_F3: return SDL_SCANCODE_F3;
        case H2D_KEY_F4: return SDL_SCANCODE_F4;
        case H2D_KEY_F5: return SDL_SCANCODE_F5;
        case H2D_KEY_F6: return SDL_SCANCODE_F6;
        case H2D_KEY_F7: return SDL_SCANCODE_F7;
        case H2D_KEY_F8: return SDL_SCANCODE_F8;
        case H2D_KEY_F9: return SDL_SCANCODE_F9;
        case H2D_KEY_F10: return SDL_SCANCODE_F10;
        case H2D_KEY_F11: return SDL_SCANCODE_F11;
        case H2D_KEY_F12: return SDL_SCANCODE_F12;
        case H2D_KEY_LALT: return SDL_SCANCODE_LALT;
        case H2D_KEY_RALT: return SDL_SCANCODE_RALT;
        case H2D_KEY_DELETE: return SDL_SCANCODE_DELETE;
        case H2D_KEY_INSERT: return SDL_SCANCODE_INSERT;
        case H2D_KEY_HOME: return SDL_SCANCODE_HOME;
        case H2D_KEY_END: return SDL_SCANCODE_END;
        case H2D_KEY_PAGEUP: return SDL_SCANCODE_PAGEUP;
        case H2D_KEY_PAGEDOWN: return SDL_SCANCODE_PAGEDOWN;
        case H2D_KEY_MINUS: return SDL_SCANCODE_MINUS;
        case H2D_KEY_EQUALS: return SDL_SCANCODE_EQUALS;
        case H2D_KEY_COMMA: return SDL_SCANCODE_COMMA;
        case H2D_KEY_PERIOD: return SDL_SCANCODE_PERIOD;
        case H2D_KEY_SLASH: return SDL_SCANCODE_SLASH;
        case H2D_KEY_SEMICOLON: return SDL_SCANCODE_SEMICOLON;
        case H2D_KEY_APOSTROPHE: return SDL_SCANCODE_APOSTROPHE;
        case H2D_KEY_LBRACKET: return SDL_SCANCODE_LEFTBRACKET;
        case H2D_KEY_RBRACKET: return SDL_SCANCODE_RIGHTBRACKET;
        case H2D_KEY_BACKSLASH: return SDL_SCANCODE_BACKSLASH;
        case H2D_KEY_GRAVE: return SDL_SCANCODE_GRAVE;
        case H2D_KEY_KP0: return SDL_SCANCODE_KP_0;
        case H2D_KEY_KP1: return SDL_SCANCODE_KP_1;
        case H2D_KEY_KP2: return SDL_SCANCODE_KP_2;
        case H2D_KEY_KP3: return SDL_SCANCODE_KP_3;
        case H2D_KEY_KP4: return SDL_SCANCODE_KP_4;
        case H2D_KEY_KP5: return SDL_SCANCODE_KP_5;
        case H2D_KEY_KP6: return SDL_SCANCODE_KP_6;
        case H2D_KEY_KP7: return SDL_SCANCODE_KP_7;
        case H2D_KEY_KP8: return SDL_SCANCODE_KP_8;
        case H2D_KEY_KP9: return SDL_SCANCODE_KP_9;
        default: return SDL_SCANCODE_UNKNOWN;
    }
}

/* ── window / frame lifecycle ──────────────────────────────────────── */

int64_t h2d_init_window(int64_t width, int64_t height, const char *title) {
    if (g_window != NULL) return 1; /* already open */

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0) {
        return 0;
    }
    /* Audio is opt-in via h2d_audio_init() — some games are silent /
     * some sandboxes have no audio device, and SDL_Init(AUDIO) failing
     * there shouldn't block opening a window. */

    g_window = SDL_CreateWindow(
        title ? title : "H2D",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        (int)width, (int)height, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );
    if (!g_window) return 0;

    g_renderer = SDL_CreateRenderer(
        g_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );
    if (!g_renderer) {
        /* Fall back to a software renderer rather than failing outright —
         * headless/CI/remote-desktop environments often have no GPU. */
        g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (!g_renderer) {
        SDL_DestroyWindow(g_window);
        g_window = NULL;
        return 0;
    }

    SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);

    memset(g_textures, 0, sizeof(g_textures));
    memset(g_fonts, 0, sizeof(g_fonts));
    memset(g_text_cache, 0, sizeof(g_text_cache));
    memset(g_pads, 0, sizeof(g_pads));
    g_font_next = 1; g_texture_next = 1; g_sound_next = 1; g_music_next = 1;
    g_frame_count = 0; g_logical_w = g_logical_h = 0; g_blend = 1;
    g_char_head = g_char_tail = 0;
    IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
    TTF_Init();
    {
        const char *mf = getenv("H2D_MAX_FRAMES");
        g_max_frames = mf ? (uint64_t)strtoull(mf, NULL, 10) : 0;
        const char *sp = getenv("H2D_SCREENSHOT");
        if (sp) { strncpy(g_shot_path, sp, sizeof(g_shot_path) - 1); g_shot_path[sizeof(g_shot_path) - 1] = 0; }
        else g_shot_path[0] = 0;
    }
    for (int i = 0; i < SDL_NumJoysticks() && i < H2D_MAX_PADS; i++)
        if (SDL_IsGameController(i)) g_pads[i] = SDL_GameControllerOpen(i);
    memset(g_sounds, 0, sizeof(g_sounds));
    memset(g_music, 0, sizeof(g_music));
    memset(g_keys_now, 0, sizeof(g_keys_now));
    memset(g_keys_prev, 0, sizeof(g_keys_prev));

    g_perf_freq = SDL_GetPerformanceFrequency();
    g_app_start_counter = SDL_GetPerformanceCounter();
    g_frame_start_counter = g_app_start_counter;
    g_should_close = 0;

    return 1;
}

int64_t h2d_window_should_close(void) {
    return g_should_close;
}

static void h2d_text_cache_clear(void) {
    for (int i = 0; i < H2D_TEXT_CACHE; i++) {
        if (g_text_cache[i].tex) SDL_DestroyTexture(g_text_cache[i].tex);
        free(g_text_cache[i].text);
        memset(&g_text_cache[i], 0, sizeof(g_text_cache[i]));
    }
}

void h2d_close_window(void) {
    if (!g_window && !g_renderer) return; /* already closed: idempotent */
    h2d_text_cache_clear();
    for (int i = 0; i < H2D_MAX_FONTS; i++) {
        if (g_fonts[i]) { TTF_CloseFont(g_fonts[i]); g_fonts[i] = NULL; }
    }
    for (int i = 0; i < H2D_MAX_PADS; i++) {
        if (g_pads[i]) { SDL_GameControllerClose(g_pads[i]); g_pads[i] = NULL; }
    }
    TTF_Quit();
    IMG_Quit();
    for (int i = 0; i < H2D_MAX_TEXTURES; i++) {
        if (g_textures[i]) { SDL_DestroyTexture(g_textures[i]); g_textures[i] = NULL; }
    }
    for (int i = 0; i < H2D_MAX_SOUNDS; i++) {
        if (g_sounds[i]) { Mix_FreeChunk(g_sounds[i]); g_sounds[i] = NULL; }
    }
    for (int i = 0; i < H2D_MAX_MUSIC; i++) {
        if (g_music[i]) { Mix_FreeMusic(g_music[i]); g_music[i] = NULL; }
    }
    if (g_audio_ready) { Mix_CloseAudio(); g_audio_ready = 0; }
    if (g_renderer) { SDL_DestroyRenderer(g_renderer); g_renderer = NULL; }
    if (g_window) { SDL_DestroyWindow(g_window); g_window = NULL; }
    SDL_Quit();
}

void h2d_set_target_fps(int64_t fps) {
    g_target_fps = (int)fps;
}

/* Poll OS events, refresh keyboard/mouse snapshots. Call once at the
 * very start of every frame, before reading any input or drawing. */
void h2d_begin_frame(void) {
    if (!g_window) return;
    memcpy(g_keys_prev, g_keys_now, sizeof(g_keys_now));
    g_mouse_buttons_prev = g_mouse_buttons_now;
    memcpy(g_pad_btn_prev, g_pad_btn_now, sizeof(g_pad_btn_now));
    g_mouse_wheel = 0.0;
    g_mouse_dx = g_mouse_dy = 0;
    g_first_key_pressed = -1;
    memset(g_key_down_edge, 0, sizeof(g_key_down_edge));
    memset(g_key_up_edge, 0, sizeof(g_key_up_edge));
    g_mouse_down_edge = g_mouse_up_edge = 0;

    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        switch (ev.type) {
            case SDL_QUIT:
                g_should_close = 1;
                break;
            case SDL_WINDOWEVENT:
                if (ev.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
                    /* no stuck keys when the window loses focus */
                    memset(g_keys_now, 0, sizeof(g_keys_now));
                    g_mouse_buttons_now = 0;
                }
                break;
            case SDL_KEYDOWN:
            case SDL_KEYUP: {
                SDL_Scancode sc = ev.key.keysym.scancode;
                int down = (ev.type == SDL_KEYDOWN) ? 1 : 0;
                if (down && ev.key.repeat) break;
                for (int k = 0; k < H2D_KEY_COUNT; k++) {
                    if (h2d_key_to_scancode(k) == sc) {
                        g_keys_now[k] = (uint8_t)down;
                        if (down) g_key_down_edge[k] = 1; else g_key_up_edge[k] = 1;
                        if (down && g_first_key_pressed < 0) g_first_key_pressed = k;
                        break;
                    }
                }
                break;
            }
            case SDL_TEXTINPUT: {
                /* decode UTF-8 into code points */
                const unsigned char *t = (const unsigned char *)ev.text.text;
                while (*t) {
                    int cp = 0, n = 1;
                    if (*t < 0x80) cp = *t;
                    else if ((*t & 0xE0) == 0xC0) { cp = *t & 0x1F; n = 2; }
                    else if ((*t & 0xF0) == 0xE0) { cp = *t & 0x0F; n = 3; }
                    else { cp = *t & 0x07; n = 4; }
                    for (int i = 1; i < n && t[i]; i++) cp = (cp << 6) | (t[i] & 0x3F);
                    int next = (g_char_tail + 1) % H2D_CHAR_QUEUE;
                    if (next != g_char_head) { g_char_queue[g_char_tail] = cp; g_char_tail = next; }
                    t += n;
                }
                break;
            }
            case SDL_MOUSEMOTION:
                g_mouse_dx += ev.motion.xrel;
                g_mouse_dy += ev.motion.yrel;
                break;
            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEBUTTONUP: {
                int mask = 0;
                if (ev.button.button == SDL_BUTTON_LEFT) mask = 1;
                else if (ev.button.button == SDL_BUTTON_MIDDLE) mask = 2;
                else if (ev.button.button == SDL_BUTTON_RIGHT) mask = 4;
                if (ev.type == SDL_MOUSEBUTTONDOWN) { g_mouse_buttons_now |= mask; g_mouse_down_edge |= mask; }
                else { g_mouse_buttons_now &= ~mask; g_mouse_up_edge |= mask; }
                break;
            }
            case SDL_MOUSEWHEEL:
                g_mouse_wheel += (double)ev.wheel.y;
                break;
            case SDL_CONTROLLERDEVICEADDED:
                if (ev.cdevice.which < H2D_MAX_PADS && !g_pads[ev.cdevice.which])
                    g_pads[ev.cdevice.which] = SDL_GameControllerOpen(ev.cdevice.which);
                break;
            case SDL_CONTROLLERDEVICEREMOVED:
                for (int i = 0; i < H2D_MAX_PADS; i++) {
                    if (g_pads[i] && SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(g_pads[i])) == ev.cdevice.which) {
                        SDL_GameControllerClose(g_pads[i]); g_pads[i] = NULL; g_pad_btn_now[i] = 0;
                    }
                }
                break;
            default:
                break;
        }
    }

    /* mouse position in LOGICAL coordinates when h2d_set_logical_size is active */
    {
        int mx = 0, my = 0;
        SDL_GetMouseState(&mx, &my);
        float lx = (float)mx, ly = (float)my;
        if (g_renderer && g_logical_w > 0) SDL_RenderWindowToLogical(g_renderer, mx, my, &lx, &ly);
        g_mouse_x = (int)lx; g_mouse_y = (int)ly;
    }
    for (int i = 0; i < H2D_MAX_PADS; i++) {
        uint32_t m = 0;
        if (g_pads[i]) {
            for (int b = 0; b < 15; b++)
                if (SDL_GameControllerGetButton(g_pads[i], (SDL_GameControllerButton)b)) m |= (1u << b);
        }
        g_pad_btn_now[i] = m;
    }
}

/* Present the frame and, if vsync isn't doing the job (software
 * renderer fallback, uncapped display), sleep off the rest of the
 * frame budget implied by h2d_set_target_fps. Also updates
 * h2d_get_frame_time()'s delta. Call once at the very end of every
 * frame, after all drawing. */
static int h2d_save_png(const char *path);

void h2d_end_frame(void) {
    if (!g_renderer) return;
    g_frame_count++;
    if (g_max_frames > 0 && g_frame_count >= g_max_frames) {
        if (g_shot_path[0]) h2d_save_png(g_shot_path);
        g_should_close = 1;
    }
    SDL_RenderPresent(g_renderer);

    uint64_t now = SDL_GetPerformanceCounter();
    double elapsed = (double)(now - g_frame_start_counter) / (double)g_perf_freq;

    if (g_target_fps > 0) {
        double budget = 1.0 / (double)g_target_fps;
        if (elapsed < budget) {
            SDL_Delay((Uint32)((budget - elapsed) * 1000.0));
            now = SDL_GetPerformanceCounter();
            elapsed = (double)(now - g_frame_start_counter) / (double)g_perf_freq;
        }
    }

    if (elapsed > 0.25) elapsed = 0.25; /* dragging/suspending the window must not explode physics */
    g_frame_time = elapsed;
    if (elapsed > 0.0) g_fps_smooth = (g_fps_smooth == 0.0) ? 1.0 / elapsed : g_fps_smooth * 0.9 + (1.0 / elapsed) * 0.1;
    g_frame_start_counter = now;
}

double h2d_get_frame_time(void) { return g_frame_time; }

double h2d_get_time(void) {
    uint64_t now = SDL_GetPerformanceCounter();
    return (double)(now - g_app_start_counter) / (double)g_perf_freq;
}

int64_t h2d_screen_width(void) {
    if (g_logical_w > 0) return g_logical_w;
    int w = 0, h = 0;
    if (g_window) SDL_GetWindowSize(g_window, &w, &h);
    return w;
}

int64_t h2d_screen_height(void) {
    if (g_logical_h > 0) return g_logical_h;
    int w = 0, h = 0;
    if (g_window) SDL_GetWindowSize(g_window, &w, &h);
    return h;
}

void h2d_set_window_title(const char *title) {
    if (g_window) SDL_SetWindowTitle(g_window, title ? title : "");
}

/* ── drawing primitives (0-255 rgba throughout) ────────────────────── */

void h2d_clear_background(int64_t r, int64_t g, int64_t b, int64_t a) {
    if (!g_renderer) return;
    SDL_SetRenderDrawColor(g_renderer, (Uint8)r, (Uint8)g, (Uint8)b, (Uint8)a);
    SDL_RenderClear(g_renderer);
}

static SDL_BlendMode h2d_blend_mode(void) {
    switch (g_blend) {
        case 0: return SDL_BLENDMODE_NONE;
        case 2: return SDL_BLENDMODE_ADD;
        case 3: return SDL_BLENDMODE_MOD;
        default: return SDL_BLENDMODE_BLEND;
    }
}

void h2d_draw_rectangle(double x, double y, double w, double h, int64_t r, int64_t g, int64_t b, int64_t a) {
    if (!g_renderer) return;
    SDL_SetRenderDrawBlendMode(g_renderer, h2d_blend_mode());
    SDL_SetRenderDrawColor(g_renderer, (Uint8)r, (Uint8)g, (Uint8)b, (Uint8)a);
    SDL_FRect rc = { (float)x, (float)y, (float)w, (float)h };
    SDL_RenderFillRectF(g_renderer, &rc);
}

void h2d_draw_rectangle_lines(double x, double y, double w, double h, double thick, int64_t r, int64_t g, int64_t b, int64_t a) {
    if (!g_renderer) return;
    int t = (int)(thick < 1.0 ? 1.0 : thick);
    for (int i = 0; i < t; i++) {
        rectangleRGBA(g_renderer, (Sint16)(x - i), (Sint16)(y - i),
                      (Sint16)(x + w + i), (Sint16)(y + h + i),
                      (Uint8)r, (Uint8)g, (Uint8)b, (Uint8)a);
    }
}

void h2d_draw_circle(double cx, double cy, double radius, int64_t r, int64_t g, int64_t b, int64_t a) {
    if (!g_renderer) return;
    filledCircleRGBA(g_renderer, (Sint16)cx, (Sint16)cy, (Sint16)radius, (Uint8)r, (Uint8)g, (Uint8)b, (Uint8)a);
}

void h2d_draw_circle_lines(double cx, double cy, double radius, int64_t r, int64_t g, int64_t b, int64_t a) {
    if (!g_renderer) return;
    aacircleRGBA(g_renderer, (Sint16)cx, (Sint16)cy, (Sint16)radius, (Uint8)r, (Uint8)g, (Uint8)b, (Uint8)a);
}

void h2d_draw_line(double x1, double y1, double x2, double y2, double thick, int64_t r, int64_t g, int64_t b, int64_t a) {
    if (!g_renderer) return;
    Uint8 t = (Uint8)(thick < 1.0 ? 1.0 : thick);
    thickLineRGBA(g_renderer, (Sint16)x1, (Sint16)y1, (Sint16)x2, (Sint16)y2,
                  t, (Uint8)r, (Uint8)g, (Uint8)b, (Uint8)a);
}

void h2d_draw_triangle(double x1, double y1, double x2, double y2, double x3, double y3, int64_t r, int64_t g, int64_t b, int64_t a) {
    if (!g_renderer) return;
    filledTrigonRGBA(g_renderer, (Sint16)x1, (Sint16)y1, (Sint16)x2, (Sint16)y2, (Sint16)x3, (Sint16)y3,
                      (Uint8)r, (Uint8)g, (Uint8)b, (Uint8)a);
}

/* Bitmap 8x8 font baked into SDL2_gfx — no font file needed. `size` is
 * an integer scale factor (8 = native 8px glyphs, 16 = 2x, ...). */
void h2d_draw_text(const char *text, double x, double y, int64_t size, int64_t r, int64_t g, int64_t b, int64_t a) {
    if (!g_renderer || !text) return;
    float scale = (float)size / 8.0f;
    if (scale < 1.0f) scale = 1.0f;
    if (scale == 1.0f) {
        stringRGBA(g_renderer, (Sint16)x, (Sint16)y, text, (Uint8)r, (Uint8)g, (Uint8)b, (Uint8)a);
        return;
    }
    float sx = 1.0f, sy = 1.0f;
    SDL_RenderGetScale(g_renderer, &sx, &sy);
    SDL_RenderSetScale(g_renderer, sx * scale, sy * scale);
    stringRGBA(g_renderer, (Sint16)(x / scale), (Sint16)(y / scale), text, (Uint8)r, (Uint8)g, (Uint8)b, (Uint8)a);
    SDL_RenderSetScale(g_renderer, sx, sy);
}

/* Rough on-screen width in pixels for `text` at `size` — lets H# code
 * center/right-align text without needing real font metrics. */
int64_t h2d_measure_text(const char *text, int64_t size) {
    if (!text) return 0;
    double scale = (double)size / 8.0;
    if (scale < 1.0) scale = 1.0;
    return (int64_t)((double)strlen(text) * 8.0 * scale);
}

/* ── textures ───────────────────────────────────────────────────────── */

static int h2d_texture_alloc_slot(void) {
    for (int tries = 0; tries < H2D_MAX_TEXTURES; tries++) {
        int slot = g_texture_next;
        g_texture_next++;
        if (g_texture_next >= H2D_MAX_TEXTURES) g_texture_next = 1;
        if (g_textures[slot] == NULL) return slot;
    }
    return 0;
}

/* Loads any format SDL_image supports (PNG, JPG, BMP, GIF, ...) — a
 * real upgrade over H#'s std -> image, which only decodes uncompressed
 * 24-bit BMP. Returns 0 on failure. */
int64_t h2d_load_texture(const char *path) {
    if (!g_renderer || !path) return 0;
    SDL_Surface *surf = IMG_Load(path);
    if (!surf) return 0;
    SDL_Texture *tex = SDL_CreateTextureFromSurface(g_renderer, surf);
    int w = surf->w, h = surf->h;
    SDL_FreeSurface(surf);
    if (!tex) return 0;
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);

    int slot = h2d_texture_alloc_slot();
    if (slot == 0) { SDL_DestroyTexture(tex); return 0; }
    g_textures[slot] = tex;
    g_texture_w[slot] = w;
    g_texture_h[slot] = h;
    return slot;
}

/* Builds a texture directly from a raw RGBA8888 buffer (top-down, 4
 * bytes/pixel) — the escape hatch for procedurally generated pixel
 * data, or for feeding in an H# `std -> image` `Image.pixels` buffer
 * after expanding it from RGB to RGBA yourself. `pixels` must contain
 * exactly width*height*4 bytes; `pixels` is H#'s `bytes` type, which
 * the compiler maps to `uint8_t*` (see the ABI note at the top of this
 * file), not `int64_t`, so it is NOT one of the width/height params. */
/* H# `bytes` is NOT a raw buffer: at runtime it is an `HshArray*`
 * (len, cap, int64_t *data — one int64 per byte, values 0..255), see
 * H-Sharp runtime core.c. The header-declared `uint8_t*` is only the C
 * spelling of the pointer. We therefore flatten it into a real byte buffer
 * here (0.2.0 read the struct header as pixel data -> garbage textures). */
typedef struct { int64_t len; int64_t cap; int64_t *data; int64_t own; } H2dHshArray;

int64_t h2d_load_texture_from_rgba(uint8_t *pixels, int64_t width, int64_t height) {
    if (!g_renderer || !pixels || width <= 0 || height <= 0) return 0;
    const H2dHshArray *arr = (const H2dHshArray *)(void *)pixels;
    int w = (int)width, h = (int)height;
    int64_t need = (int64_t)w * h * 4;
    if (!arr->data || arr->len < need) return 0;      /* too short: refuse, never read out of bounds */
    uint8_t *flat = (uint8_t *)malloc((size_t)need);
    if (!flat) return 0;
    for (int64_t i = 0; i < need; i++) flat[i] = (uint8_t)(arr->data[i] & 0xff);
    SDL_Surface *surf = SDL_CreateRGBSurfaceWithFormatFrom(
        (void *)flat, w, h, 32, w * 4, SDL_PIXELFORMAT_RGBA32 /* byte order R,G,B,A on every CPU */
    );
    if (!surf) { free(flat); return 0; }
    SDL_Texture *tex = SDL_CreateTextureFromSurface(g_renderer, surf);
    SDL_FreeSurface(surf);
    free(flat);
    if (!tex) return 0;
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);

    int slot = h2d_texture_alloc_slot();
    if (slot == 0) { SDL_DestroyTexture(tex); return 0; }
    g_textures[slot] = tex;
    g_texture_w[slot] = w;
    g_texture_h[slot] = h;
    return slot;
}

void h2d_unload_texture(int64_t handle) {
    int h = (int)handle;
    if (h <= 0 || h >= H2D_MAX_TEXTURES || !g_textures[h]) return;
    SDL_DestroyTexture(g_textures[h]);
    g_textures[h] = NULL;
}

int64_t h2d_texture_width(int64_t handle) {
    int h = (int)handle;
    if (h <= 0 || h >= H2D_MAX_TEXTURES) return 0;
    return g_texture_w[h];
}

int64_t h2d_texture_height(int64_t handle) {
    int h = (int)handle;
    if (h <= 0 || h >= H2D_MAX_TEXTURES) return 0;
    return g_texture_h[h];
}

void h2d_draw_texture(int64_t handle, double x, double y, int64_t r, int64_t g, int64_t b, int64_t a) {
    int h = (int)handle;
    if (!g_renderer || h <= 0 || h >= H2D_MAX_TEXTURES || !g_textures[h]) return;
    SDL_Texture *tex = g_textures[h];
    SDL_SetTextureColorMod(tex, (Uint8)r, (Uint8)g, (Uint8)b);
    SDL_SetTextureAlphaMod(tex, (Uint8)a);
    SDL_SetTextureBlendMode(tex, h2d_blend_mode());
    SDL_FRect dst = { (float)x, (float)y, (float)g_texture_w[h], (float)g_texture_h[h] };
    SDL_RenderCopyF(g_renderer, tex, NULL, &dst);
}

/* Full transform draw: destination size (dst_w/dst_h — pass the
 * texture's own width/height for 1:1 scale), rotation in degrees
 * around (origin_x, origin_y) measured from the destination
 * top-left, optional horizontal/vertical flip (0/1), and an RGBA
 * tint. This is the GPU path (SDL_RenderCopyEx) — rotation/scale cost
 * nothing extra over a plain blit on any hardware-accelerated
 * renderer. */
void h2d_draw_texture_ex(int64_t handle, double x, double y, double dst_w, double dst_h,
                          double rotation_deg, double origin_x, double origin_y,
                          int64_t flip_h, int64_t flip_v, int64_t r, int64_t g, int64_t b, int64_t a) {
    int h = (int)handle;
    if (!g_renderer || h <= 0 || h >= H2D_MAX_TEXTURES || !g_textures[h]) return;
    SDL_Texture *tex = g_textures[h];
    SDL_SetTextureColorMod(tex, (Uint8)r, (Uint8)g, (Uint8)b);
    SDL_SetTextureAlphaMod(tex, (Uint8)a);
    SDL_SetTextureBlendMode(tex, h2d_blend_mode());
    SDL_FRect dst = { (float)x, (float)y, (float)dst_w, (float)dst_h };
    SDL_FPoint center = { (float)origin_x, (float)origin_y };
    SDL_RendererFlip flip = SDL_FLIP_NONE;
    if (flip_h) flip |= SDL_FLIP_HORIZONTAL;
    if (flip_v) flip |= SDL_FLIP_VERTICAL;
    SDL_RenderCopyExF(g_renderer, tex, NULL, &dst, rotation_deg, &center, flip);
}

/* Draws one sub-rectangle (src_x, src_y, src_w, src_h) of the texture,
 * stretched to (dst_w, dst_h) at (x, y) — the sprite-sheet / texture
 * atlas primitive that h2d_anim2d builds frame-by-frame animation on
 * top of. */
void h2d_draw_texture_rec(int64_t handle, double src_x, double src_y, double src_w, double src_h,
                           double x, double y, double dst_w, double dst_h,
                           int64_t r, int64_t g, int64_t b, int64_t a) {
    int h = (int)handle;
    if (!g_renderer || h <= 0 || h >= H2D_MAX_TEXTURES || !g_textures[h]) return;
    SDL_Texture *tex = g_textures[h];
    SDL_SetTextureColorMod(tex, (Uint8)r, (Uint8)g, (Uint8)b);
    SDL_SetTextureAlphaMod(tex, (Uint8)a);
    SDL_SetTextureBlendMode(tex, h2d_blend_mode());
    SDL_Rect src = { (int)src_x, (int)src_y, (int)src_w, (int)src_h };
    SDL_FRect dst = { (float)x, (float)y, (float)dst_w, (float)dst_h };
    SDL_RenderCopyF(g_renderer, tex, &src, &dst);
}

/* ── input ──────────────────────────────────────────────────────────── */
/* All "is it true" queries return an H#-`int` 0/1 rather than a `bool`,
 * to keep the ABI to exactly two integer widths (see the note up top). */

int64_t h2d_is_key_down(int64_t key) {
    int k = (int)key;
    if (k < 0 || k >= H2D_KEY_COUNT) return 0;
    return g_keys_now[k];
}

int64_t h2d_is_key_pressed(int64_t key) {
    int k = (int)key;
    if (k < 0 || k >= H2D_KEY_COUNT) return 0;
    return ((g_keys_now[k] && !g_keys_prev[k]) || g_key_down_edge[k]) ? 1 : 0;
}

int64_t h2d_is_key_released(int64_t key) {
    int k = (int)key;
    if (k < 0 || k >= H2D_KEY_COUNT) return 0;
    return ((!g_keys_now[k] && g_keys_prev[k]) || g_key_up_edge[k]) ? 1 : 0;
}

double h2d_mouse_x(void) { return (double)g_mouse_x; }
double h2d_mouse_y(void) { return (double)g_mouse_y; }
double h2d_mouse_wheel(void) { return g_mouse_wheel; }

/* button: 0=left 1=middle 2=right */
int64_t h2d_is_mouse_button_down(int64_t button) {
    int mask = (button == 0) ? 1 : (button == 1) ? 2 : (button == 2) ? 4 : 0;
    return (g_mouse_buttons_now & mask) ? 1 : 0;
}

int64_t h2d_is_mouse_button_pressed(int64_t button) {
    int mask = (button == 0) ? 1 : (button == 1) ? 2 : (button == 2) ? 4 : 0;
    return (((g_mouse_buttons_now & mask) && !(g_mouse_buttons_prev & mask)) || (g_mouse_down_edge & mask)) ? 1 : 0;
}

/* ── audio ──────────────────────────────────────────────────────────── */

int64_t h2d_audio_init(void) {
    if (g_audio_ready) return 1;
    if (SDL_WasInit(SDL_INIT_AUDIO) == 0) {
        if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) return 0;
    }
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) != 0) return 0;
    Mix_AllocateChannels(32);
    g_audio_ready = 1;
    return 1;
}

int64_t h2d_load_sound(const char *path) {
    if (!g_audio_ready || !path) return 0;
    Mix_Chunk *chunk = Mix_LoadWAV(path);
    if (!chunk) return 0;
    int slot = 0;
    for (int i = 1; i < H2D_MAX_SOUNDS; i++) { if (!g_sounds[i]) { slot = i; break; } }
    if (slot == 0) { Mix_FreeChunk(chunk); return 0; } /* table full: never overwrite a live sound */
    g_sounds[slot] = chunk;
    return slot;
}

void h2d_play_sound(int64_t handle) {
    int h = (int)handle;
    if (h <= 0 || h >= H2D_MAX_SOUNDS || !g_sounds[h]) return;
    Mix_PlayChannel(-1, g_sounds[h], 0);
}

void h2d_play_sound_loop(int64_t handle) {
    int h = (int)handle;
    if (h <= 0 || h >= H2D_MAX_SOUNDS || !g_sounds[h]) return;
    Mix_PlayChannel(-1, g_sounds[h], -1);
}

void h2d_set_sound_volume(int64_t handle, double volume) {
    int h = (int)handle;
    if (h <= 0 || h >= H2D_MAX_SOUNDS || !g_sounds[h]) return;
    int v = (int)(volume * MIX_MAX_VOLUME);
    if (v < 0) v = 0;
    if (v > MIX_MAX_VOLUME) v = MIX_MAX_VOLUME;
    Mix_VolumeChunk(g_sounds[h], v);
}

void h2d_stop_all_sounds(void) {
    if (!g_audio_ready) return;
    Mix_HaltChannel(-1);
}

int64_t h2d_load_music(const char *path) {
    if (!g_audio_ready || !path) return 0;
    Mix_Music *music = Mix_LoadMUS(path);
    if (!music) return 0;
    int slot = 0;
    for (int i = 1; i < H2D_MAX_MUSIC; i++) { if (!g_music[i]) { slot = i; break; } }
    if (slot == 0) { Mix_FreeMusic(music); return 0; }
    g_music[slot] = music;
    return slot;
}

void h2d_play_music(int64_t handle, int64_t loop) {
    int h = (int)handle;
    if (h <= 0 || h >= H2D_MAX_MUSIC || !g_music[h]) return;
    Mix_PlayMusic(g_music[h], loop ? -1 : 1);
}

void h2d_stop_music(void) {
    if (!g_audio_ready) return;
    Mix_HaltMusic();
}

void h2d_set_music_volume(double volume) {
    if (!g_audio_ready) return;
    int v = (int)(volume * MIX_MAX_VOLUME);
    if (v < 0) v = 0;
    if (v > MIX_MAX_VOLUME) v = MIX_MAX_VOLUME;
    Mix_VolumeMusic(v);
}

/* ════════════════════════════════════════════════════════════════════
 *  0.3.0 additions — everything a finished 2D game needs on top of the
 *  0.2.0 core: window control, virtual resolution, more input
 *  (text, gamepad, mouse delta), TrueType fonts, more primitives,
 *  render targets, richer audio, screenshots.
 *  ABI rule unchanged: H# int = int64_t, f64 = double, no bool/struct.
 * ════════════════════════════════════════════════════════════════════ */

/* ── window ─────────────────────────────────────────────────────────── */

void h2d_set_window_size(int64_t w, int64_t h) {
    if (g_window && w > 0 && h > 0) SDL_SetWindowSize(g_window, (int)w, (int)h);
}

void h2d_set_window_min_size(int64_t w, int64_t h) {
    if (g_window) SDL_SetWindowMinimumSize(g_window, (int)w, (int)h);
}

void h2d_set_fullscreen(int64_t on) {
    if (g_window) SDL_SetWindowFullscreen(g_window, on ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
}

int64_t h2d_is_fullscreen(void) {
    return (g_window && (SDL_GetWindowFlags(g_window) & SDL_WINDOW_FULLSCREEN_DESKTOP)) ? 1 : 0;
}

/* Virtual resolution: the game draws in w x h "logical" pixels and SDL
 * scales + letterboxes it to the real window. w <= 0 turns it off. */
void h2d_set_logical_size(int64_t w, int64_t h) {
    if (!g_renderer) return;
    if (w <= 0 || h <= 0) { SDL_RenderSetLogicalSize(g_renderer, 0, 0); g_logical_w = g_logical_h = 0; return; }
    SDL_RenderSetLogicalSize(g_renderer, (int)w, (int)h);
    g_logical_w = (int)w; g_logical_h = (int)h;
}

void h2d_set_integer_scale(int64_t on) {
    if (g_renderer) SDL_RenderSetIntegerScale(g_renderer, on ? SDL_TRUE : SDL_FALSE);
}

void h2d_show_cursor(int64_t on) { SDL_ShowCursor(on ? SDL_ENABLE : SDL_DISABLE); }

void h2d_set_relative_mouse(int64_t on) { SDL_SetRelativeMouseMode(on ? SDL_TRUE : SDL_FALSE); }

void h2d_set_window_icon(const char *path) {
    if (!g_window || !path) return;
    SDL_Surface *s = IMG_Load(path);
    if (!s) return;
    SDL_SetWindowIcon(g_window, s);
    SDL_FreeSurface(s);
}

void h2d_request_close(void) { g_should_close = 1; }
double h2d_get_fps(void) { return g_fps_smooth; }
int64_t h2d_get_frame_count(void) { return (int64_t)g_frame_count; }
int64_t h2d_get_ticks_ms(void) { return (int64_t)SDL_GetTicks64(); }

/* ── input ──────────────────────────────────────────────────────────── */

double h2d_mouse_dx(void) { return (double)g_mouse_dx; }
double h2d_mouse_dy(void) { return (double)g_mouse_dy; }

int64_t h2d_is_mouse_button_released(int64_t button) {
    int mask = (button == 0) ? 1 : (button == 1) ? 2 : (button == 2) ? 4 : 0;
    return ((!(g_mouse_buttons_now & mask) && (g_mouse_buttons_prev & mask)) || (g_mouse_up_edge & mask)) ? 1 : 0;
}

/* Key code (h2d_keys) of the first key pressed this frame, or -1. */
int64_t h2d_get_key_pressed(void) { return g_first_key_pressed; }

void h2d_start_text_input(void) { SDL_StartTextInput(); }
void h2d_stop_text_input(void)  { SDL_StopTextInput(); }

/* Pops one typed Unicode code point (UTF-32) from the text queue; 0 = empty. */
int64_t h2d_get_char_pressed(void) {
    if (g_char_head == g_char_tail) return 0;
    int c = g_char_queue[g_char_head];
    g_char_head = (g_char_head + 1) % H2D_CHAR_QUEUE;
    return c;
}

int64_t h2d_gamepad_count(void) {
    int n = 0;
    for (int i = 0; i < H2D_MAX_PADS; i++) if (g_pads[i]) n++;
    return n;
}

int64_t h2d_gamepad_connected(int64_t pad) {
    return (pad >= 0 && pad < H2D_MAX_PADS && g_pads[pad]) ? 1 : 0;
}

/* button: SDL_CONTROLLER_BUTTON_* order: 0=A 1=B 2=X 3=Y 4=Back 5=Guide 6=Start
 * 7=L3 8=R3 9=LB 10=RB 11=Up 12=Down 13=Left 14=Right */
int64_t h2d_gamepad_button_down(int64_t pad, int64_t button) {
    if (pad < 0 || pad >= H2D_MAX_PADS || button < 0 || button > 14) return 0;
    return (g_pad_btn_now[pad] >> button) & 1u;
}

int64_t h2d_gamepad_button_pressed(int64_t pad, int64_t button) {
    if (pad < 0 || pad >= H2D_MAX_PADS || button < 0 || button > 14) return 0;
    return (((g_pad_btn_now[pad] & ~g_pad_btn_prev[pad]) >> button) & 1u);
}

/* axis: 0=LX 1=LY 2=RX 3=RY 4=LT 5=RT; returns -1.0 .. 1.0 (triggers 0 .. 1) */
double h2d_gamepad_axis(int64_t pad, int64_t axis) {
    if (pad < 0 || pad >= H2D_MAX_PADS || !g_pads[pad] || axis < 0 || axis > 5) return 0.0;
    return (double)SDL_GameControllerGetAxis(g_pads[pad], (SDL_GameControllerAxis)axis) / 32767.0;
}

/* ── more drawing ───────────────────────────────────────────────────── */

void h2d_set_blend_mode(int64_t mode) { g_blend = (int)mode; }

void h2d_set_clip(double x, double y, double w, double h) {
    if (!g_renderer) return;
    SDL_Rect r = { (int)x, (int)y, (int)w, (int)h };
    SDL_RenderSetClipRect(g_renderer, &r);
}

void h2d_clear_clip(void) { if (g_renderer) SDL_RenderSetClipRect(g_renderer, NULL); }

void h2d_draw_pixel(double x, double y, int64_t r, int64_t g, int64_t b, int64_t a) {
    if (!g_renderer) return;
    pixelRGBA(g_renderer, (Sint16)x, (Sint16)y, (Uint8)r, (Uint8)g, (Uint8)b, (Uint8)a);
}

void h2d_draw_ellipse(double cx, double cy, double rx, double ry, int64_t r, int64_t g, int64_t b, int64_t a) {
    if (!g_renderer) return;
    filledEllipseRGBA(g_renderer, (Sint16)cx, (Sint16)cy, (Sint16)rx, (Sint16)ry, (Uint8)r, (Uint8)g, (Uint8)b, (Uint8)a);
}

void h2d_draw_ellipse_lines(double cx, double cy, double rx, double ry, int64_t r, int64_t g, int64_t b, int64_t a) {
    if (!g_renderer) return;
    aaellipseRGBA(g_renderer, (Sint16)cx, (Sint16)cy, (Sint16)rx, (Sint16)ry, (Uint8)r, (Uint8)g, (Uint8)b, (Uint8)a);
}

void h2d_draw_rounded_rect(double x, double y, double w, double h, double radius, int64_t r, int64_t g, int64_t b, int64_t a) {
    if (!g_renderer) return;
    roundedBoxRGBA(g_renderer, (Sint16)x, (Sint16)y, (Sint16)(x + w), (Sint16)(y + h), (Sint16)radius,
                   (Uint8)r, (Uint8)g, (Uint8)b, (Uint8)a);
}

void h2d_draw_rounded_rect_lines(double x, double y, double w, double h, double radius, int64_t r, int64_t g, int64_t b, int64_t a) {
    if (!g_renderer) return;
    roundedRectangleRGBA(g_renderer, (Sint16)x, (Sint16)y, (Sint16)(x + w), (Sint16)(y + h), (Sint16)radius,
                         (Uint8)r, (Uint8)g, (Uint8)b, (Uint8)a);
}

void h2d_draw_triangle_lines(double x1, double y1, double x2, double y2, double x3, double y3, int64_t r, int64_t g, int64_t b, int64_t a) {
    if (!g_renderer) return;
    aatrigonRGBA(g_renderer, (Sint16)x1, (Sint16)y1, (Sint16)x2, (Sint16)y2, (Sint16)x3, (Sint16)y3,
                 (Uint8)r, (Uint8)g, (Uint8)b, (Uint8)a);
}

/* Full-featured sprite draw: source rectangle + destination + rotation +
 * origin + flips + tint in ONE call — what a flipped sprite-sheet frame
 * (platformer character facing left) needs. */
void h2d_draw_texture_full(int64_t handle, double src_x, double src_y, double src_w, double src_h,
                           double x, double y, double dst_w, double dst_h,
                           double rotation_deg, double origin_x, double origin_y,
                           int64_t flip_h, int64_t flip_v, int64_t r, int64_t g, int64_t b, int64_t a) {
    int h = (int)handle;
    if (!g_renderer || h <= 0 || h >= H2D_MAX_TEXTURES || !g_textures[h]) return;
    SDL_Texture *tex = g_textures[h];
    SDL_SetTextureColorMod(tex, (Uint8)r, (Uint8)g, (Uint8)b);
    SDL_SetTextureAlphaMod(tex, (Uint8)a);
    SDL_SetTextureBlendMode(tex, h2d_blend_mode());
    SDL_Rect src = { (int)src_x, (int)src_y, (int)src_w, (int)src_h };
    SDL_FRect dst = { (float)x, (float)y, (float)dst_w, (float)dst_h };
    SDL_FPoint center = { (float)origin_x, (float)origin_y };
    int flip = SDL_FLIP_NONE;
    if (flip_h) flip |= SDL_FLIP_HORIZONTAL;
    if (flip_v) flip |= SDL_FLIP_VERTICAL;
    SDL_RenderCopyExF(g_renderer, tex, &src, &dst, rotation_deg, &center, (SDL_RendererFlip)flip);
}

/* 0 = nearest (crisp pixel art, default), 1 = linear (smooth). Affects
 * textures created AFTER the call. */
void h2d_set_texture_filter(int64_t linear) {
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, linear ? "1" : "0");
}

/* ── render targets (off-screen textures: minimaps, lighting, post-fx) ── */

int64_t h2d_create_render_texture(int64_t width, int64_t height) {
    if (!g_renderer || width <= 0 || height <= 0) return 0;
    SDL_Texture *tex = SDL_CreateTexture(g_renderer, SDL_PIXELFORMAT_RGBA8888,
                                         SDL_TEXTUREACCESS_TARGET, (int)width, (int)height);
    if (!tex) return 0;
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    int slot = h2d_texture_alloc_slot();
    if (slot == 0) { SDL_DestroyTexture(tex); return 0; }
    g_textures[slot] = tex;
    g_texture_w[slot] = (int)width;
    g_texture_h[slot] = (int)height;
    return slot;
}

/* Everything drawn between begin/end goes into the texture. */
void h2d_begin_texture_mode(int64_t handle) {
    int h = (int)handle;
    if (!g_renderer || h <= 0 || h >= H2D_MAX_TEXTURES || !g_textures[h]) return;
    SDL_SetRenderTarget(g_renderer, g_textures[h]);
}

void h2d_end_texture_mode(void) {
    if (g_renderer) SDL_SetRenderTarget(g_renderer, NULL);
}

/* ── TrueType fonts (UTF-8, so Polish diacritics work) ──────────────── */

int64_t h2d_load_font(const char *path, int64_t pt_size) {
    if (!g_window || !path || pt_size <= 0) return 0;
    int slot = 0;
    for (int i = 1; i < H2D_MAX_FONTS; i++) { if (!g_fonts[i]) { slot = i; break; } }
    if (slot == 0) return 0;
    TTF_Font *f = TTF_OpenFont(path, (int)pt_size);
    if (!f) return 0;
    g_fonts[slot] = f;
    return slot;
}

void h2d_unload_font(int64_t handle) {
    int h = (int)handle;
    if (h <= 0 || h >= H2D_MAX_FONTS || !g_fonts[h]) return;
    for (int i = 0; i < H2D_TEXT_CACHE; i++) {
        if (g_text_cache[i].tex && g_text_cache[i].font == h) {
            SDL_DestroyTexture(g_text_cache[i].tex);
            free(g_text_cache[i].text);
            memset(&g_text_cache[i], 0, sizeof(g_text_cache[i]));
        }
    }
    TTF_CloseFont(g_fonts[h]);
    g_fonts[h] = NULL;
}

/* Rendered strings are cached (font + text), tinted per draw with a colour
 * mod, so a HUD redrawn every frame costs one blit, not a re-render. */
static TextCacheEntry *h2d_text_lookup(int font, const char *text) {
    int oldest = 0;
    for (int i = 0; i < H2D_TEXT_CACHE; i++) {
        TextCacheEntry *e = &g_text_cache[i];
        if (e->tex && e->font == font && strcmp(e->text, text) == 0) { e->used = g_frame_count + 1; return e; }
        if (!e->tex) { oldest = i; break; }
        if (e->used < g_text_cache[oldest].used) oldest = i;
    }
    TextCacheEntry *slot = &g_text_cache[oldest];
    if (slot->tex) { SDL_DestroyTexture(slot->tex); free(slot->text); memset(slot, 0, sizeof(*slot)); }
    SDL_Color white = { 255, 255, 255, 255 };
    SDL_Surface *surf = TTF_RenderUTF8_Blended(g_fonts[font], text, white);
    if (!surf) return NULL;
    slot->tex = SDL_CreateTextureFromSurface(g_renderer, surf);
    slot->w = surf->w; slot->h = surf->h;
    SDL_FreeSurface(surf);
    if (!slot->tex) { memset(slot, 0, sizeof(*slot)); return NULL; }
    SDL_SetTextureBlendMode(slot->tex, SDL_BLENDMODE_BLEND);
    slot->text = strdup(text);
    slot->font = font;
    slot->used = g_frame_count + 1;
    return slot;
}

void h2d_draw_text_font(int64_t font, const char *text, double x, double y, int64_t r, int64_t g, int64_t b, int64_t a) {
    int f = (int)font;
    if (!g_renderer || !text || !*text || f <= 0 || f >= H2D_MAX_FONTS || !g_fonts[f]) return;
    TextCacheEntry *e = h2d_text_lookup(f, text);
    if (!e) return;
    SDL_SetTextureColorMod(e->tex, (Uint8)r, (Uint8)g, (Uint8)b);
    SDL_SetTextureAlphaMod(e->tex, (Uint8)a);
    SDL_FRect dst = { (float)x, (float)y, (float)e->w, (float)e->h };
    SDL_RenderCopyF(g_renderer, e->tex, NULL, &dst);
}

int64_t h2d_measure_text_font(int64_t font, const char *text) {
    int f = (int)font, w = 0, h = 0;
    if (!text || f <= 0 || f >= H2D_MAX_FONTS || !g_fonts[f]) return 0;
    TTF_SizeUTF8(g_fonts[f], text, &w, &h);
    return w;
}

int64_t h2d_font_height(int64_t font) {
    int f = (int)font;
    if (f <= 0 || f >= H2D_MAX_FONTS || !g_fonts[f]) return 0;
    return TTF_FontHeight(g_fonts[f]);
}

/* ── audio ──────────────────────────────────────────────────────────── */

void h2d_unload_sound(int64_t handle) {
    int h = (int)handle;
    if (h <= 0 || h >= H2D_MAX_SOUNDS || !g_sounds[h]) return;
    Mix_FreeChunk(g_sounds[h]);
    g_sounds[h] = NULL;
}

void h2d_unload_music(int64_t handle) {
    int h = (int)handle;
    if (h <= 0 || h >= H2D_MAX_MUSIC || !g_music[h]) return;
    Mix_FreeMusic(g_music[h]);
    g_music[h] = NULL;
}

/* volume 0..1, pan -1 (left) .. 1 (right), loop 0/1. Returns the mixer
 * channel (>= 0, usable with h2d_stop_channel) or -1. */
int64_t h2d_play_sound_ex(int64_t handle, double volume, double pan, int64_t loop) {
    int h = (int)handle;
    if (h <= 0 || h >= H2D_MAX_SOUNDS || !g_sounds[h]) return -1;
    int ch = Mix_PlayChannel(-1, g_sounds[h], loop ? -1 : 0);
    if (ch < 0) return -1;
    int v = (int)(volume * MIX_MAX_VOLUME);
    if (v < 0) v = 0;
    if (v > MIX_MAX_VOLUME) v = MIX_MAX_VOLUME;
    Mix_Volume(ch, v);
    if (pan < -1.0) pan = -1.0;
    if (pan > 1.0) pan = 1.0;
    int right = (int)((pan + 1.0) * 127.5);
    Mix_SetPanning(ch, (Uint8)(255 - right), (Uint8)right);
    return ch;
}

void h2d_stop_channel(int64_t ch) { if (g_audio_ready && ch >= 0) Mix_HaltChannel((int)ch); }
void h2d_pause_music(void)  { if (g_audio_ready) Mix_PauseMusic(); }
void h2d_resume_music(void) { if (g_audio_ready) Mix_ResumeMusic(); }
int64_t h2d_is_music_playing(void) { return (g_audio_ready && Mix_PlayingMusic()) ? 1 : 0; }

void h2d_fade_in_music(int64_t handle, int64_t loop, int64_t ms) {
    int h = (int)handle;
    if (h <= 0 || h >= H2D_MAX_MUSIC || !g_music[h]) return;
    Mix_FadeInMusic(g_music[h], loop ? -1 : 1, (int)ms);
}

void h2d_fade_out_music(int64_t ms) { if (g_audio_ready) Mix_FadeOutMusic((int)ms); }

/* master volume for all sound effects, 0..1 */
void h2d_set_master_volume(double volume) {
    if (!g_audio_ready) return;
    int v = (int)(volume * MIX_MAX_VOLUME);
    if (v < 0) v = 0;
    if (v > MIX_MAX_VOLUME) v = MIX_MAX_VOLUME;
    Mix_MasterVolume(v);
}

/* ── screenshots ────────────────────────────────────────────────────── */

static int h2d_save_png(const char *path) {
    if (!g_renderer || !path) return 0;
    int w = 0, h = 0;
    if (SDL_GetRendererOutputSize(g_renderer, &w, &h) != 0 || w <= 0 || h <= 0) return 0;
    SDL_Surface *surf = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ARGB8888);
    if (!surf) return 0;
    int ok = 0;
    if (SDL_RenderReadPixels(g_renderer, NULL, SDL_PIXELFORMAT_ARGB8888, surf->pixels, surf->pitch) == 0)
        ok = (IMG_SavePNG(surf, path) == 0);
    SDL_FreeSurface(surf);
    return ok;
}

/* Saves the CURRENT frame (call before next_frame). 1 on success. */
int64_t h2d_save_screenshot(const char *path) { return h2d_save_png(path); }
