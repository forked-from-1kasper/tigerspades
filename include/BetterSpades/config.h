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

#ifndef CONFIG_H
#define CONFIG_H

#include <stddef.h>

#include <BetterSpades/common.h>
#include <BetterSpades/list.h>

struct config_file_entry {
    char section[32];
    char name[32];
    char value[32];
};

extern struct RENDER_OPTIONS {
    char name[16];
    int opengl14;
    int color_correction;
    int shadow_entities;
    int ambient_occlusion;
    float render_distance;
    int window_width;
    int window_height;
    int multisamples;
    int player_arms;
    int fullscreen;
    int greedy_meshing;
    int vsync;
    float mouse_sensitivity;
    int show_news;
    int show_fps;
    int volume;
    int voxlap_models;
    int force_displaylist;
    int invert_y;
    int smooth_fog;
    float camera_fov;
    int hold_down_sights;
    int chat_shadow;
} settings, settings_tmp;

extern struct list config_keys;

struct config_key_pair {
    int internal;
    int def;
    int original;
    int toggle;
    char name[24];
    char display[24];
    char category[24];
};

enum {
    CONFIG_TYPE_STRING,
    CONFIG_TYPE_INT,
    CONFIG_TYPE_FLOAT,
};

struct config_setting {
    void* value;
    int type;
    int min;
    int max;
    char name[32];
    char help[32];
    int defaults[8];
    int defaults_length;
    void (*label_callback)(char* buffer, size_t length, int value, size_t index);
};

extern struct list config_settings;

void config_register_key(int internal, int def, const char* name, int toggle, const char* display,
                         const char* category);
int config_key_translate(int key, int dir, int* results);
struct config_key_pair* config_key(int key);
void config_key_reset_togglestates();
void config_reload(void);
void config_save(void);

