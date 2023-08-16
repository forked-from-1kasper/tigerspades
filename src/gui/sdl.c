#include <stdlib.h>
#include <string.h>

#include <BetterSpades/common.h>
#include <BetterSpades/main.h>
#include <BetterSpades/window.h>
#include <BetterSpades/config.h>
#include <BetterSpades/hud.h>

#ifdef OS_WINDOWS
#include <sysinfoapi.h>
#include <windows.h>
#endif

#ifdef OS_LINUX
#include <unistd.h>
#endif

#ifdef OS_HAIKU
#include <kernel/OS.h>
#endif

#ifdef USE_SDL

static struct window_finger fingers[8];

void window_init() {
    static struct window_instance i;
    hud_window = &i;

#ifdef USE_TOUCH
    SDL_SetHintWithPriority(SDL_HINT_ANDROID_SEPARATE_MOUSE_AND_TOUCH, "1", SDL_HINT_OVERRIDE);
#endif

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_TIMER);

    hud_window->impl
        = SDL_CreateWindow("BetterSpades " BETTERSPADES_VERSION, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                           settings.window_width, settings.window_height, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
#ifdef OPENGL_ES
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
#endif
    SDL_GL_CreateContext(hud_window->impl);

    memset(fingers, 0, sizeof(fingers));
}

void window_update() {
    SDL_GL_SwapWindow(hud_window->impl);
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT: quit = 1; break;
            case SDL_KEYDOWN: {
                int count = config_key_translate(event.key.keysym.sym, 0, NULL);

                if (count > 0) {
                    int results[count];
                    config_key_translate(event.key.keysym.sym, 0, results);

                    for (int k = 0; k < count; k++) {
                        keys(hud_window, results[k], event.key.keysym.sym, WINDOW_PRESS,
                             event.key.keysym.mod & KMOD_CTRL);

                        if (hud_active->input_keyboard)
                            hud_active->input_keyboard(results[k], WINDOW_PRESS, event.key.keysym.mod & KMOD_CTRL,
                                                       event.key.keysym.sym);
                    }
                } else {
                    if (hud_active->input_keyboard)
                        hud_active->input_keyboard(WINDOW_KEY_UNKNOWN, WINDOW_PRESS, event.key.keysym.mod & KMOD_CTRL,
                                                   event.key.keysym.sym);
                }
                break;
            }
            case SDL_KEYUP: {
                int count = config_key_translate(event.key.keysym.sym, 0, NULL);

                if (count > 0) {
                    int results[count];
                    config_key_translate(event.key.keysym.sym, 0, results);

                    for (int k = 0; k < count; k++) {
                        keys(hud_window, results[k], event.key.keysym.sym, WINDOW_RELEASE,
                             event.key.keysym.mod & KMOD_CTRL);

                        if (hud_active->input_keyboard)
                            hud_active->input_keyboard(results[k], WINDOW_RELEASE, event.key.keysym.mod & KMOD_CTRL,
                                                       event.key.keysym.sym);
                    }
                } else {
                    if (hud_active->input_keyboard)
                        hud_active->input_keyboard(WINDOW_KEY_UNKNOWN, WINDOW_RELEASE, event.key.keysym.mod & KMOD_CTRL,
                                                   event.key.keysym.sym);
                }
                break;
            }
            case SDL_MOUSEBUTTONDOWN: {
                int a = 0;
                switch (event.button.button) {
                    case SDL_BUTTON_LEFT: a = WINDOW_MOUSE_LMB; break;
                    case SDL_BUTTON_RIGHT: a = WINDOW_MOUSE_RMB; break;
                    case SDL_BUTTON_MIDDLE: a = WINDOW_MOUSE_MMB; break;
                }
                mouse_click(hud_window, a, WINDOW_PRESS, 0);
                break;
            }
            case SDL_MOUSEBUTTONUP: {
                int a = 0;
                switch (event.button.button) {
                    case SDL_BUTTON_LEFT: a = WINDOW_MOUSE_LMB; break;
                    case SDL_BUTTON_RIGHT: a = WINDOW_MOUSE_RMB; break;
                    case SDL_BUTTON_MIDDLE: a = WINDOW_MOUSE_MMB; break;
                }
                mouse_click(hud_window, a, WINDOW_RELEASE, 0);
                break;
            }
            case SDL_WINDOWEVENT:
                if (event.window.event == SDL_WINDOWEVENT_RESIZED
                   || event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                    reshape(hud_window, event.window.data1, event.window.data2);
                }
                break;
            case SDL_MOUSEWHEEL: mouse_scroll(hud_window, event.wheel.x, event.wheel.y); break;
            case SDL_MOUSEMOTION: {
                if (SDL_GetRelativeMouseMode()) {
                    static int x, y;
                    x += event.motion.xrel;
                    y += event.motion.yrel;
                    mouse(hud_window, x, y);
                } else {
                    mouse(hud_window, event.motion.x, event.motion.y);
                }
                break;
            }
            case SDL_TEXTINPUT: text_input(hud_window, event.text.text[0]); break;
            case SDL_FINGERDOWN:
                if (hud_active->input_touch) {
                    struct window_finger* f;
                    for (int k = 0; k < 8; k++) {
                        if (!fingers[k].full) {
                            fingers[k].finger = event.tfinger.fingerId;
                            fingers[k].start.x = event.tfinger.x * settings.window_width;
                            fingers[k].start.y = event.tfinger.y * settings.window_height;
                            fingers[k].down_time = window_time();
                            fingers[k].full = 1;
                            f = fingers + k;
                            break;
                        }
                    }

                    hud_active->input_touch(f, TOUCH_DOWN, event.tfinger.x * settings.window_width,
                                            event.tfinger.y * settings.window_height,
                                            event.tfinger.dx * settings.window_width,
                                            event.tfinger.dy * settings.window_height);
                }
                break;
            case SDL_FINGERUP:
                if (hud_active->input_touch) {
                    struct window_finger* f;
                    for (int k = 0; k < 8; k++) {
                        if (fingers[k].full && fingers[k].finger == event.tfinger.fingerId) {
                            fingers[k].full = 0;
                            f = fingers + k;
                            break;
                        }
                    }
                    hud_active->input_touch(
                        f, TOUCH_UP, event.tfinger.x * settings.window_width, event.tfinger.y * settings.window_height,
                        event.tfinger.dx * settings.window_width, event.tfinger.dy * settings.window_height);
                }
                break;
            case SDL_FINGERMOTION:
                if (hud_active->input_touch) {
                    struct window_finger* f;
                    for (int k = 0; k < 8; k++) {
                        if (fingers[k].full && fingers[k].finger == event.tfinger.fingerId) {
                            f = fingers + k;
                            break;
                        }
                    }
                    hud_active->input_touch(f, TOUCH_MOVE, event.tfinger.x * settings.window_width,
                                            event.tfinger.y * settings.window_height,
                                            event.tfinger.dx * settings.window_width,
                                            event.tfinger.dy * settings.window_height);
                }
                break;
        }
    }
}

#endif
