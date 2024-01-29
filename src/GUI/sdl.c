#define _XOPEN_SOURCE 600

#include <stdlib.h>
#include <string.h>
#include <time.h>

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

static int quit = 0;

static struct window_finger fingers[8];

void window_init(int * argc, char ** argv) {
    static struct window_instance i;
    hud_window = &i;

#ifdef USE_TOUCH
    SDL_SetHintWithPriority(SDL_HINT_ANDROID_SEPARATE_MOUSE_AND_TOUCH, "1", SDL_HINT_OVERRIDE);
#endif

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_TIMER);

    hud_window->impl
        = SDL_CreateWindow("TigerSpades " BETTERSPADES_VERSION, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
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

static inline __attribute((always_inline))
int get_sdl_button(int button) {
    switch (button) {
        case SDL_BUTTON_LEFT:   return WINDOW_MOUSE_LMB;
        case SDL_BUTTON_RIGHT:  return WINDOW_MOUSE_RMB;
        case SDL_BUTTON_MIDDLE: return WINDOW_MOUSE_MMB;
        default:                return -1;
    }
}

int window_get_mousemode() {
    int s = SDL_GetRelativeMouseMode();
    return s ? WINDOW_CURSOR_DISABLED : WINDOW_CURSOR_ENABLED;
}

void window_update() {
    SDL_GL_SwapWindow(hud_window->impl);
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT: quit = 1; break;

            case SDL_KEYDOWN: window_sendkey(WINDOW_PRESS, event.key.keysym.sym, event.key.keysym.mod & KMOD_CTRL); break;
            case SDL_KEYUP: window_sendkey(WINDOW_RELEASE, event.key.keysym.sym, event.key.keysym.mod & KMOD_CTRL); break;

            case SDL_MOUSEBUTTONDOWN: mouse_click(hud_window, get_sdl_button(event.button.button), WINDOW_PRESS, 0); break;
            case SDL_MOUSEBUTTONUP: mouse_click(hud_window, get_sdl_button(event.button.button), WINDOW_RELEASE, 0); break;

            case SDL_WINDOWEVENT: {
                if (event.window.event == SDL_WINDOWEVENT_RESIZED
                   || event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                    reshape(hud_window, event.window.data1, event.window.data2);
                }

                if (event.window.event == SDL_WINDOWEVENT_LEAVE)
                    mouse_hover(hud_window, false);

                if (event.window.event == SDL_WINDOWEVENT_ENTER)
                    mouse_hover(hud_window, true);

                if (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST)
                    mouse_focus(hud_window, false);

                if (event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED)
                    mouse_focus(hud_window, true);

                break;
            }

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

            case SDL_TEXTINPUT: text_input(hud_window, event.text.text); break;

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

void window_eventloop(Idle idle, Display display) {
    double last_frame_start = 0.0F;

    while (!quit) {
        double dt = window_time() - last_frame_start;
        last_frame_start = window_time();

        idle(dt);
        display();
        window_update();

        if (settings.vsync > 1 && (window_time() - last_frame_start) < (1.0 / settings.vsync)) {
            double sleep_s = 1.0 / settings.vsync - (window_time() - last_frame_start);
            struct timespec ts;
            ts.tv_sec = (int) sleep_s;
            ts.tv_nsec = (sleep_s - ts.tv_sec) * 1000000000.0;
            nanosleep(&ts, NULL);
        }

        fps = 1.0F / dt;
    }
}

#endif