#ifdef USE_GLFW
    #define TOOLKIT_KEY_SPACE       GLFW_KEY_SPACE
    #define TOOLKIT_KEY_LSHIFT      GLFW_KEY_LEFT_SHIFT
    #define TOOLKIT_KEY_RSHIFT      GLFW_KEY_RIGHT_SHIFT
    #define TOOLKIT_KEY_LCTRL       GLFW_KEY_LEFT_CONTROL
    #define TOOLKIT_KEY_RCTRL       GLFW_KEY_RIGHT_CONTROL
    #define TOOLKIT_KEY_LALT        GLFW_KEY_LEFT_ALT
    #define TOOLKIT_KEY_RALT        GLFW_KEY_RIGHT_ALT
    #define TOOLKIT_KEY_UP          GLFW_KEY_UP
    #define TOOLKIT_KEY_DOWN        GLFW_KEY_DOWN
    #define TOOLKIT_KEY_LEFT        GLFW_KEY_LEFT
    #define TOOLKIT_KEY_RIGHT       GLFW_KEY_RIGHT
    #define TOOLKIT_KEY_BACK        GLFW_KEY_BACKSPACE
    #define TOOLKIT_KEY_RETURN      GLFW_KEY_ENTER
    #define TOOLKIT_KEY_PAGEUP      GLFW_KEY_PAGE_UP
    #define TOOLKIT_KEY_PAGEDOWN    GLFW_KEY_PAGE_DOWN
    #define TOOLKIT_KEY_HOME        GLFW_KEY_HOME
    #define TOOLKIT_KEY_END         GLFW_KEY_END
    #define TOOLKIT_KEY_INSERT      GLFW_KEY_INSERT
    #define TOOLKIT_KEY_DELETE      GLFW_KEY_DELETE
    #define TOOLKIT_KEY_PAUSE       GLFW_KEY_PAUSE
    #define TOOLKIT_KEY_MENU        GLFW_KEY_MENU
    #define TOOLKIT_KEY_LSUPER      GLFW_KEY_LEFT_SUPER
    #define TOOLKIT_KEY_RSUPER      GLFW_KEY_RIGHT_SUPER
    #define TOOLKIT_KEY_PRTSC       GLFW_KEY_PRINT_SCREEN
    #define TOOLKIT_KEY_TAB         GLFW_KEY_TAB
    #define TOOLKIT_KEY_ESC         GLFW_KEY_ESCAPE
    #define TOOLKIT_KEY_COMMA       GLFW_KEY_COMMA
    #define TOOLKIT_KEY_PERIOD      GLFW_KEY_PERIOD
    #define TOOLKIT_KEY_SLASH       GLFW_KEY_SLASH
    #define TOOLKIT_KEY_NUM_LOCK    GLFW_KEY_NUM_LOCK
    #define TOOLKIT_KEY_CAPS_LOCK   GLFW_KEY_CAPS_LOCK
    #define TOOLKIT_KEY_SCROLL_LOCK GLFW_KEY_SCROLL_LOCK
    #define TOOLKIT_KEY_1           GLFW_KEY_1
    #define TOOLKIT_KEY_2           GLFW_KEY_2
    #define TOOLKIT_KEY_3           GLFW_KEY_3
    #define TOOLKIT_KEY_4           GLFW_KEY_4
    #define TOOLKIT_KEY_5           GLFW_KEY_5
    #define TOOLKIT_KEY_6           GLFW_KEY_6
    #define TOOLKIT_KEY_7           GLFW_KEY_7
    #define TOOLKIT_KEY_8           GLFW_KEY_8
    #define TOOLKIT_KEY_9           GLFW_KEY_9
    #define TOOLKIT_KEY_A           GLFW_KEY_A
    #define TOOLKIT_KEY_B           GLFW_KEY_B
    #define TOOLKIT_KEY_C           GLFW_KEY_C
    #define TOOLKIT_KEY_D           GLFW_KEY_D
    #define TOOLKIT_KEY_E           GLFW_KEY_E
    #define TOOLKIT_KEY_F           GLFW_KEY_F
    #define TOOLKIT_KEY_G           GLFW_KEY_G
    #define TOOLKIT_KEY_H           GLFW_KEY_H
    #define TOOLKIT_KEY_I           GLFW_KEY_I
    #define TOOLKIT_KEY_J           GLFW_KEY_J
    #define TOOLKIT_KEY_K           GLFW_KEY_K
    #define TOOLKIT_KEY_L           GLFW_KEY_L
    #define TOOLKIT_KEY_M           GLFW_KEY_M
    #define TOOLKIT_KEY_N           GLFW_KEY_N
    #define TOOLKIT_KEY_O           GLFW_KEY_O
    #define TOOLKIT_KEY_P           GLFW_KEY_P
    #define TOOLKIT_KEY_Q           GLFW_KEY_Q
    #define TOOLKIT_KEY_R           GLFW_KEY_R
    #define TOOLKIT_KEY_S           GLFW_KEY_S
    #define TOOLKIT_KEY_T           GLFW_KEY_T
    #define TOOLKIT_KEY_U           GLFW_KEY_U
    #define TOOLKIT_KEY_V           GLFW_KEY_V
    #define TOOLKIT_KEY_W           GLFW_KEY_W
    #define TOOLKIT_KEY_X           GLFW_KEY_X
    #define TOOLKIT_KEY_Y           GLFW_KEY_Y
    #define TOOLKIT_KEY_Z           GLFW_KEY_Z
    #define TOOLKIT_KEY_F1          GLFW_KEY_F1
    #define TOOLKIT_KEY_F2          GLFW_KEY_F2
    #define TOOLKIT_KEY_F3          GLFW_KEY_F3
    #define TOOLKIT_KEY_F4          GLFW_KEY_F4
    #define TOOLKIT_KEY_F5          GLFW_KEY_F5
    #define TOOLKIT_KEY_F6          GLFW_KEY_F6
    #define TOOLKIT_KEY_F7          GLFW_KEY_F7
    #define TOOLKIT_KEY_F8          GLFW_KEY_F8
    #define TOOLKIT_KEY_F9          GLFW_KEY_F9
    #define TOOLKIT_KEY_F10         GLFW_KEY_F10
    #define TOOLKIT_KEY_F11         GLFW_KEY_F11
    #define TOOLKIT_KEY_F12         GLFW_KEY_F12
    #define TOOLKIT_KEY_F13         GLFW_KEY_F13
    #define TOOLKIT_KEY_F14         GLFW_KEY_F14
    #define TOOLKIT_KEY_F15         GLFW_KEY_F15
    #define TOOLKIT_KEY_F16         GLFW_KEY_F16
    #define TOOLKIT_KEY_F17         GLFW_KEY_F17
    #define TOOLKIT_KEY_F18         GLFW_KEY_F18
    #define TOOLKIT_KEY_F19         GLFW_KEY_F19
    #define TOOLKIT_KEY_F20         GLFW_KEY_F20
    #define TOOLKIT_KEY_F21         GLFW_KEY_F21
    #define TOOLKIT_KEY_F22         GLFW_KEY_F22
    #define TOOLKIT_KEY_F23         GLFW_KEY_F23
    #define TOOLKIT_KEY_F24         GLFW_KEY_F24
    #define TOOLKIT_KEY_KP_0        GLFW_KEY_KP_0
    #define TOOLKIT_KEY_KP_1        GLFW_KEY_KP_1
    #define TOOLKIT_KEY_KP_2        GLFW_KEY_KP_2
    #define TOOLKIT_KEY_KP_3        GLFW_KEY_KP_3
    #define TOOLKIT_KEY_KP_4        GLFW_KEY_KP_4
    #define TOOLKIT_KEY_KP_5        GLFW_KEY_KP_5
    #define TOOLKIT_KEY_KP_6        GLFW_KEY_KP_6
    #define TOOLKIT_KEY_KP_7        GLFW_KEY_KP_7
    #define TOOLKIT_KEY_KP_8        GLFW_KEY_KP_8
    #define TOOLKIT_KEY_KP_9        GLFW_KEY_KP_9
    #define TOOLKIT_KEY_KP_ADD      GLFW_KEY_KP_ADD
    #define TOOLKIT_KEY_KP_SUBTRACT GLFW_KEY_KP_SUBTRACT
    #define TOOLKIT_KEY_KP_MULTIPLY GLFW_KEY_KP_MULTIPLY
    #define TOOLKIT_KEY_KP_DIVIDE   GLFW_KEY_KP_DIVIDE
    #define TOOLKIT_KEY_KP_EQUAL    GLFW_KEY_KP_EQUAL
    #define TOOLKIT_KEY_KP_ENTER    GLFW_KEY_KP_ENTER
