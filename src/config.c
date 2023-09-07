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
#include <float.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <math.h>

#include <BetterSpades/window.h>
#include <BetterSpades/file.h>
#include <BetterSpades/config.h>
#include <BetterSpades/sound.h>
#include <BetterSpades/model.h>
#include <BetterSpades/camera.h>
#include <BetterSpades/utils.h>

#include <ini.h>

struct RENDER_OPTIONS settings, settings_tmp;

List config_keys, config_settings, config_file, config_keybind;

static void config_sets(const char * section, const char * name, const char * value) {
    for (int k = 0; k < list_size(&config_file); k++) {
        struct config_file_entry * e = list_get(&config_file, k);
        if (strcmp(e->name, name) == 0) {
            strncpy(e->value, value, sizeof(e->value) - 1);
            return;
        }
    }
    struct config_file_entry e;
    strncpy(e.section, section, sizeof(e.section) - 1);
    strncpy(e.name, name, sizeof(e.name) - 1);
    strncpy(e.value, value, sizeof(e.value) - 1);
    list_add(&config_file, &e);
}

static void config_seti(const char * section, const char * name, int value) {
    char tmp[32];
    sprintf(tmp, "%i", value);
    config_sets(section, name, tmp);
}

static void config_setf(const char * section, const char * name, float value) {
    char tmp[32];
    sprintf(tmp, "%0.6f", value);
    config_sets(section, name, tmp);
}

void config_save() {
    kv6_rebuild_complete();

    config_sets("client", "name",              settings.name);
    config_seti("client", "xres",              settings.window_width);
    config_seti("client", "yres",              settings.window_height);
    config_seti("client", "windowed",          !settings.fullscreen);
    config_seti("client", "multisamples",      settings.multisamples);
    config_seti("client", "greedy_meshing",    settings.greedy_meshing);
    config_seti("client", "vsync",             settings.vsync);
    config_setf("client", "mouse_sensitivity", settings.mouse_sensitivity);
    config_seti("client", "show_news",         settings.show_news);
    config_seti("client", "vol",               settings.volume);
    config_seti("client", "show_fps",          settings.show_fps);
    config_seti("client", "voxlap_models",     settings.voxlap_models);
    config_seti("client", "force_displaylist", settings.force_displaylist);
    config_seti("client", "inverty",           settings.invert_y);
    config_seti("client", "smooth_fog",        settings.smooth_fog);
    config_seti("client", "ambient_occlusion", settings.ambient_occlusion);
    config_setf("client", "camera_fov",        settings.camera_fov);
    config_seti("client", "hold_down_sights",  settings.hold_down_sights);
    config_seti("client", "chat_shadow",       settings.chat_shadow);
    config_seti("client", "show_player_arms",  settings.player_arms);
    config_seti("client", "scale",             settings.scale);

    for (int k = 0; k < list_size(&config_keys); k++) {
        struct config_key_pair * e = list_get(&config_keys, k);
        if (strlen(e->name) > 0)
            config_seti("controls", e->name, e->def);
    }

    void * f = file_open("config.ini", "w");
    if (f) {
        char last_section[32] = {0};
        for (int k = 0; k < list_size(&config_file); k++) {
            struct config_file_entry * e = list_get(&config_file, k);
            if (strcmp(e->section, last_section) != 0) {
                file_printf(f, "\r\n[%s]\r\n", e->section);
                strcpy(last_section, e->section);
            }

            file_printf(f, "%s", e->name);

            for (int l = 0; l < 31 - strlen(e->name); l++)
                file_printf(f, " ");

            file_printf(f, "= %s\r\n", e->value);
        }

        file_printf(f, "\r\n[keybind]\r\n");
        for (int k = 0; k < list_size(&config_keybind); k++) {
            Keybind * keybind = list_get(&config_keybind, k);

            if (keybind->key > 0 && strlen(keybind->value) > 0)
                file_printf(f, "%d = %s\r\n", keybind->key, keybind->value);
        }

        file_close(f);
    }
}

