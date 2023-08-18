/*
    Copyright (c) 2017-2020 ByteBit

    This file is part of BetterSpades.

    BetterSpades is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    BetterSpades is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with BetterSpades.  If not, see <http://www.gnu.org/licenses/>.
*/

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

void window_keyname(int keycode, char* output, size_t length) {
    #if defined(OS_WINDOWS) && defined(USE_GLFW)
        GetKeyNameTextA(glfwGetKeyScancode(keycode) << 16, output, length);
    #else
        #ifdef USE_GLFW
            const char * keyname = glfwGetKeyName(keycode, 0);
        #endif

        #ifdef USE_SDL
            const char * keyname = SDL_GetKeyName(keycode);
        #endif

        if (keyname != NULL && *keyname != 0) {
            strncpy(output, keyname, length);
            output[length - 1] = 0;
        } else {
            snprintf(output, length, "<%d>", keycode);
        }
    #endif
}

void window_deinit() {
    #ifdef USE_GLFW
        glfwTerminate();
    #endif

    #ifdef USE_SDL
        SDL_DestroyWindow(hud_window->impl);
        SDL_Quit();
    #endif
}

float window_time() {
    #ifdef USE_GLFW
        return glfwGetTime();
    #endif

    #ifdef USE_SDL
        return ((double) SDL_GetTicks()) / 1000.0F;
    #endif
}

#ifdef USE_SDL
    int quit = 0;
#endif

int window_closed() {
    #ifdef USE_GLFW
        return glfwWindowShouldClose(hud_window->impl);
    #endif

    #ifdef USE_SDL
        return quit;
    #endif
}

void window_title(char * suffix) {
    char title[128];

    if (suffix)
        snprintf(title, sizeof(title) - 1, "BetterSpades %s — %s", BETTERSPADES_VERSION, suffix);
    else
        sprintf(title, "BetterSpades " BETTERSPADES_VERSION);

    #ifdef USE_GLFW
        glfwSetWindowTitle(hud_window->impl, title);
    #endif

    #ifdef USE_SDL
        SDL_SetWindowTitle(hud_window->impl, title);
    #endif
}

static double mx = -1, my = -1;

void window_setmouseloc(double x, double y) {
    #ifdef USE_SDL
        mx = x;
        my = y;
    #endif
}

void window_mouseloc(double * x, double * y) {
    #ifdef USE_GLFW
        glfwGetCursorPos(hud_window->impl, x, y);
    #endif

    #ifdef USE_SDL
        if (mx < 0 && my < 0) {
            int xi, yi;
            SDL_GetMouseState(&xi, &yi);
            *x = xi;
            *y = yi;
        } else {
            *x = mx;
            *y = my;
        }
    #endif
}

void window_textinput(int allow) {
    #ifdef USE_SDL
        if (allow && !SDL_IsTextInputActive())
            SDL_StartTextInput();
        if (!allow && SDL_IsTextInputActive())
            SDL_StopTextInput();
    #endif
}

const char * window_clipboard() {
    #ifdef USE_GLFW
        return glfwGetClipboardString(hud_window->impl);
    #endif

    #ifdef USE_SDL
        return SDL_HasClipboardText() ? SDL_GetClipboardText() : NULL;
    #endif
}

void window_swapping(int value) {
    #ifdef USE_GLFW
        glfwSwapInterval(value);
    #endif

    #ifdef USE_SDL
        SDL_GL_SetSwapInterval(value);
    #endif
}

void window_mousemode(int mode) {
    #ifdef USE_GLFW
        int s = glfwGetInputMode(hud_window->impl, GLFW_CURSOR);
        if ((s == GLFW_CURSOR_DISABLED && mode == WINDOW_CURSOR_ENABLED)
         || (s == GLFW_CURSOR_NORMAL && mode == WINDOW_CURSOR_DISABLED))
            glfwSetInputMode(hud_window->impl, GLFW_CURSOR,
                             mode == WINDOW_CURSOR_ENABLED ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
    #endif

    #ifdef USE_SDL
        int s = SDL_GetRelativeMouseMode();
        if ((s && mode == WINDOW_CURSOR_ENABLED) || (!s && mode == WINDOW_CURSOR_DISABLED))
            SDL_SetRelativeMouseMode(mode == WINDOW_CURSOR_ENABLED ? 0 : 1);
    #endif
}

void window_fromsettings() {
    #ifdef USE_GLFW
        glfwWindowHint(GLFW_SAMPLES, settings.multisamples);
        glfwSetWindowSize(hud_window->impl, settings.window_width, settings.window_height);

        if (settings.vsync < 2)
            window_swapping(settings.vsync);
        if (settings.vsync > 1)
            window_swapping(0);

        const GLFWvidmode * mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
        if (settings.fullscreen)
            glfwSetWindowMonitor(hud_window->impl, glfwGetPrimaryMonitor(), 0, 0, settings.window_width,
                                 settings.window_height, mode->refreshRate);
        else
            glfwSetWindowMonitor(hud_window->impl, NULL, (mode->width - settings.window_width) / 2,
                                 (mode->height - settings.window_height) / 2, settings.window_width, settings.window_height,
                                 0);
    #endif

    #ifdef USE_SDL
        SDL_SetWindowSize(hud_window->impl, settings.window_width, settings.window_height);

        if (settings.vsync < 2)
            window_swapping(settings.vsync);
        if (settings.vsync > 1)
            window_swapping(0);

        if (settings.fullscreen)
            SDL_SetWindowFullscreen(hud_window->impl, SDL_WINDOW_FULLSCREEN);
        else
            SDL_SetWindowFullscreen(hud_window->impl, 0);
    #endif
}

int window_pressed_keys[64] = {0};

int window_key_down(int key) {
    return window_pressed_keys[key];
}

int window_cpucores() {
    #ifdef OS_LINUX
        #ifdef USE_TOUCH
            return sysconf(_SC_NPROCESSORS_CONF);
        #else
            return get_nprocs();
        #endif
    #endif

    #ifdef OS_WINDOWS
        SYSTEM_INFO info;
        GetSystemInfo(&info);
        return info.dwNumberOfProcessors;
    #endif

    #ifdef OS_HAIKU
        system_info info;
        get_system_info(&info);
        return info.cpu_count;
    #endif

    return 1;
}