#endif

#ifdef USE_SDL
    #define TOOLKIT_KEY_SPACE       SDLK_SPACE
    #define TOOLKIT_KEY_LSHIFT      SDLK_LSHIFT
    #define TOOLKIT_KEY_RSHIFT      SDLK_RSHIFT
    #define TOOLKIT_KEY_LCTRL       SDLK_LCTRL
    #define TOOLKIT_KEY_RCTRL       SDLK_RCTRL
    #define TOOLKIT_KEY_LALT        SDLK_LALT
    #define TOOLKIT_KEY_RALT        SDLK_RALT
    #define TOOLKIT_KEY_UP          SDLK_UP
    #define TOOLKIT_KEY_DOWN        SDLK_DOWN
    #define TOOLKIT_KEY_LEFT        SDLK_LEFT
    #define TOOLKIT_KEY_RIGHT       SDLK_RIGHT
    #define TOOLKIT_KEY_BACK        SDLK_BACKSPACE
    #define TOOLKIT_KEY_RETURN      SDLK_RETURN
    #define TOOLKIT_KEY_PAGEUP      SDLK_PAGEUP
    #define TOOLKIT_KEY_PAGEDOWN    SDLK_PAGEDOWN
    #define TOOLKIT_KEY_HOME        SDLK_HOME
    #define TOOLKIT_KEY_END         SDLK_END
    #define TOOLKIT_KEY_INSERT      SDLK_INSERT
    #define TOOLKIT_KEY_DELETE      SDLK_DELETE
    #define TOOLKIT_KEY_PAUSE       SDLK_PAUSE
    #define TOOLKIT_KEY_MENU        SDLK_MENU
    #define TOOLKIT_KEY_LSUPER      SDLK_LGUI
    #define TOOLKIT_KEY_RSUPER      SDLK_RGUI
    #define TOOLKIT_KEY_PRTSC       SDLK_PRINTSCREEN
    #define TOOLKIT_KEY_TAB         SDLK_TAB
    #define TOOLKIT_KEY_ESC         SDLK_ESCAPE
    #define TOOLKIT_KEY_COMMA       SDLK_COMMA
    #define TOOLKIT_KEY_PERIOD      SDLK_PERIOD
    #define TOOLKIT_KEY_SLASH       SDLK_SLASH
    #define TOOLKIT_KEY_NUM_LOCK    SDLK_NUMLOCKCLEAR
    #define TOOLKIT_KEY_CAPS_LOCK   SDLK_CAPSLOCK
    #define TOOLKIT_KEY_SCROLL_LOCK SDLK_SCROLLLOCK
    #define TOOLKIT_KEY_1           SDLK_1
    #define TOOLKIT_KEY_2           SDLK_2
    #define TOOLKIT_KEY_3           SDLK_3
    #define TOOLKIT_KEY_4           SDLK_4
    #define TOOLKIT_KEY_5           SDLK_5
    #define TOOLKIT_KEY_6           SDLK_6
    #define TOOLKIT_KEY_7           SDLK_7
    #define TOOLKIT_KEY_8           SDLK_8
    #define TOOLKIT_KEY_9           SDLK_9
    #define TOOLKIT_KEY_A           SDLK_a
    #define TOOLKIT_KEY_B           SDLK_b
    #define TOOLKIT_KEY_C           SDLK_c
    #define TOOLKIT_KEY_D           SDLK_d
    #define TOOLKIT_KEY_E           SDLK_e
    #define TOOLKIT_KEY_F           SDLK_f
    #define TOOLKIT_KEY_G           SDLK_g
    #define TOOLKIT_KEY_H           SDLK_h
    #define TOOLKIT_KEY_I           SDLK_i
    #define TOOLKIT_KEY_J           SDLK_j
    #define TOOLKIT_KEY_K           SDLK_k
    #define TOOLKIT_KEY_L           SDLK_l
    #define TOOLKIT_KEY_M           SDLK_m
    #define TOOLKIT_KEY_N           SDLK_n
    #define TOOLKIT_KEY_O           SDLK_o
    #define TOOLKIT_KEY_P           SDLK_p
    #define TOOLKIT_KEY_Q           SDLK_q
    #define TOOLKIT_KEY_R           SDLK_r
    #define TOOLKIT_KEY_S           SDLK_s
    #define TOOLKIT_KEY_T           SDLK_t
    #define TOOLKIT_KEY_U           SDLK_u
    #define TOOLKIT_KEY_V           SDLK_v
    #define TOOLKIT_KEY_W           SDLK_w
    #define TOOLKIT_KEY_X           SDLK_x
    #define TOOLKIT_KEY_Y           SDLK_y
    #define TOOLKIT_KEY_Z           SDLK_z
    #define TOOLKIT_KEY_F1          SDLK_F1
    #define TOOLKIT_KEY_F2          SDLK_F2
    #define TOOLKIT_KEY_F3          SDLK_F3
    #define TOOLKIT_KEY_F4          SDLK_F4
    #define TOOLKIT_KEY_F5          SDLK_F5
    #define TOOLKIT_KEY_F6          SDLK_F6
    #define TOOLKIT_KEY_F7          SDLK_F7
    #define TOOLKIT_KEY_F8          SDLK_F8
    #define TOOLKIT_KEY_F9          SDLK_F9
    #define TOOLKIT_KEY_F10         SDLK_F10
    #define TOOLKIT_KEY_F11         SDLK_F11
    #define TOOLKIT_KEY_F12         SDLK_F12
    #define TOOLKIT_KEY_F13         SDLK_F13
    #define TOOLKIT_KEY_F14         SDLK_F14
    #define TOOLKIT_KEY_F15         SDLK_F15
    #define TOOLKIT_KEY_F16         SDLK_F16
    #define TOOLKIT_KEY_F17         SDLK_F17
    #define TOOLKIT_KEY_F18         SDLK_F18
    #define TOOLKIT_KEY_F19         SDLK_F19
    #define TOOLKIT_KEY_F20         SDLK_F20
    #define TOOLKIT_KEY_F21         SDLK_F21
    #define TOOLKIT_KEY_F22         SDLK_F22
    #define TOOLKIT_KEY_F23         SDLK_F23
    #define TOOLKIT_KEY_F24         SDLK_F24
    #define TOOLKIT_KEY_KP_0        SDLK_KP_0
    #define TOOLKIT_KEY_KP_1        SDLK_KP_1
    #define TOOLKIT_KEY_KP_2        SDLK_KP_2
    #define TOOLKIT_KEY_KP_3        SDLK_KP_3
    #define TOOLKIT_KEY_KP_4        SDLK_KP_4
    #define TOOLKIT_KEY_KP_5        SDLK_KP_5
    #define TOOLKIT_KEY_KP_6        SDLK_KP_6
    #define TOOLKIT_KEY_KP_7        SDLK_KP_7
    #define TOOLKIT_KEY_KP_8        SDLK_KP_8
    #define TOOLKIT_KEY_KP_9        SDLK_KP_9
    #define TOOLKIT_KEY_KP_ADD      SDLK_KP_PLUS
    #define TOOLKIT_KEY_KP_SUBTRACT SDLK_KP_MINUS
    #define TOOLKIT_KEY_KP_MULTIPLY SDLK_KP_MULTIPLY
    #define TOOLKIT_KEY_KP_DIVIDE   SDLK_KP_DIVIDE
    #define TOOLKIT_KEY_KP_EQUAL    SDLK_KP_EQUALS
    #define TOOLKIT_KEY_KP_ENTER    SDLK_KP_ENTER
#endif

#endif