static int config_read_key(void * user, const char * section, const char * name, const char * value) {
    if (strcmp(section, "keybind") != 0) {
        struct config_file_entry e;
        strncpy(e.section, section, sizeof(e.section) - 1);
        strncpy(e.name, name, sizeof(e.name) - 1);
        strncpy(e.value, value, sizeof(e.value) - 1);
        list_add(&config_file, &e);
    } else {
        Keybind * keybind = list_add(&config_keybind, NULL);
        keybind->key = atoi(name);
        strncpy(keybind->value, value, sizeof(keybind->value));
    }

    if (!strcmp(section, "client")) {
        if (!strcmp(name, "name")) {
            strcpy(settings.name, value);
        } else if (!strcmp(name, "xres")) {
            settings.window_width = atoi(value);
        } else if (!strcmp(name, "yres")) {
            settings.window_height = atoi(value);
        } else if (!strcmp(name, "windowed")) {
            settings.fullscreen = !atoi(value);
        } else if (!strcmp(name, "multisamples")) {
            settings.multisamples = atoi(value);
        } else if (!strcmp(name, "greedy_meshing")) {
            settings.greedy_meshing = atoi(value);
        } else if (!strcmp(name, "vsync")) {
            settings.vsync = atoi(value);
        } else if (!strcmp(name, "mouse_sensitivity")) {
            settings.mouse_sensitivity = atof(value);
        } else if (!strcmp(name, "show_news")) {
            settings.show_news = atoi(value);
        } else if (!strcmp(name, "vol")) {
            settings.volume = max(min(atoi(value), 10), 0);
            sound_volume(settings.volume / 10.0F);
        } else if (!strcmp(name, "show_fps")) {
            settings.show_fps = atoi(value);
        } else if (!strcmp(name, "voxlap_models")) {
            settings.voxlap_models = atoi(value);
        } else if (!strcmp(name, "force_displaylist")) {
            settings.force_displaylist = atoi(value);
        } else if (!strcmp(name, "inverty")) {
            settings.invert_y = atoi(value);
        } else if (!strcmp(name, "smooth_fog")) {
            settings.smooth_fog = atoi(value);
        } else if (!strcmp(name, "ambient_occlusion")) {
            settings.ambient_occlusion = atoi(value);
        } else if (!strcmp(name, "camera_fov")) {
            settings.camera_fov = fmax(fmin(atof(value), CAMERA_MAX_FOV), CAMERA_DEFAULT_FOV);
        } else if (!strcmp(name, "hold_down_sights")) {
            settings.hold_down_sights = atoi(value);
        } else if (!strcmp(name, "chat_shadow")) {
            settings.chat_shadow = atoi(value);
        } else if (!strcmp(name, "show_player_arms")) {
            settings.player_arms = atoi(value);
        } else if (!strcmp(name, "scale")) {
            settings.scale = atoi(value);
        }
    }

    if (!strcmp(section, "controls")) {
        for (int k = 0; k < list_size(&config_keys); k++) {
            struct config_key_pair * key = list_get(&config_keys, k);
            if (!strcmp(name, key->name)) {
                log_debug("found override for %s, from %i to %i", key->name, key->def, atoi(value));
                key->def = strtol(value, NULL, 0);
                break;
            }
        }
    }

    return 1;
}

void config_register_key(int internal, int def, const char * name, int toggle, const char * display,
                         const char * category) {
    struct config_key_pair key;
    key.internal = internal;
    key.def = def;
    key.original = def;
    key.toggle = toggle;
    if (display)
        strncpy(key.display, display, sizeof(key.display) - 1);
    else
        *key.display = 0;

    if (name)
        strncpy(key.name, name, sizeof(key.name) - 1);
    else
        *key.name = 0;

    if (category)
        strncpy(key.category, category, sizeof(key.category) - 1);
    else
        *key.category = 0;
    list_add(&config_keys, &key);
}

int config_key_translate(int key, int dir, int * results) {
    int count = 0;

    for (int k = 0; k < list_size(&config_keys); k++) {
        struct config_key_pair* a = list_get(&config_keys, k);

        if (dir && a->internal == key) {
            if (results)
                results[count] = a->def;
            count++;
        } else if (!dir && a->def == key) {
            if (results)
                results[count] = a->internal;
            count++;
        }
    }

    return count;
}

struct config_key_pair * config_key(int key) {
    for (int k = 0; k < list_size(&config_keys); k++) {
        struct config_key_pair * a = list_get(&config_keys, k);
        if (a->internal == key)
            return a;
    }
    return NULL;
}

void config_key_reset_togglestates() {
    for (int k = 0; k < list_size(&config_keys); k++) {
        struct config_key_pair * a = list_get(&config_keys, k);
        if (a->toggle)
            window_pressed_keys[a->internal] = 0;
    }
}

static int config_key_cmp(const void * a, const void * b) {
    const struct config_key_pair * A = (const struct config_key_pair*) a;
    const struct config_key_pair * B = (const struct config_key_pair*) b;

    int cmp = strcmp(A->category, B->category);
    return cmp ? cmp : strcmp(A->display, B->display);
}

static void config_label_scale(char * buffer, size_t length, int value, size_t index) {
    if (value == 0) {
        snprintf(buffer, length, "Auto");
    } else {
        snprintf(buffer, length, "%i", value);
    }
}

static void config_label_pixels(char * buffer, size_t length, int value, size_t index) {
    if (value == 800 || value == 600) {
        snprintf(buffer, length, "default: %ipx", value);
    } else {
        snprintf(buffer, length, "%ipx", value);
    }
}

static void config_label_vsync(char * buffer, size_t length, int value, size_t index) {
    if (value == 0) {
        snprintf(buffer, length, "disabled");
    } else if (value == 1) {
        snprintf(buffer, length, "enabled");
    } else {
        snprintf(buffer, length, "max %i fps", value);
    }
}

static void config_label_msaa(char * buffer, size_t length, int value, size_t index) {
    if (index == 0) {
        snprintf(buffer, length, "No MSAA");
    } else {
        snprintf(buffer, length, "%ix MSAA", value);
    }
}

void config_reload() {
    if (!list_created(&config_keybind))
        list_create(&config_keybind, sizeof(Keybind));
    else
        list_clear(&config_keybind);

    if (!list_created(&config_file))
        list_create(&config_file, sizeof(struct config_file_entry));
    else
        list_clear(&config_file);

    if (!list_created(&config_keys))
        list_create(&config_keys, sizeof(struct config_key_pair));
    else
        list_clear(&config_keys);

    config_register_key(WINDOW_KEY_UP,           TOOLKIT_KEY_UP, "move_forward", 0, "Forward", "Movement");
    config_register_key(WINDOW_KEY_DOWN,         TOOLKIT_KEY_DOWN, "move_backward", 0, "Backward", "Movement");
    config_register_key(WINDOW_KEY_LEFT,         TOOLKIT_KEY_LEFT, "move_left", 0, "Left", "Movement");
    config_register_key(WINDOW_KEY_RIGHT,        TOOLKIT_KEY_RIGHT, "move_right", 0, "Right", "Movement");
    config_register_key(WINDOW_KEY_SPACE,        TOOLKIT_KEY_JUMP, "jump", 0, "Jump", "Movement");
    config_register_key(WINDOW_KEY_SPRINT,       TOOLKIT_KEY_SPRINT, "sprint", 0, "Sprint", "Movement");
    config_register_key(WINDOW_KEY_SHIFT,        TOOLKIT_KEY_SPRINT, NULL, 0, NULL, NULL);
    config_register_key(WINDOW_KEY_CURSOR_UP,    TOOLKIT_KEY_CURSOR_UP, "cube_color_up", 0, "Color up", "Block");
    config_register_key(WINDOW_KEY_CURSOR_DOWN,  TOOLKIT_KEY_CURSOR_DOWN, "cube_color_down", 0, "Color down", "Block");
    config_register_key(WINDOW_KEY_CURSOR_LEFT,  TOOLKIT_KEY_CURSOR_LEFT, "cube_color_left", 0, "Color left", "Block");
    config_register_key(WINDOW_KEY_CURSOR_RIGHT, TOOLKIT_KEY_CURSOR_RIGHT, "cube_color_right", 0, "Color right", "Block");
    config_register_key(WINDOW_KEY_BACKSPACE,    TOOLKIT_KEY_BACKSPACE, NULL, 0, NULL, NULL);
    config_register_key(WINDOW_KEY_TOOL1,        TOOLKIT_KEY_TOOL1, "tool_spade", 0, "Select spade", "Tools & Weapons");
    config_register_key(WINDOW_KEY_TOOL2,        TOOLKIT_KEY_TOOL2, "tool_block", 0, "Select block", "Tools & Weapons");
    config_register_key(WINDOW_KEY_TOOL3,        TOOLKIT_KEY_TOOL3, "tool_gun", 0, "Select gun", "Tools & Weapons");
    config_register_key(WINDOW_KEY_TOOL4,        TOOLKIT_KEY_TOOL4, "tool_grenade", 0, "Select grenade", "Tools & Weapons");
    config_register_key(WINDOW_KEY_TAB,          TOOLKIT_KEY_TAB, "view_score", 0, "Score", "Information");
    config_register_key(WINDOW_KEY_ESCAPE,       TOOLKIT_KEY_ESCAPE, "quit_game", 0, "Quit", "Game");
    config_register_key(WINDOW_KEY_MAP,          TOOLKIT_KEY_MAP, "view_map", 1, "Map", "Information");
    config_register_key(WINDOW_KEY_CROUCH,       TOOLKIT_KEY_CROUCH, "crouch", 0, "Crouch", "Movement");
    config_register_key(WINDOW_KEY_SNEAK,        TOOLKIT_KEY_SNEAK, "sneak", 0, "Sneak", "Movement");
    config_register_key(WINDOW_KEY_ENTER,        TOOLKIT_KEY_ENTER, NULL, 0, NULL, NULL);
    config_register_key(WINDOW_KEY_F1,           TOOLKIT_KEY_F1, NULL, 0, NULL, NULL);
    config_register_key(WINDOW_KEY_F2,           TOOLKIT_KEY_F2, NULL, 0, NULL, NULL);
    config_register_key(WINDOW_KEY_F3,           TOOLKIT_KEY_F3, NULL, 0, NULL, NULL);
    config_register_key(WINDOW_KEY_F4,           TOOLKIT_KEY_F4, NULL, 0, NULL, NULL);
    config_register_key(WINDOW_KEY_YES,          TOOLKIT_KEY_YES, NULL, 0, NULL, NULL);
    config_register_key(WINDOW_KEY_NO,           TOOLKIT_KEY_NO, NULL, 0, NULL, NULL);
    config_register_key(WINDOW_KEY_VOLUME_UP,    TOOLKIT_KEY_VOLUME_UP, "volume_up", 0, "Volume up", "Game");
    config_register_key(WINDOW_KEY_VOLUME_DOWN,  TOOLKIT_KEY_VOLUME_DOWN, "volume_down", 0, "Volume down", "Game");
    config_register_key(WINDOW_KEY_V,            TOOLKIT_KEY_SNEAK, NULL, 0, NULL, NULL);
    config_register_key(WINDOW_KEY_RELOAD,       TOOLKIT_KEY_RELOAD, "reload", 0, "Reload", "Tools & Weapons");
    config_register_key(WINDOW_KEY_CHAT,         TOOLKIT_KEY_CHAT, "chat_global", 0, "Chat", "Game");
    config_register_key(WINDOW_KEY_FULLSCREEN,   TOOLKIT_KEY_FULLSCREEN, "fullscreen", 0, "Fullscreen", "Game");
    config_register_key(WINDOW_KEY_SCREENSHOT,   TOOLKIT_KEY_SCREENSHOT, "screenshot", 0, "Screenshot", "Game");
    config_register_key(WINDOW_KEY_CHANGETEAM,   TOOLKIT_KEY_CHANGETEAM, "change_team", 0, "Team select", "Game");
    config_register_key(WINDOW_KEY_CHANGEWEAPON, TOOLKIT_KEY_CHANGEWEAPON, "change_weapon", 0, "Gun select", "Tools & Weapons");
    config_register_key(WINDOW_KEY_PICKCOLOR,    TOOLKIT_KEY_PICKCOLOR, "cube_color_sample", 0, "Pick color", "Block");
    config_register_key(WINDOW_KEY_COMMAND,      TOOLKIT_KEY_COMMAND, "chat_command", 0, "Command", "Game");
    config_register_key(WINDOW_KEY_HIDEHUD,      TOOLKIT_KEY_HIDEHUD, "hide_hud", 1, "Hide HUD", "Game");
    config_register_key(WINDOW_KEY_LASTTOOL,     TOOLKIT_KEY_LASTTOOL, "last_tool", 0, "Last tool", "Tools & Weapons");
    config_register_key(WINDOW_KEY_NETWORKSTATS, TOOLKIT_KEY_NETWORKSTATS, "network_stats", 1, "Network stats", "Information");
    config_register_key(WINDOW_KEY_SAVE_MAP,     TOOLKIT_KEY_SAVE_MAP, "save_map", 0, "Save map", "Game");
    config_register_key(WINDOW_KEY_SELECT1,      TOOLKIT_KEY_SELECT1, NULL, 0, NULL, NULL);
    config_register_key(WINDOW_KEY_SELECT2,      TOOLKIT_KEY_SELECT2, NULL, 0, NULL, NULL);
    config_register_key(WINDOW_KEY_SELECT3,      TOOLKIT_KEY_SELECT3, NULL, 0, NULL, NULL);

    list_sort(&config_keys, config_key_cmp);

    char * s = file_load("config.ini");
    if (s) {
        ini_parse_string(s, config_read_key, NULL);
        free(s);
    }

    if (!list_created(&config_settings))
        list_create(&config_settings, sizeof(struct config_setting));
    else
        list_clear(&config_settings);

    list_add(&config_settings,
             &(struct config_setting) {
                 .value = settings_tmp.name,
                 .type = CONFIG_TYPE_STRING,
                 .max = sizeof(settings.name) - 1,
                 .name = "Name",
                 .help = "Ingame player name",
             });
    list_add(&config_settings,
             &(struct config_setting) {
                 .value = &settings_tmp.mouse_sensitivity,
                 .type = CONFIG_TYPE_FLOAT,
                 .min = 0,
                 .max = INT_MAX,
                 .name = "Mouse sensitivity",
             });
    list_add(&config_settings,
             &(struct config_setting) {
                 .value = &settings_tmp.camera_fov,
                 .type = CONFIG_TYPE_FLOAT,
                 .min = CAMERA_DEFAULT_FOV,
                 .max = CAMERA_MAX_FOV,
                 .name = "Camera FOV",
                 .help = "Field of View in degrees",
             });
    list_add(&config_settings,
             &(struct config_setting) {
                 .value = &settings_tmp.volume,
                 .type = CONFIG_TYPE_INT,
                 .min = 0,
                 .max = 10,
                 .name = "Volume",
             });
    list_add(&config_settings,
             &(struct config_setting) {
                 .value = &settings_tmp.window_width,
                 .type = CONFIG_TYPE_INT,
                 .min = 0,
                 .max = INT_MAX,
                 .name = "Game width",
                 .defaults = {640, 800, 854, 1024, 1280, 1920, 3840},
                 .defaults_length = 7,
                 .help = "Default: 800",
                 .label_callback = config_label_pixels,
             });
    list_add(&config_settings,
             &(struct config_setting) {
                 .value = &settings_tmp.window_height,
                 .type = CONFIG_TYPE_INT,
                 .min = 0,
                 .max = INT_MAX,
                 .name = "Game height",
                 .defaults = {480, 600, 720, 768, 1024, 1080, 2160},
                 .defaults_length = 7,
                 .help = "Default: 600",
                 .label_callback = config_label_pixels,
             });
    list_add(&config_settings,
             &(struct config_setting) {
                 .value = &settings_tmp.scale,
                 .type = CONFIG_TYPE_INT,
                 .min = 0,
                 .max = INT_MAX,
                 .name = "GUI scale",
                 .defaults = {0, 1, 2, 4, 8, 16, 32, 64},
                 .defaults_length = 8,
                 .label_callback = config_label_scale,
             });
    list_add(&config_settings,
             &(struct config_setting) {
                 .value = &settings_tmp.vsync,
                 .type = CONFIG_TYPE_INT,
                 .min = 0,
                 .max = INT_MAX,
                 .name = "V-Sync",
                 .help = "Limits your game's fps",
                 .defaults = {0, 1, 20, 30, 60, 120, 144, 240},
                 .defaults_length = 8,
                 .label_callback = config_label_vsync,
             });
    list_add(&config_settings,
             &(struct config_setting) {
                 .value = &settings_tmp.fullscreen,
                 .type = CONFIG_TYPE_INT,
                 .min = 0,
                 .max = 1,
                 .name = "Fullscreen",
             });
    list_add(&config_settings,
             &(struct config_setting) {
                 .value = &settings_tmp.multisamples,
                 .type = CONFIG_TYPE_INT,
                 .min = 0,
                 .max = 16,
                 .name = "Multisamples",
                 .help = "Smooth out block edges",
                 .defaults = {0, 2, 4, 8, 16},
                 .defaults_length = 5,
                 .label_callback = config_label_msaa,
             });
    list_add(&config_settings,
             &(struct config_setting) {
                 .value = &settings_tmp.chat_shadow,
                 .type = CONFIG_TYPE_INT,
                 .min = 0,
                 .max = 1,
                 .help = "Dark chat background",
                 .name = "Chat shadow",
             });
    list_add(&config_settings,
             &(struct config_setting) {
                 .value = &settings_tmp.voxlap_models,
                 .type = CONFIG_TYPE_INT,
                 .min = 0,
                 .max = 1,
                 .help = "Render models like in voxlap",
                 .name = "Voxlap models",
             });
    list_add(&config_settings,
             &(struct config_setting) {
                 .value = &settings_tmp.hold_down_sights,
                 .type = CONFIG_TYPE_INT,
                 .min = 0,
                 .max = 1,
                 .help = "Only aim while pressing RMB",
                 .name = "Hold down sights",
             });
    list_add(&config_settings,
             &(struct config_setting) {
                 .value = &settings_tmp.greedy_meshing,
                 .type = CONFIG_TYPE_INT,
                 .min = 0,
                 .max = 1,
                 .help = "Join similar mesh faces",
                 .name = "Greedy meshing",
             });
    list_add(&config_settings,
             &(struct config_setting) {
                 .value = &settings_tmp.force_displaylist,
                 .type = CONFIG_TYPE_INT,
                 .min = 0,
                 .max = 1,
                 .help = "Enable this on buggy drivers",
                 .name = "Force Displaylist",
             });
    list_add(&config_settings,
             &(struct config_setting) {
                 .value = &settings_tmp.smooth_fog,
                 .type = CONFIG_TYPE_INT,
                 .min = 0,
                 .max = 1,
                 .help = "Enable this on buggy drivers",
                 .name = "Smooth fog",
             });
    list_add(&config_settings,
             &(struct config_setting) {
                 .value = &settings_tmp.ambient_occlusion,
                 .type = CONFIG_TYPE_INT,
                 .min = 0,
                 .max = 1,
                 .help = "(won't work with greedy mesh)",
                 .name = "Ambient occlusion",
             });
    list_add(&config_settings,
             &(struct config_setting) {
                 .value = &settings_tmp.show_fps,
                 .type = CONFIG_TYPE_INT,
                 .min = 0,
                 .max = 1,
                 .name = "Show fps",
                 .help = "Show current fps and ping ingame",
             });
    list_add(&config_settings,
             &(struct config_setting) {
                 .value = &settings_tmp.invert_y,
                 .type = CONFIG_TYPE_INT,
                 .min = 0,
                 .max = 1,
                 .name = "Invert y",
                 .help = "Invert vertical mouse movement",
             });
    list_add(&config_settings,
             &(struct config_setting) {
                 .value = &settings_tmp.show_news,
                 .type = CONFIG_TYPE_INT,
                 .min = 0,
                 .max = 1,
                 .name = "Show news",
                 .help = "Show news on server list",
             });
}
