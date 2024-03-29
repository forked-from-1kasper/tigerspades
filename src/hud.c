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
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <float.h>
#include <limits.h>
#include <ctype.h>

#include <lodepng/lodepng.h>

#include <BetterSpades/main.h>
#include <BetterSpades/file.h>
#include <BetterSpades/common.h>
#include <BetterSpades/list.h>
#include <BetterSpades/matrix.h>
#include <BetterSpades/texture.h>
#include <BetterSpades/hud.h>
#include <BetterSpades/config.h>
#include <BetterSpades/network.h>
#include <BetterSpades/rpc.h>
#include <BetterSpades/map.h>
#include <BetterSpades/camera.h>
#include <BetterSpades/cameracontroller.h>
#include <BetterSpades/ping.h>
#include <BetterSpades/chunk.h>
#include <BetterSpades/utils.h>
#include <BetterSpades/weapon.h>
#include <BetterSpades/tracer.h>
#include <BetterSpades/font.h>
#include <BetterSpades/unicode.h>
#include <BetterSpades/player.h>
#include <BetterSpades/particle.h>
#include <BetterSpades/opengl.h>

#include <parson.h>
#include <http.h>

HUD * hud_active;
WindowInstance * hud_window;

static pthread_mutex_t serverlist_lock;

static int is_inside_centered(double mx, double my, int x, int y, int w, int h) {
    return mx >= x - w / 2 && mx < x + w / 2 && my >= y - h / 2 && my < y + h / 2;
}

/*static int is_inside(double mx, double my, int x, int y, int w, int h) {
    return mx >= x && mx < x + w && my >= y && my < y + h;
}*/

void hud_init() {
    hud_serverlist.ctx = malloc(sizeof(mu_Context));
    hud_settings.ctx   = malloc(sizeof(mu_Context));
    hud_controls.ctx   = malloc(sizeof(mu_Context));

    pthread_mutex_init(&serverlist_lock, NULL);

    hud_change(&hud_serverlist);
}

static int mu_text_height(mu_Font font) {
    float scalex = fmax(1, round(settings.window_width / 800.0F));
    float scaley = fmax(1, round(settings.window_height / 600.0F));
    float scale  = settings.scale == 0 ? fmin(scalex, scaley) : settings.scale;

    return scale * 16.0F;
}

static int mu_text_width(mu_Font font, const char* text, int len) {
    if (len <= 0) {
        return ceil(font_length(mu_text_height(font) / 16.0F, (char*) text, UTF8));
    } else {
        char tmp[len + 1];
        memcpy(tmp, text, len);
        tmp[len] = 0;
        return ceil(font_length(mu_text_height(font) / 16.0F, tmp, UTF8));
    }
}

static void mu_text_color(mu_Context * ctx, int red, int green, int blue) {
    ctx->style->colors[MU_COLOR_TEXT] = mu_color(red, green, blue, 255);
}

static void mu_text_color_default(mu_Context * ctx) {
    ctx->style->colors[MU_COLOR_TEXT] = mu_color(230, 230, 230, 255);
}

void hud_change(HUD * new) {
    config_key_reset_togglestates();
    hud_active = new;

    if (hud_active->ctx) {
        mu_init(hud_active->ctx);
        hud_active->ctx->text_width                          = mu_text_width;
        hud_active->ctx->text_height                         = mu_text_height;
        hud_active->ctx->style->colors[MU_COLOR_BUTTONHOVER] = mu_color(95, 95, 70, 255);
        hud_active->ctx->style->colors[MU_COLOR_PANELBG]     = mu_color(10, 10, 10, 192);
        hud_active->ctx->style->colors[MU_COLOR_SCROLLTHUMB] = mu_color(128, 128, 128, 255);
    }

    if (hud_active->init)
        hud_active->init();
}

/*          HUD_INGAME START           */

static float hud_ingame_touch_x = 0.0F;
static float hud_ingame_touch_y = 0.0F;

int screen_current = SCREEN_NONE;
int show_exit = 0;
static void hud_ingame_init() {
    window_textinput(0);
    chat_input_mode = CHAT_NO_INPUT;
    show_exit = 0;
    window_mousemode(WINDOW_CURSOR_DISABLED);
}

typedef struct {
    unsigned char id;
    unsigned int score;
} PlayerTable;

static int playertable_sort(const void * a, const void * b) {
    PlayerTable * A = (PlayerTable *) a, * B = (PlayerTable *) b;
    return B->score - A->score;
}

static void hud_ingame_render3D() {
    glDepthRange(0.0F, 0.05F);

    matrix_identity(matrix_projection);
    matrix_perspective(matrix_projection, CAMERA_DEFAULT_FOV,
                       ((float) settings.window_width) / ((float) settings.window_height), 0.1F, 128.0F);
    matrix_identity(matrix_view);
    matrix_upload_p();

    if (!network_map_transfer) {
        if (camera.mode == CAMERAMODE_FPS && players[local_player.id].items_show) {
            static kv6 * const model_spade = &model[MODEL_SPADE];

            players[local_player.id].input.buttons &= MASKOFF(BUTTON_SECONDARY);

            matrix_identity(matrix_model);
            matrix_translate(matrix_model, -2.25F, -1.5F - (players[local_player.id].held_item == TOOL_SPADE) * 0.5F,
                             -6.0F);
            matrix_rotate(matrix_model, window_time() * 57.4F, 0.0F, 1.0F, 0.0F);
            matrix_translate(matrix_model, (model_spade->xpiv - model_spade->xsiz / 2) * 0.05F,
                             (model_spade->zpiv - model_spade->zsiz / 2) * 0.05F,
                             (model_spade->ypiv - model_spade->ysiz / 2) * 0.05F);

            if (players[local_player.id].held_item == TOOL_SPADE)
                matrix_scale(matrix_model, 1.5F, 1.5F, 1.5F);

            matrix_upload();
            kv6_render(model_spade, players[local_player.id].team);

            if (local_player.blocks > 0) {
                static kv6 * const model_block = &model[MODEL_BLOCK];

                matrix_identity(matrix_model);
                matrix_translate(matrix_model, -2.25F,
                                 -1.5F - (players[local_player.id].held_item == TOOL_BLOCK) * 0.5F, -6.0F);
                matrix_translate(matrix_model, 1.5F, 0.0F, 0.0F);
                matrix_rotate(matrix_model, window_time() * 57.4F, 0.0F, 1.0F, 0.0F);
                matrix_translate(matrix_model, (model_block->xpiv - model_block->xsiz / 2) * 0.05F,
                                 (model_block->zpiv - model_block->zsiz / 2) * 0.05F,
                                 (model_block->ypiv - model_block->ysiz / 2) * 0.05F);

                if (players[local_player.id].held_item == TOOL_BLOCK)
                    matrix_scale(matrix_model, 1.5F, 1.5F, 1.5F);

                model_block->red   = players[local_player.id].block.r / 255.0F;
                model_block->green = players[local_player.id].block.g / 255.0F;
                model_block->blue  = players[local_player.id].block.b / 255.0F;
                matrix_upload();

                kv6_render(model_block, players[local_player.id].team);
            }

            if (local_player.ammo + local_player.ammo_reserved > 0) {
                kv6 * gun = weapon_model(players[local_player.id].weapon);

                matrix_identity(matrix_model);
                matrix_translate(matrix_model, -2.25F, -1.5F - (players[local_player.id].held_item == TOOL_GUN) * 0.5F,
                                 -6.0F);
                matrix_translate(matrix_model, 3.0F, 0.0F, 0.0F);
                matrix_rotate(matrix_model, window_time() * 57.4F, 0.0F, 1.0F, 0.0F);
                matrix_translate(matrix_model, (gun->xpiv - gun->xsiz / 2) * 0.05F, (gun->zpiv - gun->zsiz / 2) * 0.05F,
                                 (gun->ypiv - gun->ysiz / 2) * 0.05F);

                if (players[local_player.id].held_item == TOOL_GUN)
                    matrix_scale(matrix_model, 1.5F, 1.5F, 1.5F);

                matrix_upload();
                kv6_render(gun, players[local_player.id].team);
            }

            if (local_player.grenades > 0) {
                static kv6 * const model_grenade = &model[MODEL_GRENADE];

                matrix_identity(matrix_model);
                matrix_translate(matrix_model, -2.25F,
                                 -1.5F - (players[local_player.id].held_item == TOOL_GRENADE) * 0.5F, -6.0F);
                matrix_translate(matrix_model, 4.5F, 0.0F, 0.0F);
                matrix_rotate(matrix_model, window_time() * 57.4F, 0.0F, 1.0F, 0.0F);
                matrix_translate(matrix_model, (model_grenade->xpiv - model_grenade->xsiz / 2) * 0.05F,
                                 (model_grenade->zpiv - model_grenade->zsiz / 2) * 0.05F,
                                 (model_grenade->ypiv - model_grenade->ysiz / 2) * 0.05F);

                if (players[local_player.id].held_item == TOOL_GRENADE)
                    matrix_scale(matrix_model, 1.5F, 1.5F, 1.5F);

                matrix_upload();
                kv6_render(model_grenade, players[local_player.id].team);
            }
        }

        if (screen_current == SCREEN_TEAM_SELECT) {
            matrix_identity(matrix_model);
            matrix_translate(matrix_model, -1.4F, -2.0F, -3.0F);
            matrix_rotate(matrix_model, -90.0F + 22.5F, 0.0F, 1.0F, 0.0F);
            matrix_upload();
            Player p_hud;
            memset(&p_hud, 0, sizeof(Player));
            p_hud.spade_use_timer    = FLT_MAX;
            p_hud.input.keys         = 0;
            p_hud.input.buttons      = 0;
            p_hud.held_item          = TOOL_SPADE;
            p_hud.physics.eye.x      = p_hud.pos.x = 0;
            p_hud.physics.eye.y      = p_hud.pos.y = 0;
            p_hud.physics.eye.z      = p_hud.pos.z = 0;
            p_hud.physics.velocity.x = 0.0F;
            p_hud.physics.velocity.y = 0.0F;
            p_hud.physics.velocity.z = 0.0F;
            p_hud.orientation.x = p_hud.orientation_smooth.x = 1.0F;
            p_hud.orientation.y = p_hud.orientation_smooth.y = 0.0F;
            p_hud.orientation.z = p_hud.orientation_smooth.z = 0.0F;
            p_hud.alive = 1;

            p_hud.team = TEAM_1;
            player_render(&p_hud, PLAYERS_MAX);
            matrix_identity(matrix_model);
            matrix_translate(matrix_model, 1.4F, -2.0F, -3.0F);
            matrix_rotate(matrix_model, -90.0F - 22.5F, 0.0F, 1.0F, 0.0F);
            matrix_upload();
            p_hud.team = TEAM_2;
            player_render(&p_hud, PLAYERS_MAX);
        }

        if (screen_current == SCREEN_GUN_SELECT) {
            static kv6 * const model_semi = &model[MODEL_SEMI];

            matrix_identity(matrix_model);
            matrix_translate(matrix_model, -1.5F, -1.25F, -3.25F);
            matrix_rotate(matrix_model, window_time() * 90.0F, 0.0F, 1.0F, 0.0F);
            matrix_translate(matrix_model, (model_semi->xpiv - model_semi->xsiz / 2.0F) * model_semi->scale,
                             (model_semi->zpiv - model_semi->zsiz / 2.0F) * model_semi->scale,
                             (model_semi->ypiv - model_semi->ysiz / 2.0F) * model_semi->scale);
            matrix_upload();
            kv6_render(model_semi, TEAM_SPECTATOR);

            static kv6 * const model_smg = &model[MODEL_SMG];

            matrix_identity(matrix_model);
            matrix_translate(matrix_model, 0.0F, -1.25F, -3.25F);
            matrix_rotate(matrix_model, window_time() * 90.0F, 0.0F, 1.0F, 0.0F);
            matrix_translate(matrix_model, (model_smg->xpiv - model_smg->xsiz / 2.0F) * model_smg->scale,
                             (model_smg->zpiv - model_smg->zsiz / 2.0F) * model_smg->scale,
                             (model_smg->ypiv - model_smg->ysiz / 2.0F) * model_smg->scale);
            matrix_upload();
            kv6_render(model_smg, TEAM_SPECTATOR);

            static kv6 * const model_shotgun = &model[MODEL_SHOTGUN];

            matrix_identity(matrix_model);
            matrix_translate(matrix_model, 1.5F, -1.25F, -3.25F);
            matrix_rotate(matrix_model, window_time() * 90.0F, 0.0F, 1.0F, 0.0F);
            matrix_translate(matrix_model, (model_shotgun->xpiv - model_shotgun->xsiz / 2.0F) * model_shotgun->scale,
                             (model_shotgun->zpiv - model_shotgun->zsiz / 2.0F) * model_shotgun->scale,
                             (model_shotgun->ypiv - model_shotgun->ysiz / 2.0F) * model_shotgun->scale);
            matrix_upload();
            kv6_render(model_shotgun, TEAM_SPECTATOR);
        }

        kv6 * rotating_model = NULL;
        int rotating_model_team = TEAM_SPECTATOR;
        if (gamestate.gamemode_type == GAMEMODE_CTF) {
            switch (players[local_player.id].team) {
                case TEAM_1: {
                    if (HASBIT(gamestate.gamemode.ctf.intels, TEAM_2_INTEL)
                    && (gamestate.gamemode.ctf.team_2_intel_location.held == local_player.id)) {
                        rotating_model      = &model[MODEL_INTEL];
                        rotating_model_team = TEAM_2;
                    }
                    break;
                }

                case TEAM_2: {
                    if (HASBIT(gamestate.gamemode.ctf.intels, TEAM_1_INTEL)
                    && (gamestate.gamemode.ctf.team_1_intel_location.held == local_player.id)) {
                        rotating_model      = &model[MODEL_INTEL];
                        rotating_model_team = TEAM_1;
                    }
                    break;
                }
            }
        }

        if (gamestate.gamemode_type == GAMEMODE_TC) {
            for (int k = 0; k < gamestate.gamemode.tc.territory_count; k++) {
                float n = norm3f(gamestate.gamemode.tc.territory[k].pos.x,
                                 63.0F - gamestate.gamemode.tc.territory[k].pos.z,
                                 gamestate.gamemode.tc.territory[k].pos.y,
                                 players[local_player.id].pos.x,
                                 players[local_player.id].pos.y,
                                 players[local_player.id].pos.z);

                if (n <= 400.0F) {
                    rotating_model      = &model[MODEL_TENT];
                    rotating_model_team = gamestate.gamemode.tc.territory[k].team;
                    break;
                }
            }
        }

        if (rotating_model) {
            matrix_identity(matrix_model);
            matrix_translate(matrix_model, 0.0F,
                             -(rotating_model->zsiz * 0.5F + rotating_model->zpiv) * rotating_model->scale, -10.0F);
            matrix_rotate(matrix_model, window_time() * 90.0F, 0.0F, 1.0F, 0.0F);
            matrix_upload();
            glViewport(-settings.window_width * 0.4F, settings.window_height * 0.2F, settings.window_width,
                       settings.window_height);
            kv6_render(rotating_model, rotating_model_team);
            glViewport(0.0F, 0.0F, settings.window_width, settings.window_height);
        }
    }
}

static void hud_ingame_keyboard(int key, int action, int mods, int internal);

static int hud_ingame_onscreencontrol(int index, char * str, int activate) {
    if (chat_input_mode == CHAT_NO_INPUT) {
        if (show_exit) {
            switch (index) {
                case 0:
                    if (str)
                        strcpy(str, "Yes");
                    if (activate == 0)
                        hud_ingame_keyboard(WINDOW_KEY_YES, WINDOW_RELEASE, 0, 0);
                    if (activate == 1)
                        hud_ingame_keyboard(WINDOW_KEY_YES, WINDOW_PRESS, 0, 0);
                    return 1;
                case 1:
                    if (str)
                        strcpy(str, "No");
                    if (activate == 0)
                        hud_ingame_keyboard(WINDOW_KEY_NO, WINDOW_RELEASE, 0, 0);
                    if (activate == 1)
                        hud_ingame_keyboard(WINDOW_KEY_NO, WINDOW_PRESS, 0, 0);
                    return 1;
            }
        } else {
            if (!network_connected || (network_connected && network_logged_in)) {
                switch (index) {
                    case 0:
                        if (str)
                            strcpy(str, "G-Chat");
                        if (activate == 0)
                            hud_ingame_keyboard(WINDOW_KEY_CHAT, WINDOW_RELEASE, 0, 0);
                        if (activate == 1)
                            hud_ingame_keyboard(WINDOW_KEY_CHAT, WINDOW_PRESS, 0, 0);
                        return 1;
                    case 1:
                        if (str)
                            strcpy(str, "T-Chat");
                        if (activate == 0)
                            hud_ingame_keyboard(WINDOW_KEY_YES, WINDOW_RELEASE, 0, 0);
                        if (activate == 1)
                            hud_ingame_keyboard(WINDOW_KEY_YES, WINDOW_PRESS, 0, 0);
                        return 1;
                    case 2:
                        if (str)
                            strcpy(str, "Score");
                        if (activate == 0)
                            keys(hud_window, WINDOW_KEY_TAB, WINDOW_RELEASE, 0);
                        if (activate == 1)
                            keys(hud_window, WINDOW_KEY_TAB, WINDOW_PRESS, 0);
                        return 1;
                    case 3:
                        if (str)
                            strcpy(str, "Team");
                        if (activate == 0)
                            hud_ingame_keyboard(WINDOW_KEY_CHANGETEAM, WINDOW_RELEASE, 0, 0);
                        if (activate == 1)
                            hud_ingame_keyboard(WINDOW_KEY_CHANGETEAM, WINDOW_PRESS, 0, 0);
                        return 1;
                    case 4:
                        if (str)
                            strcpy(str, "Weapon");
                        if (activate == 0)
                            hud_ingame_keyboard(WINDOW_KEY_CHANGEWEAPON, WINDOW_RELEASE, 0, 0);
                        if (activate == 1)
                            hud_ingame_keyboard(WINDOW_KEY_CHANGEWEAPON, WINDOW_PRESS, 0, 0);
                        return 1;
                    case 5:
                        if (str)
                            strcpy(str, "Network");
                        if (activate == 0)
                            keys(hud_window, WINDOW_KEY_NETWORKSTATS, WINDOW_RELEASE, 0);
                        if (activate == 1)
                            keys(hud_window, WINDOW_KEY_NETWORKSTATS, WINDOW_PRESS, 0);
                        return 1;
                    case 6:
                        if (str)
                            strcpy(str, "Tool");
                        if (activate == 1)
                            mouse_scroll(hud_window, 0, -1);
                        return 1;
                    case 64:
                        if (str)
                            strcpy(str, "LMB");
                        if (activate == 0)
                            mouse_click(hud_window, WINDOW_MOUSE_LMB, WINDOW_RELEASE, 0);
                        if (activate == 1)
                            mouse_click(hud_window, WINDOW_MOUSE_LMB, WINDOW_PRESS, 0);
                        return 1;
                    case 65:
                        if (str)
                            strcpy(str, "RMB");
                        if (activate == 0)
                            mouse_click(hud_window, WINDOW_MOUSE_RMB, WINDOW_RELEASE, 0);
                        if (activate == 1)
                            mouse_click(hud_window, WINDOW_MOUSE_RMB, WINDOW_PRESS, 0);
                        return 1;
                }
            }
        }
    } else {
        switch (index) {
            case 0:
                if (str)
                    strcpy(str, "Send");
                if (activate == 0)
                    hud_ingame_keyboard(WINDOW_KEY_ENTER, WINDOW_RELEASE, 0, 0);
                if (activate == 1)
                    hud_ingame_keyboard(WINDOW_KEY_ENTER, WINDOW_PRESS, 0, 0);
                return 1;
            case 1:
                if (str)
                    strcpy(str, "Close");
                if (activate == 0)
                    hud_ingame_keyboard(WINDOW_KEY_ESCAPE, WINDOW_RELEASE, 0, 0);
                if (activate == 1)
                    hud_ingame_keyboard(WINDOW_KEY_ESCAPE, WINDOW_PRESS, 0, 0);
                return 1;
        }
    }
    return 0;
}

static void hud_ingame_render(mu_Context * ctx, float scale) {
    // window_mousemode(camera.mode==CAMERAMODE_SELECTION?WINDOW_CURSOR_ENABLED:WINDOW_CURSOR_DISABLED);
    hud_active->render_localplayer = players[local_player.id].team != TEAM_SPECTATOR
        && (screen_current == SCREEN_NONE || camera.mode != CAMERAMODE_FPS);

    if (window_key_down(WINDOW_KEY_NETWORKSTATS)) {
        if (network_map_transfer)
            glColor3f(1.0F, 1.0F, 1.0F);
        else
            glColor3f(0.0F, 0.0F, 0.0F);

        glEnable(GL_DEPTH_TEST);
        glColorMask(0, 0, 0, 0);
        texture_draw_empty(8.0F * scale, 380.0F * scale, 160.0F * scale, 160.0F * scale);
        glColorMask(1, 1, 1, 1);
        glDepthFunc(GL_NOTEQUAL);
        texture_draw_empty(7.0F * scale, 381.0F * scale, 162.0F * scale, 162.0F * scale);
        glDepthFunc(GL_LEQUAL);
        glDisable(GL_DEPTH_TEST);
        font_select(FONT_SMALLFNT);
        char dbg_str[32];

        int max = 0;
        for (int k = 0; k < 40; k++) {
            max = max(max, network_stats[k].ingoing + network_stats[k].outgoing);
        }
        for (int k = 0; k < 40; k++) {
            float in_h = (float)(network_stats[39 - k].ingoing) / max * 160.0F;
            float out_h = (float)(network_stats[39 - k].ingoing + network_stats[39 - k].outgoing) / max * 160.0F;
            float ping_h = min(network_stats[39 - k].avg_ping / 25.0F, 160.0F);

            glColor3f(0.0F, 0.0F, 1.0F);
            texture_draw_empty(8.0F * scale + 4 * k * scale, (220.0F + out_h) * scale, 4.0F * scale,
                               out_h * scale);

            if (!k) {
                sprintf(dbg_str, "out: %i b/s", network_stats[1].outgoing);
                font_render(8.0F * scale + 80 * scale, 212.0F * scale, 1.0F * scale, dbg_str, ASCII);
            }

            glColor3f(0.0F, 1.0F, 0.0F);
            texture_draw_empty(8.0F * scale + 4 * k * scale, (220.0F + in_h) * scale, 4.0F * scale, in_h * scale);

            if (!k) {
                sprintf(dbg_str, "in: %i b/s", network_stats[1].ingoing);
                font_render(8.0F * scale, 212.0F * scale, 1.0F * scale, dbg_str, ASCII);
            }

            glColor3f(1.0F, 0.0F, 0.0F);
            texture_draw_empty(8.0F * scale + 4 * k * scale, (220.0F + ping_h) * scale, 4.0F * scale,
                               ping_h * scale);

            if (!k) {
                sprintf(dbg_str, "ping: %i ms", network_stats[1].avg_ping);
                font_render(8.0F * scale, 202.0F * scale, 1.0F * scale, dbg_str, ASCII);
            }
        }
        font_select(FONT_FIXEDSYS);
        glColor3f(1.0F, 1.0F, 1.0F);
    }

    if (network_map_transfer) {
        glColor3f(1.0F, 1.0F, 1.0F);
        texture_draw(
            texture(TEXTURE_SPLASH), (settings.window_width - settings.window_height * 4.0F / 3.0F * 0.7F) * 0.5F,
            settings.window_height - 40 * scale, settings.window_height * 4.0F / 3.0F * 0.7F, settings.window_height * 0.7F
        );

        float p = (compressed_chunk_data_estimate > 0) ? ((float) compressed_chunk_data_offset / (float) compressed_chunk_data_estimate) : 0.0F;
        p = clamp(0.0F, 1.0F, p);

        glColor3ub(68, 68, 68);
        texture_draw(texture(TEXTURE_WHITE), (settings.window_width - 440.0F * scale) / 2.0F + 440.0F * scale * p,
                     settings.window_height * 0.25F, 440.0F * scale * (1.0F - p), 20.0F * scale);
        glColor3ub(255, 255, 50);
        texture_draw(texture(TEXTURE_WHITE), (settings.window_width - 440.0F * scale) / 2.0F, settings.window_height * 0.25F,
                     440.0F * scale * p, 20.0F * scale);
        glColor3ub(69, 69, 69);

        char str[128];
        sprintf(str, "Receiving %i KiB / %i KiB", compressed_chunk_data_offset / 1024, compressed_chunk_data_estimate / 1024);
        font_centered(settings.window_width / 2.0F, settings.window_height * 0.25F - 20.0F * scale, 2.0F * scale, str, ASCII);

        font_select(FONT_SMALLFNT);
        glColor3f(1.0F, 1.0F, 0.0F);
        font_render(0.0F, 16.0F * scale, 1.0F * scale, "Created by ByteBit, visit https://github.com/xtreme8000/BetterSpades", ASCII);
        font_select(FONT_FIXEDSYS);
    } else {
        if (window_key_down(WINDOW_KEY_HIDEHUD))
            return;

        if (window_key_down(WINDOW_KEY_TRACE_CLEAN))
            trajectories_reset();

        if (screen_current == SCREEN_TEAM_SELECT) {
            glColor3f(1.0F, 0.0F, 0.0F);
            char join_str[48];

            sprintf(join_str, "Press 1 to join %s", gamestate.team_1.name);
            font_centered(settings.window_width / 4.0F, 61 * scale, 1.0F * scale, join_str, ASCII);

            font_centered(settings.window_width / 2.0F, 61 * scale, 1.0F * scale, "Press 2 to spectate", ASCII);

            sprintf(join_str, "Press 3 to join %s", gamestate.team_2.name);
            font_centered(settings.window_width / 4.0F * 3.0F, 61 * scale, 1.0F * scale, join_str, ASCII);
            glColor3f(1.0F, 1.0F, 1.0F);
        }

        if (screen_current == SCREEN_GUN_SELECT) {
            glColor3f(1.0F, 0.0F, 0.0F);
            font_centered(settings.window_width / 4.0F * 1.0F, 61 * scale, 1.0F * scale, "Press 1 to select", ASCII);
            font_centered(settings.window_width / 4.0F * 2.0F, 61 * scale, 1.0F * scale, "Press 2 to select", ASCII);
            font_centered(settings.window_width / 4.0F * 3.0F, 61 * scale, 1.0F * scale, "Press 3 to select", ASCII);
            glColor3f(1.0F, 1.0F, 1.0F);
        }

        if ((window_key_down(WINDOW_KEY_TAB) && chat_input_mode == CHAT_NO_INPUT) || camera.mode == CAMERAMODE_SELECTION) {
            if (network_connected && network_logged_in) {
                char ping_str[16];
                sprintf(ping_str, "Ping: %i ms", network_ping());
                font_select(FONT_SMALLFNT);
                glColor3f(1.0F, 0.0F, 0.0F);
                font_centered(settings.window_width / 2.0F, settings.window_height * 0.92F, 1.0F * scale, ping_str, ASCII);
                font_select(FONT_FIXEDSYS);
            }

            char score_str[8];
            glColorRGB3i(gamestate.team_1.color);

            switch (gamestate.gamemode_type) {
                case GAMEMODE_CTF:
                    sprintf(score_str, "%i/%i", gamestate.gamemode.ctf.team_1_score,
                            gamestate.gamemode.ctf.capture_limit);
                    break;
                case GAMEMODE_TC: {
                    int t = 0;
                    for (int k = 0; k < gamestate.gamemode.tc.territory_count; k++)
                        if (gamestate.gamemode.tc.territory[k].team == TEAM_1)
                            t++;
                    sprintf(score_str, "%i/%i", t, gamestate.gamemode.tc.territory_count);
                    break;
                }
            }

            font_centered(settings.window_width * 0.25F, settings.window_height - 15 * scale, 2.0F * scale, gamestate.team_1.name, UTF8);
            font_centered(settings.window_width * 0.25F, settings.window_height - 47 * scale, 3.0F * scale, score_str, ASCII);

            glColorRGB3i(gamestate.team_2.color);
            switch (gamestate.gamemode_type) {
                case GAMEMODE_CTF:
                    sprintf(score_str, "%i/%i", gamestate.gamemode.ctf.team_2_score,
                            gamestate.gamemode.ctf.capture_limit);
                    break;
                case GAMEMODE_TC: {
                    int t = 0;
                    for (int k = 0; k < gamestate.gamemode.tc.territory_count; k++)
                        if (gamestate.gamemode.tc.territory[k].team == TEAM_2)
                            t++;
                    sprintf(score_str, "%i/%i", t, gamestate.gamemode.tc.territory_count);
                    break;
                }
            }

            font_centered(settings.window_width * 0.75F, settings.window_height - 15 * scale, 2.0F * scale, gamestate.team_2.name, UTF8);
            font_centered(settings.window_width * 0.75F, settings.window_height - 47 * scale, 3.0F * scale, score_str, ASCII);

            PlayerTable pt[PLAYERS_MAX];
            int connected = 0;
            for (int k = 0; k < PLAYERS_MAX; k++) {
                if (players[k].connected) {
                    pt[connected].id = k;
                    pt[connected++].score = players[k].score;
                }
            }
            qsort(pt, connected, sizeof(PlayerTable), playertable_sort);

            int cntt[3] = {0};
            for (int k = 0; k < connected; k++) {
                int mul = 0;
                switch (players[pt[k].id].team) {
                    case TEAM_1: mul = 1; break;
                    case TEAM_2: mul = 3; break;
                    default:
                    case TEAM_SPECTATOR: mul = 2; break;
                }
                if (pt[k].id == local_player.id)
                    glColor3f(1.0F, 1.0F, 0.0F);
                else if (!players[pt[k].id].alive)
                    glColor3f(0.6F, 0.6F, 0.6F);
                else
                    glColor3f(1.0F, 1.0F, 1.0F);
                char id_str[16];
                sprintf(id_str, "#%i", pt[k].id);
                if (gamestate.gamemode_type == GAMEMODE_CTF &&
                      ((HASBIT(gamestate.gamemode.ctf.intels, TEAM_1_INTEL) &&
                        (gamestate.gamemode.ctf.team_1_intel_location.held == pt[k].id)) ||
                       (HASBIT(gamestate.gamemode.ctf.intels, TEAM_2_INTEL) &&
                        (gamestate.gamemode.ctf.team_2_intel_location.held == pt[k].id)))) {
                    texture_draw(texture(TEXTURE_INTEL),
                                 settings.window_width / 4.0F * mul
                                     - font_length(1.0F * scale, players[pt[k].id].name, UTF8) - 27.0F * scale,
                                 (427 - 16 * cntt[mul - 1]) * scale, 16.0F * scale, 16.0F * scale);
                }
                font_render(settings.window_width / 4.0F * mul - font_length(1.0F * scale, players[pt[k].id].name, UTF8),
                            (427 - 16 * cntt[mul - 1]) * scale, 1.0F * scale, players[pt[k].id].name, UTF8);
                font_render(settings.window_width / 4.0F * mul + 8.82F * scale, (427 - 16 * cntt[mul - 1]) * scale,
                            1.0F * scale, id_str, ASCII);
                if (mul != 2) {
                    sprintf(id_str, "%i", pt[k].score);
                    font_render(settings.window_width / 4.0F * mul + 44.1F * scale,
                                (427 - 16 * cntt[mul - 1]) * scale, 1.0F * scale, id_str, ASCII);
                }
                cntt[mul - 1]++;
            }
        }

        int is_local = (camera.mode == CAMERAMODE_FPS) || (cameracontroller_bodyview_player == local_player.id);
        int local_id = (camera.mode == CAMERAMODE_FPS) ? local_player.id : cameracontroller_bodyview_player;

        if (camera.mode == CAMERAMODE_BODYVIEW
           || (camera.mode == CAMERAMODE_SPECTATOR && cameracontroller_bodyview_mode)) {
            if (cameracontroller_bodyview_player != local_player.id) {
                font_select(FONT_SMALLFNT);
                switch (players[cameracontroller_bodyview_player].team) {
                    case TEAM_1: glColorRGB3i(gamestate.team_1.color); break;
                    case TEAM_2: glColorRGB3i(gamestate.team_2.color); break;
                }
                font_centered(settings.window_width / 2.0F, settings.window_height * 0.25F, 1.0F * scale,
                              players[cameracontroller_bodyview_player].name, UTF8);
            }
            font_select(FONT_FIXEDSYS);
            glColor3f(1.0F, 1.0F, 0.0F);
            font_centered(settings.window_width / 2.0F, settings.window_height, 1.0F * scale,
                          "Click to switch players", ASCII);
            if (window_time() - local_player.death_time <= local_player.respawn_time) {
                glColor3f(1.0F, 0.0F, 0.0F);
                int cnt = local_player.respawn_time - (int)(window_time() - local_player.death_time);
                char coin[16];
                sprintf(coin, "Respawn in %i s", cnt);
                font_centered(settings.window_width / 2.0F,
                              48.0F * scale * (cameracontroller_bodyview_mode ? 2.0F : 1.0F), 3.0F * scale, coin, ASCII);
                if (local_player.respawn_cnt_last != cnt) {
                    if (cnt < 4)
                        sound_create(SOUND_LOCAL, sound(cnt == 1 ? SOUND_BEEP1 : SOUND_BEEP2), 0.0F, 0.0F, 0.0F);

                    local_player.respawn_cnt_last = cnt;
                }
            }
            glColor3f(1.0F, 1.0F, 1.0F);
        }

        if (camera.mode == CAMERAMODE_FPS
           || ((camera.mode == CAMERAMODE_BODYVIEW || camera.mode == CAMERAMODE_SPECTATOR)
               && cameracontroller_bodyview_mode)) {
            glColor3f(1.0F, 1.0F, 1.0F);

            if (players[local_id].held_item == TOOL_GUN &&
                HASBIT(players[local_id].input.buttons, BUTTON_SECONDARY) &&
                players[local_id].alive) {
                Texture * zoom;
                switch (players[local_id].weapon) {
                    case WEAPON_RIFLE:   zoom = texture(TEXTURE_ZOOM_SEMI);    break;
                    case WEAPON_SMG:     zoom = texture(TEXTURE_ZOOM_SMG);     break;
                    case WEAPON_SHOTGUN: zoom = texture(TEXTURE_ZOOM_SHOTGUN); break;
                }

                float last_shot = is_local ? weapon_last_shot : players[local_id].gun_shoot_timer;
                float zoom_factor = fmax(0.25F * (1.0F - ((window_time() - last_shot) / weapon_delay(players[local_id].weapon))) + 1.0F, 1.0F);
                float aspect_ratio = texture_width(zoom) / texture_height(zoom);

                texture_draw(zoom, (settings.window_width - settings.window_height * aspect_ratio * zoom_factor) / 2.0F,
                             settings.window_height * (zoom_factor * 0.5F + 0.5F),
                             settings.window_height * aspect_ratio * zoom_factor, settings.window_height * zoom_factor);
                texture_draw_sector(zoom, 0, settings.window_height * (zoom_factor * 0.5F + 0.5F),
                                    (settings.window_width - settings.window_height * aspect_ratio * zoom_factor)
                                        / 2.0F,
                                    settings.window_height * zoom_factor, 0.0F, 0.0F, 1.0F / texture_width(zoom), 1.0F);
                texture_draw_sector(
                    zoom, (settings.window_width + settings.window_height * aspect_ratio * zoom_factor) / 2.0F,
                    settings.window_height * (zoom_factor * 0.5F + 0.5F),
                    (settings.window_width - settings.window_height * aspect_ratio * zoom_factor) / 2.0F,
                    settings.window_height * zoom_factor, (texture_width(zoom) - 1) / texture_width(zoom), 0.0F,
                    1.0F / texture_width(zoom), 1.0F
                );
            } else {
                texture_draw(
                    texture(HASBIT(players[local_player.id].input.buttons, BUTTON_PRIMARY) ? TEXTURE_CROSSHAIR2 : TEXTURE_CROSSHAIR1),
                    (settings.window_width - 32) / 2.0F, (settings.window_height + 32) / 2.0F, 32, 32
                );
            }

            if (window_time() - local_player.last_damage_timer <= 0.5F && is_local) {
                float ang = atan2(players[local_player.id].orientation.z, players[local_player.id].orientation.x)
                          - atan2(camera.pos.z - local_player.last_damage.z, camera.pos.x - local_player.last_damage.x) + PI;
                texture_draw_rotated(texture(TEXTURE_INDICATOR), settings.window_width / 2.0F, settings.window_height / 2.0F, 200, 200, ang);
            }

            int health = is_local ? (players[local_id].alive ? local_player.health : 0) : (players[local_id].alive ? 100 : 0);

            if (health <= 30)
                glColor3f(1, 0, 0);
            else
                glColor3f(1, 1, 1);

            char hp[4]; sprintf(hp, "%i", health);
            font_render(8.0F * scale, 32.0F * scale, 2.0F * scale, hp, ASCII);

            char item_mini_str[32]; int off = 0;
            glColor3f(1.0F, 1.0F, 1.0F);

            switch (players[local_id].held_item) {
                default:
                case TOOL_BLOCK: off = 64 * scale;
                case TOOL_SPADE:
                    sprintf(item_mini_str, "%i", is_local ? local_player.blocks : 50);
                    break;
                case TOOL_GRENADE:
                    sprintf(item_mini_str, "%i", is_local ? local_player.grenades : 3);
                    break;
                case TOOL_GUN: {
                    int ammo = is_local ? local_player.ammo : players[local_id].ammo;
                    int ammo_reserve = is_local ? local_player.ammo_reserved : players[local_id].ammo_reserved;
                    sprintf(item_mini_str, "%02i/%02i", ammo, ammo_reserve);

                    if (ammo == 0) glColor3f(1.0F, 0.0F, 0.0F);
                    break;
                }
            }

            font_render(settings.window_width - font_length(2.0F * scale, item_mini_str, ASCII) - 8.0F * scale - off, 32.0F * scale, 2.0F * scale, item_mini_str, ASCII);

            glColor3f(1.0F, 1.0F, 1.0F);
            if (players[local_id].held_item == TOOL_BLOCK) {
                if (local_player.color[X] >= 0 && local_player.color[Y] >= 0) {
                    unsigned char g = (((int) (window_time() * 4)) & 1) * 0xFF; glColor3ub(g, g, g);
                    texture_draw_empty(
                        settings.window_width + (local_player.color[X] * 8 - 65) * scale,
                        (65 - local_player.color[Y] * 8) * scale, 8 * scale, 8 * scale
                    );
                }

                glColor3f(1.0F, 1.0F, 1.0F);

                texture_draw(texture_color_selection, settings.window_width - 64 * scale, 64 * scale, 64 * scale, 64 * scale);
            }
        }

        if (camera.mode != CAMERAMODE_SELECTION) {
            int chat_y = 8 * 18.0F * scale;

            font_select(FONT_SMALLFNT);

            if (settings.chat_shadow) {
                float chat_width = 0;
                int chat_height = 0;
                for (int k = 0; k < 6; k++) {
                    if ((window_time() - chat_timer[0][k + 1] < 10.0F || chat_input_mode != CHAT_NO_INPUT)
                       && strlen(chat[0][k + 1]) > 0) {
                        chat_width = fmaxf(font_length(1.0F * scale, chat[0][k + 1], UTF8), chat_width);
                        chat_height = k + 1;
                    }
                }

                if (chat_input_mode != CHAT_NO_INPUT) {
                    chat_height += 2;
                    chat_width = fmaxf(
                        font_length(1.0F * scale, chat[0][0], UTF8),
                        fmaxf(settings.window_width / 2.0F, chat_width)
                    );
                }

                if (chat_height > 0) {
                    glColor4f(0, 0, 0, 0.5F);
                    glEnable(GL_BLEND);
                    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                    if (chat_input_mode == CHAT_NO_INPUT) {
                        texture_draw_empty(3.0F * scale, chat_y,
                                           chat_width + 16.0F * scale, 18.0F * scale * chat_height);
                    } else {
                        texture_draw_empty(3.0F * scale, chat_y + 36.0F * scale,
                                           chat_width + 16.0F * scale, 18.0F * scale * chat_height);
                    }
                    glDisable(GL_BLEND);
                }
            }

            glColor3f(1.0F, 1.0F, 1.0F);

            if (chat_input_mode != CHAT_NO_INPUT) {
                switch (chat_input_mode) {
                    case CHAT_ALL_INPUT:
                        font_render(11.0F * scale, chat_y + 36.0F * scale, 1.0F * scale,
                                    "Global:", ASCII);
                        break;
                    case CHAT_TEAM_INPUT:
                        font_render(11.0F * scale, chat_y + 36.0F * scale, 1.0F * scale,
                                    "Team:", ASCII);
                        break;
                }
                int l = strlen(chat[0][0]);
                chat[0][0][l] = '_';
                chat[0][0][l + 1] = 0;

                font_render(11.0F * scale, chat_y + 18.0F * scale, 1.0F * scale, chat[0][0], UTF8);
                chat[0][0][l] = 0;
            }

            float top = settings.window_height - 11.0F * scale;

            if (window_key_down(WINDOW_KEY_DEBUG)) {
                char buff[64]; FontType old = font_select(FONT_SMALLFNT);
                glColor3f(1.0F, 1.0F, 1.0F);

                sprintf(buff, "TigerSpades %s (%s)", BETTERSPADES_VERSION, GIT_COMMIT_HASH);
                font_render(11.0F * scale, top, scale, buff, ASCII); top -= 16.0F * scale;

                sprintf(buff, "%i ms, %i fps", network_ping(), (int) fps);
                font_render(11.0F * scale, top, scale, buff, ASCII); top -= 16.0F * scale;

                Vector3f r = camera.mode == CAMERAMODE_FPS ? players[local_player.id].pos
                                                           : camera.pos;

                sprintf(buff, "XYZ: %.02f / %.02f / %.02f", r.x, r.y, r.z);
                font_render(11.0F * scale, top, scale, buff, ASCII); top -= 16.0F * scale;

                Vector3f o = camera.mode == CAMERAMODE_FPS ? players[local_player.id].orientation
                                                           : camera_orientation();

                sprintf(buff, "Facing: %.04f / %.04f / %.04f", o.x, o.y, o.z);
                font_render(11.0F * scale, top, scale, buff, ASCII); top -= 16.0F * scale;

                sprintf(
                    buff, "RGB: #%02X%02X%02X",
                    players[local_player.id].block.r,
                    players[local_player.id].block.g,
                    players[local_player.id].block.b
                );
                font_render(11.0F * scale, top, scale, buff, ASCII); top -= 16.0F * scale;

                font_select(old);
            }

            for (int k = 0; k < 6; k++) {
                if (window_time() - chat_timer[0][k + 1] < 10.0F || chat_input_mode != CHAT_NO_INPUT) {
                    glColor3ub(chat_color[0][k + 1].r, chat_color[0][k + 1].g, chat_color[0][k + 1].b);

                    font_render(11.0F * scale, chat_y - 18.0F * scale * k, 1.0F * scale, chat[0][k + 1], UTF8);
                }

                if (window_time() - chat_timer[1][k + 1] < 10.0F) {
                    glColor3ub(chat_color[1][k + 1].r, chat_color[1][k + 1].g, chat_color[1][k + 1].b);

                    font_render(11.0F * scale, top - 18.0F * scale * k, 1.0F * scale, chat[1][k + 1], UTF8);
                }
            }

            font_select(FONT_FIXEDSYS);
            glColor3f(1.0F, 1.0F, 1.0F);
        } else {
            glColor3f(1.0F, 1.0F, 1.0F);
            texture_draw(texture(TEXTURE_SPLASH), (settings.window_width - 240 * scale) * 0.5F, settings.window_height - 1 * scale, 240 * scale, 180 * scale);
            glColor3f(1.0F, 1.0F, 0.0F);
            font_centered(settings.window_width / 2.0F, settings.window_height - 180 * scale, 2 * scale, "CONTROLS", ASCII);
            char help_str[2][12][16] = {{"Movement", "Weapons", "Reload", "Jump", "Crouch", "Sneak",
                                         "Sprint", "Map", "Change Team", "Change Weapon", "Global Chat",
                                         "Team Chat"},
                                        {"W S A D", "1-4 / Wheel", "R", "Space", "CTRL", "V",
                                         "SHIFT", "M", ",", ".", "T", "Y"}};

            for (int k = 0; k < 12; k++) {
                font_render(settings.window_width / 2.0F - font_length(1.0F * scale, help_str[0][k], ASCII),
                            settings.window_height - (180 + 32 + 16 * k) * scale, 1.0F * scale, help_str[0][k], ASCII);
                font_render(settings.window_width / 2.0F + font_length(1.0F * scale, " ", ASCII),
                            settings.window_height - (180 + 32 + 16 * k) * scale, 1.0F * scale, help_str[1][k], ASCII);
            }

            glColor3f(1.0F, 1.0F, 1.0F);
        }

        if (gamestate.gamemode_type == GAMEMODE_TC && gamestate.progressbar.tent < gamestate.gamemode.tc.territory_count
           && gamestate.gamemode.tc.territory[gamestate.progressbar.tent].team
               != gamestate.progressbar.team_capturing) {
            float p = clamp(0.0F, 1.0F, gamestate.progressbar.progress + 0.05F * gamestate.progressbar.rate * (window_time() - gamestate.progressbar.update));

            float l = norm3f(gamestate.gamemode.tc.territory[gamestate.progressbar.tent].pos.x,
                             63.0F - gamestate.gamemode.tc.territory[gamestate.progressbar.tent].pos.z,
                             gamestate.gamemode.tc.territory[gamestate.progressbar.tent].pos.y,
                             players[local_player.id].pos.x,
                             players[local_player.id].pos.y,
                             players[local_player.id].pos.z);

            if (p < 1.0F && l < 20.0F * 20.0F) {
                switch (gamestate.gamemode.tc.territory[gamestate.progressbar.tent].team) {
                    case TEAM_1: glColorRGB3i(gamestate.team_1.color); break;
                    case TEAM_2: glColorRGB3i(gamestate.team_2.color); break;
                    default: glColor3ub(0, 0, 0);
                }
                texture_draw(texture(TEXTURE_WHITE), (settings.window_width - 440.0F * scale) / 2.0F + 440.0F * scale * p,
                             settings.window_height * 0.25F, 440.0F * scale * (1.0F - p), 20.0F * scale);

                switch (gamestate.progressbar.team_capturing) {
                    case TEAM_1: glColorRGB3i(gamestate.team_1.color); break;
                    case TEAM_2: glColorRGB3i(gamestate.team_2.color); break;
                    default: glColor3ub(0, 0, 0);
                }
                texture_draw(texture(TEXTURE_WHITE), (settings.window_width - 440.0F * scale) / 2.0F,
                             settings.window_height * 0.25F, 440.0F * scale * p, 20.0F * scale);
            }
        }

        // draw the minimap
        if (camera.mode != CAMERAMODE_SELECTION) {
            glColor3f(1.0F, 1.0F, 1.0F);
            // large
            if (window_key_down(WINDOW_KEY_MAP)) {
                float minimap_x = (settings.window_width  - (map_size_x + 1) * scale) / 2.0F;
                float minimap_y = (settings.window_height - (map_size_z + 1) * scale) / 2.0F + (map_size_z + 1) * scale;

                texture_draw(texture_minimap, minimap_x, minimap_y, 512 * scale, 512 * scale);

                font_select(FONT_SMALLFNT);
                char c[2] = {0};
                for (int k = 0; k < 8; k++) {
                    c[0] = 'A' + k; font_centered(minimap_x + (64 * k + 32) * scale, minimap_y + 16.0F * scale, 1.0F * scale, c, ASCII);
                    c[0] = '1' + k; font_centered(minimap_x - 8 * scale, minimap_y - (64 * k + 32 - 4) * scale, 1.0F * scale, c, ASCII);
                }
                font_select(FONT_FIXEDSYS);

                tracer_minimap(1, scale, minimap_x, minimap_y);

                if (gamestate.gamemode_type == GAMEMODE_CTF) {
                    if (!HASBIT(gamestate.gamemode.ctf.intels, TEAM_1_INTEL)) {
                        glColorRGB3i(gamestate.team_1.color);
                        texture_draw_rotated(
                            texture(TEXTURE_INTEL), minimap_x + gamestate.gamemode.ctf.team_1_intel_location.dropped.x * scale,
                            minimap_y - gamestate.gamemode.ctf.team_1_intel_location.dropped.y * scale, 16 * scale,
                            16 * scale, 0.0F
                        );
                    }

                    if (map_object_visible(gamestate.gamemode.ctf.team_1_base.x, 0.0F, gamestate.gamemode.ctf.team_1_base.y)) {
                        glColorRGB3ib(gamestate.team_1.color, 0.94F);
                        texture_draw_rotated(
                            texture(TEXTURE_MEDICAL), minimap_x + gamestate.gamemode.ctf.team_1_base.x * scale,
                            minimap_y - gamestate.gamemode.ctf.team_1_base.y * scale, 16 * scale, 16 * scale, 0.0F
                        );
                    }

                    if (!HASBIT(gamestate.gamemode.ctf.intels, TEAM_2_INTEL)) {
                        glColorRGB3i(gamestate.team_2.color);
                        texture_draw_rotated(
                            texture(TEXTURE_INTEL), minimap_x + gamestate.gamemode.ctf.team_2_intel_location.dropped.x * scale,
                            minimap_y - gamestate.gamemode.ctf.team_2_intel_location.dropped.y * scale, 16 * scale, 16 * scale, 0.0F
                        );
                    }

                    if (map_object_visible(gamestate.gamemode.ctf.team_2_base.x, 0.0F, gamestate.gamemode.ctf.team_2_base.y)) {
                        glColorRGB3ib(gamestate.team_2.color, 0.94F);
                        texture_draw_rotated(
                            texture(TEXTURE_MEDICAL), minimap_x + gamestate.gamemode.ctf.team_2_base.x * scale,
                            minimap_y - gamestate.gamemode.ctf.team_2_base.y * scale, 16 * scale, 16 * scale, 0.0F
                        );
                    }
                }

                if (gamestate.gamemode_type == GAMEMODE_TC) {
                    for (int k = 0; k < gamestate.gamemode.tc.territory_count; k++) {
                        switch (gamestate.gamemode.tc.territory[k].team) {
                            case TEAM_1: glColorRGB3ib(gamestate.team_1.color, 0.94F); break;
                            case TEAM_2: glColorRGB3ib(gamestate.team_2.color, 0.94F); break;
                            default: case TEAM_SPECTATOR: glColor3ub(0, 0, 0);
                        }

                        texture_draw_rotated(
                            texture(TEXTURE_COMMAND), minimap_x + gamestate.gamemode.tc.territory[k].pos.x * scale,
                            minimap_y - gamestate.gamemode.tc.territory[k].pos.y * scale, 12 * scale, 12 * scale, 0.0F
                        );
                    }
                }

                for (int k = 0; k < PLAYERS_MAX; k++) {
                #if HACKS_ENABLED && HACK_MAPHACK
                    if (players[k].connected && players[k].alive && k != local_player.id)
                #else
                    if (players[k].connected && players[k].alive && k != local_player.id && players[k].team != TEAM_SPECTATOR
                    && (players[k].team == players[local_player.id].team || camera.mode == CAMERAMODE_SPECTATOR))
                #endif
                    {
                        switch (players[k].team) {
                            case TEAM_1: glColorRGB3i(gamestate.team_1.color); break;
                            case TEAM_2: glColorRGB3i(gamestate.team_2.color); break;
                        }
                        float ang = -atan2(players[k].orientation.z, players[k].orientation.x) - HALFPI;
                        texture_draw_rotated(texture(TEXTURE_PLAYER), minimap_x + players[k].pos.x * scale,
                                             minimap_y - players[k].pos.z * scale, 16 * scale, 16 * scale, ang);
                    }
                }

                glColor3f(0.0F, 1.0F, 1.0F);
                texture_draw_rotated(texture(TEXTURE_PLAYER), minimap_x + camera.pos.x * scale, minimap_y - camera.pos.z * scale,
                                     16 * scale, 16 * scale, camera.rot.x + PI);
                glColor3f(1.0F, 1.0F, 1.0F);
            } else if (settings.show_minimap) {
                // minimized, top right
                float view_x = camera.pos.x - 64.0F; // min(max(camera.pos.x-64.0F,0.0F),map_size_x+1-128.0F);
                float view_z = camera.pos.z - 64.0F; // min(max(camera.pos.z-64.0F,0.0F),map_size_z+1-128.0F);

                switch (players[local_player.id].team) {
                    case TEAM_1: glColorRGB3i(gamestate.team_1.color); break;
                    case TEAM_2: glColorRGB3i(gamestate.team_2.color); break;
                    case TEAM_SPECTATOR: default: glColor3f(0.0F, 0.0F, 0.0F); // same as chat
                }

                float minimap_x = settings.window_width - 143 * scale;
                float minimap_y = settings.window_height - 15 * scale;

                char sector_str[3] = {(int)(camera.pos.x / 64.0F) + 'A', (int)(camera.pos.z / 64.0F) + '1', 0};
                font_centered(minimap_x + 64 * scale, minimap_y - 129 * scale, 2.0F * scale, sector_str, ASCII);

                glColor3ub(0, 0, 0);
                texture_draw_empty(minimap_x - 1 * scale, minimap_y + 1 * scale, 130 * scale, 130 * scale);
                glColor3f(1.0F, 1.0F, 1.0F);

                texture_draw_sector(texture_minimap, minimap_x, minimap_y, 128 * scale,
                                    128 * scale, (camera.pos.x - 64.0F) / 512.0F, (camera.pos.z - 64.0F) / 512.0F, 0.25F,
                                    0.25F);

                tracer_minimap(0, scale, view_x, view_z);

                if (gamestate.gamemode_type == GAMEMODE_CTF) {
                    float tent1_x = clamp(view_x, view_x + 128.0F, gamestate.gamemode.ctf.team_1_base.x) - view_x;
                    float tent1_y = clamp(view_z, view_z + 128.0F, gamestate.gamemode.ctf.team_2_base.y) - view_z;

                    float tent2_x = clamp(view_x, view_x + 128.0F, gamestate.gamemode.ctf.team_2_base.x) - view_x;
                    float tent2_y = clamp(view_z, view_z + 128.0F, gamestate.gamemode.ctf.team_2_base.y) - view_z;

                    if (map_object_visible(gamestate.gamemode.ctf.team_1_base.x, 0.0F,
                                           gamestate.gamemode.ctf.team_1_base.y)) {
                        glColorRGB3ib(gamestate.team_1.color, 0.94F);
                        texture_draw_rotated(texture(TEXTURE_MEDICAL), minimap_x + tent1_x * scale,
                                             minimap_y - tent1_y * scale, 16 * scale, 16 * scale, 0.0F);
                    }

                    if (!HASBIT(gamestate.gamemode.ctf.intels, TEAM_1_INTEL)) {
                        float intel_x = clamp(view_x, view_x + 128.0F, gamestate.gamemode.ctf.team_1_intel_location.dropped.x) - view_x;
                        float intel_y = clamp(view_z, view_z + 128.0F, gamestate.gamemode.ctf.team_1_intel_location.dropped.y) - view_z;

                        glColorRGB3i(gamestate.team_1.color);
                        texture_draw_rotated(texture(TEXTURE_INTEL), minimap_x + intel_x * scale,
                                             minimap_y - intel_y * scale, 16 * scale, 16 * scale, 0.0F);
                    }

                    if (map_object_visible(gamestate.gamemode.ctf.team_2_base.x, 0.0F,
                                          gamestate.gamemode.ctf.team_2_base.y)) {
                        glColorRGB3ib(gamestate.team_2.color, 0.94F);
                        texture_draw_rotated(texture(TEXTURE_MEDICAL), minimap_x + tent2_x * scale,
                                             minimap_y - tent2_y * scale, 16 * scale, 16 * scale, 0.0F);
                    }

                    if (!HASBIT(gamestate.gamemode.ctf.intels, TEAM_2_INTEL)) {
                        float intel_x = clamp(view_x, view_x + 128.0F, gamestate.gamemode.ctf.team_2_intel_location.dropped.x) - view_x;
                        float intel_y = clamp(view_z, view_z + 128.0F, gamestate.gamemode.ctf.team_2_intel_location.dropped.y) - view_z;

                        glColorRGB3i(gamestate.team_2.color);
                        texture_draw_rotated(texture(TEXTURE_INTEL), minimap_x + intel_x * scale,
                                             minimap_y - intel_y * scale, 16 * scale, 16 * scale, 0.0F);
                    }
                }

                if (gamestate.gamemode_type == GAMEMODE_TC) {
                    for (int k = 0; k < gamestate.gamemode.tc.territory_count; k++) {
                        switch (gamestate.gamemode.tc.territory[k].team) {
                            case TEAM_1: glColorRGB3ib(gamestate.team_1.color, 0.94F); break;
                            case TEAM_2: glColorRGB3ib(gamestate.team_2.color, 0.94F); break;
                            default: case TEAM_SPECTATOR: glColor3ub(0, 0, 0);
                        }

                        float t_x = clamp(view_x, view_x + 128.0F, gamestate.gamemode.tc.territory[k].pos.x) - view_x;
                        float t_y = clamp(view_z, view_z + 128.0F, gamestate.gamemode.tc.territory[k].pos.y) - view_z;
                        texture_draw_rotated(texture(TEXTURE_COMMAND), minimap_x + t_x * scale,
                                             minimap_y - t_y * scale, 12 * scale, 12 * scale, 0.0F);
                    }
                }

                for (int k = 0; k < PLAYERS_MAX; k++) {
                #if HACKS_ENABLED && HACK_MAPHACK
                    if (players[k].connected && players[k].alive)
                #else
                    if (players[k].connected && players[k].alive
                       && (players[k].team == players[local_player.id].team
                           || (camera.mode == CAMERAMODE_SPECTATOR
                               && (k == local_player.id || players[k].team != TEAM_SPECTATOR))))
                #endif
                    {
                        if (k == local_player.id) {
                            glColor3ub(0, 255, 255);
                        } else {
                            switch (players[k].team) {
                                case TEAM_1: glColorRGB3i(gamestate.team_1.color); break;
                                case TEAM_2: glColorRGB3i(gamestate.team_2.color); break;
                            }
                        }

                        float player_x = ((k == local_player.id) ? camera.pos.x : players[k].pos.x) - view_x;
                        float player_y = ((k == local_player.id) ? camera.pos.z : players[k].pos.z) - view_z;
                        if (player_x > 0.0F && player_x < 128.0F && player_y > 0.0F && player_y < 128.0F) {
                            float ang = (k == local_player.id) ?
                                camera.rot.x + PI :
                                -atan2(players[k].orientation.z, players[k].orientation.x) - HALFPI;
                            texture_draw_rotated(texture(TEXTURE_PLAYER),
                                                 minimap_x + player_x * scale,
                                                 minimap_y - player_y * scale, 16 * scale, 16 * scale, ang);
                        }
                    }
                }
            }
        }

        if (player_intersection_type >= 0
           && (players[local_player.id].team == TEAM_SPECTATOR ||
               players[player_intersection_player].team == players[local_player.id].team)) {
            font_select(FONT_SMALLFNT);

            char * th[4] = {"torso", "head", "arms", "legs"}; char str[32];
            switch (players[player_intersection_player].team) {
                case TEAM_1: glColorRGB3i(gamestate.team_1.color); break;
                case TEAM_2: glColorRGB3i(gamestate.team_2.color); break;
                default: glColor3f(1.0F, 1.0F, 1.0F);
            }
            sprintf(str, "%s (%s)", players[player_intersection_player].name, th[player_intersection_type]);
            font_centered(settings.window_width / 2.0F, settings.window_height * 0.2F, 1.0F * scale, str, UTF8);
            font_select(FONT_FIXEDSYS);
        }

        if (show_exit) {
            glColor3f(1.0F, 0.0F, 0.0F);
            font_render(settings.window_width / 2.0F - font_length(3.0F * scale, "EXIT GAME? Y/N", ASCII) / 2.0F,
                        settings.window_height / 2.0F + 48.0F * scale, 3.0F * scale, "EXIT GAME? Y/N", ASCII);

            char play_time[128];
            sprintf(play_time, "Playing for %i min. %02i sec.", (int) window_time() / 60, (int) window_time() % 60);
            font_render(settings.window_width / 2.0F - font_length(2.0F * scale, play_time, ASCII) / 2.0F,
                        settings.window_height, 2.0F * scale, play_time, ASCII);
        }

        if (window_time() - chat_popup_timer < chat_popup_duration) {
            glColor3ub(chat_popup_color.r, chat_popup_color.g, chat_popup_color.b);
            font_render((settings.window_width - font_length(3.0F * scale, chat_popup, UTF8)) / 2.0F,
                        settings.window_height * 0.65F, 3.0F * scale, chat_popup, UTF8);
        }

        glColor3f(1.0F, 1.0F, 1.0F);
    }

#ifdef USE_TOUCH
    glColor3f(1.0F, 1.0F, 1.0F);
    if (camera.mode == CAMERAMODE_FPS || camera.mode == CAMERAMODE_SPECTATOR) {
        texture_draw_rotated(texture(TEXTURE_UI_JOYSTICK), settings.window_height * 0.3F, settings.window_height * 0.3F,
                             settings.window_height * 0.4F, settings.window_height * 0.4F, 0.0F);
        texture_draw_rotated(texture(TEXTURE_UI_KNOB), settings.window_height * 0.3F, settings.window_height * 0.3F,
                             settings.window_height * 0.075F, settings.window_height * 0.075F, 0.0F);
        texture_draw_rotated(texture(TEXTURE_UI_KNOB), hud_ingame_touch_x + settings.window_height * 0.3F,
                             hud_ingame_touch_y + settings.window_height * 0.3F, settings.window_height * 0.1F,
                             settings.window_height * 0.1F, 0.0F);
    }

    int k = 0;
    char str[128];
    while (hud_ingame_onscreencontrol(k, str, -1)) {
        texture_draw_rotated(texture(TEXTURE_UI_INPUT), settings.window_height * (0.2F + 0.175F * k),
                             settings.window_height * 0.96F, settings.window_height * 0.15F,
                             settings.window_height * 0.1F, 0.0F);
        font_centered(settings.window_height * (0.2F + 0.175F * k), settings.window_height * 0.98F,
                      settings.window_height * 0.04F, str, ASCII);
        k++;
    }
    if (hud_ingame_onscreencontrol(64, str, -1)) {
        texture_draw_rotated(texture(TEXTURE_UI_INPUT), settings.window_width - settings.window_height * 0.075F,
                             settings.window_height * 0.6F, settings.window_height * 0.15F,
                             settings.window_height * 0.1F, 0.0F);
        font_centered(settings.window_width - settings.window_height * 0.075F, settings.window_height * 0.62F,
                      settings.window_height * 0.04F, str, ASCII);
    }
    if (hud_ingame_onscreencontrol(65, str, -1)) {
        texture_draw_rotated(texture(TEXTURE_UI_INPUT), settings.window_width - settings.window_height * 0.075F,
                             settings.window_height * 0.45F, settings.window_height * 0.15F,
                             settings.window_height * 0.1F, 0.0F);
        font_centered(settings.window_width - settings.window_height * 0.075F, settings.window_height * 0.47F,
                      settings.window_height * 0.04F, str, ASCII);
    }
#endif
}

static void hud_ingame_scroll(double yoffset) {
    if (camera.mode == CAMERAMODE_FPS && yoffset != 0.0F) {
        int h = players[local_player.id].held_item;
        if (!players[local_player.id].items_show)
            local_player.last_tool = h;
        h += (yoffset < 0) ? 1 : -1;
        if (h < 0)
            h = 3;
        if (h == TOOL_BLOCK && local_player.blocks == 0)
            h += (yoffset < 0) ? 1 : -1;
        if (h == TOOL_GUN && local_player.ammo + local_player.ammo_reserved == 0)
            h += (yoffset < 0) ? 1 : -1;
        if (h == TOOL_GRENADE && local_player.grenades == 0)
            h += (yoffset < 0) ? 1 : -1;
        if (h > 3)
            h = 0;
        players[local_player.id].held_item = h;
        sound_create(SOUND_LOCAL, sound(SOUND_SWITCH), 0.0F, 0.0F, 0.0F);
        player_on_held_item_change(players + local_player.id);
    }
}

bool window_focused = true, window_hovered = true;

void hud_ingame_focus(bool focused) {
    window_focused = focused;
    if (!focused) window_mousemode(WINDOW_CURSOR_ENABLED);
}

void hud_ingame_hover(bool hovered) {
    window_hovered = hovered;
    if (!hovered) window_mousemode(WINDOW_CURSOR_ENABLED);
}

static double last_x, last_y;
static void hud_ingame_mouselocation(double x, double y) {
    if (window_get_mousemode() != WINDOW_CURSOR_DISABLED) {
        last_x = x;
        last_y = y;
        return;
    }

    float dx = x - last_x;
    float dy = y - last_y;
    last_x = x;
    last_y = y;

    float s = 1.0F;
    if (camera.mode == CAMERAMODE_FPS && players[local_player.id].held_item == TOOL_GUN &&
        HASBIT(players[local_player.id].input.buttons, BUTTON_SECONDARY)) {
        s = 0.5F;
    }

    if (settings.invert_y)
        dy *= -1.0F;

    camera.rot.x -= dx * settings.mouse_sensitivity / 5.0F * (float) MOUSE_SENSITIVITY * s;
    camera.rot.y += dy * settings.mouse_sensitivity / 5.0F * (float) MOUSE_SENSITIVITY * s;

    camera_overflow_adjust();
}

static void hud_ingame_mouseclick(double x, double y, int button, int action, int mods) {
    if (window_get_mousemode() == WINDOW_CURSOR_ENABLED) {
        if (window_focused && window_hovered && action == WINDOW_RELEASE && !show_exit)
            window_mousemode(WINDOW_CURSOR_DISABLED);

        return;
    }

    if (button == WINDOW_MOUSE_LMB) button_map.lmb = (action == WINDOW_PRESS);

    if (button == WINDOW_MOUSE_RMB) {
        if (action == WINDOW_PRESS && players[local_player.id].held_item == TOOL_GUN
            && !settings.hold_down_sights && !players[local_player.id].items_show) {
            players[local_player.id].input.buttons ^= MASKON(BUTTON_SECONDARY);
        }

        if (local_player.drag_active && action == WINDOW_RELEASE && players[local_player.id].held_item == TOOL_BLOCK) {
            int * pos = camera_terrain_pick(0);
            if (pos != NULL && pos[1] > 1 && norm3i(pos[X], pos[Y], pos[Z], camera.pos.x, camera.pos.y, camera.pos.z) < 25) {
                int amount = map_cube_line(local_player.drag.x, local_player.drag.z, 63 - local_player.drag.y,
                                           pos[0], pos[2], 63 - pos[1], NULL);
                if (amount <= local_player.blocks) {
                    PacketBlockLine contained;
                    contained.player_id = local_player.id;
                    contained.start     = htonv3i(local_player.drag);
                    contained.end.x     = pos[X];
                    contained.end.y     = pos[Z];
                    contained.end.z     = 63 - pos[Y];

                    sendPacketBlockLine(&contained, 0);
                    local_player.blocks -= amount;
                }

                players[local_player.id].item_showup = window_time();
            }
        }
        local_player.drag_active = 0;
        if (action == WINDOW_PRESS && players[local_player.id].held_item == TOOL_BLOCK
           && window_time() - players[local_player.id].item_showup >= 0.5F) {
            int * pos = camera_terrain_pick(0);

            if (pos != NULL && pos[Y] > 1 && norm3f(camera.pos.x, camera.pos.y, camera.pos.z, pos[X], pos[Y], pos[Z]) < 25.0F) {
                local_player.drag_active = 1;
                local_player.drag.x = pos[X];
                local_player.drag.y = pos[Y];
                local_player.drag.z = pos[Z];
            }
        }

        button_map.rmb = (action == WINDOW_PRESS);
    }

    if (button == WINDOW_MOUSE_MMB) button_map.mmb = (action == WINDOW_PRESS);

    if (camera.mode == CAMERAMODE_BODYVIEW && button == WINDOW_MOUSE_MMB && action == WINDOW_PRESS) {
        float nearest_dist = FLT_MAX;
        int nearest_player = -1;
        for (int k = 0; k < PLAYERS_MAX; k++) {
            float dist = norm3f(camera.pos.x, camera.pos.y, camera.pos.z, players[k].pos.x, players[k].pos.y, players[k].pos.z);

            if (player_can_spectate(&players[k]) && players[k].alive && k != cameracontroller_bodyview_player && dist < nearest_dist) {
                nearest_dist   = dist;
                nearest_player = k;
            }
        }

        if (nearest_player >= 0) cameracontroller_bodyview_player = nearest_player;
    }

    if (button == WINDOW_MOUSE_RMB && action == WINDOW_PRESS) {
        players[local_player.id].start.rmb = window_time();
        if (camera.mode == CAMERAMODE_BODYVIEW || camera.mode == CAMERAMODE_SPECTATOR) {
            if (camera.mode == CAMERAMODE_SPECTATOR)
                cameracontroller_bodyview_mode = 1;
            for (int k = 0; k < PLAYERS_MAX * 2; k++) {
                cameracontroller_bodyview_player = (cameracontroller_bodyview_player + 1) % PLAYERS_MAX;
                if (player_can_spectate(&players[cameracontroller_bodyview_player])) break;
            }
            cameracontroller_bodyview_zoom = 0.0F;
        }
    }

    if (button == WINDOW_MOUSE_LMB) {
        if (camera.mode == CAMERAMODE_FPS && window_time() - players[local_player.id].item_showup >= 0.5F) {
            if (players[local_player.id].held_item == TOOL_GRENADE && local_player.grenades > 0) {
                if (action == WINDOW_RELEASE) {
                    local_player.grenades = max(local_player.grenades - 1, 0);

                    PacketGrenade contained;
                    contained.player_id   = local_player.id;
                    contained.fuse_length = max(3.0F - (window_time() - players[local_player.id].start.lmb), 0.0F);

                    contained.pos = htonv3f(players[local_player.id].pos);

                    contained.vel.x = (contained.fuse_length == 0.0F) ? 0.0F :
                                      (players[local_player.id].orientation.x + players[local_player.id].physics.velocity.x);
                    contained.vel.y = (contained.fuse_length == 0.0F) ? 0.0F :
                                      (players[local_player.id].orientation.z + players[local_player.id].physics.velocity.z);
                    contained.vel.z = (contained.fuse_length == 0.0F) ? 0.0F :
                                      (-players[local_player.id].orientation.y - players[local_player.id].physics.velocity.y);

                    sendPacketGrenade(&contained, 0);

                    handlePacketGrenade(&contained); // server won’t loop packet back
                    players[local_player.id].item_showup = window_time();
                }

                if (action == WINDOW_PRESS) {
                    sound_create(SOUND_LOCAL, sound(SOUND_GRENADE_PIN), 0.0F, 0.0F, 0.0F);
                }
            }
        }
    }

    if (button == WINDOW_MOUSE_LMB && action == WINDOW_PRESS) {
        players[local_player.id].start.lmb = window_time();

        if (camera.mode == CAMERAMODE_FPS) {
            if (players[local_player.id].held_item == TOOL_GUN) {
                if (weapon_reloading()) weapon_reload_abort();

                if (local_player.ammo == 0 && window_time() - players[local_player.id].item_showup >= 0.5F) {
                    sound_create(SOUND_LOCAL, sound(SOUND_EMPTY), 0.0F, 0.0F, 0.0F);

                    static const char popup[] = "RELOAD";
                    chat_showpopup(popup, sizeof(popup), ASCII, 0.4F, Red);
                }
            }
        }

        if (camera.mode == CAMERAMODE_BODYVIEW || camera.mode == CAMERAMODE_SPECTATOR) {
            if (camera.mode == CAMERAMODE_SPECTATOR)
                cameracontroller_bodyview_mode = 1;
            for (int k = 0; k < PLAYERS_MAX * 2; k++) {
                cameracontroller_bodyview_player = (cameracontroller_bodyview_player - 1) % PLAYERS_MAX;
                if (cameracontroller_bodyview_player < 0)
                    cameracontroller_bodyview_player = PLAYERS_MAX - 1;
                if (player_can_spectate(&players[cameracontroller_bodyview_player]))
                    break;
            }
            cameracontroller_bodyview_zoom = 0.0F;
        }
    }
}

typedef struct {
    const char * str;
    int acceptance;
} AutocompleteType;

static int autocomplete_type_cmp(const void * a, const void * b) {
    AutocompleteType * A = (AutocompleteType *) a, * B = (AutocompleteType *) b;
    return B->acceptance - A->acceptance;
}

void broadcast_chat(unsigned char chat_type, const char * message, size_t size) {
    char buff[2048]; size_t written = encodeMagic(buff, sizeof(buff), message, size);

    PacketChatMessage contained;
    contained.player_id = local_player.id;
    contained.chat_type = chat_type;
    contained.message   = buff;

    sendPacketChatMessage(&contained, written);
}

static const char * hud_ingame_completeword(const char * s) {
    // find most likely player name or command

    AutocompleteType candidates[PLAYERS_MAX * 2 + 64] = {{0}};
    int candidates_cnt = 0;

    for (int k = 0; k < PLAYERS_MAX; k++) {
        if (players[k].connected)
            candidates[candidates_cnt++] = (AutocompleteType) {
                .str = players[k].name,
                .acceptance = 0,
            };
    }

    candidates[candidates_cnt++] = (AutocompleteType) {
        .str = gamestate.team_1.name,
        .acceptance = 0,
    };

    candidates[candidates_cnt++] = (AutocompleteType) {
        .str = gamestate.team_2.name,
        .acceptance = 0,
    };
    candidates[candidates_cnt++] = (AutocompleteType) {"/help", 0};
    candidates[candidates_cnt++] = (AutocompleteType) {"/medkit", 0};
    candidates[candidates_cnt++] = (AutocompleteType) {"/squad", 0};
    candidates[candidates_cnt++] = (AutocompleteType) {"/votekick", 0};
    candidates[candidates_cnt++] = (AutocompleteType) {"/login", 0};
    candidates[candidates_cnt++] = (AutocompleteType) {"/airstrike", 0};
    candidates[candidates_cnt++] = (AutocompleteType) {"/streak", 0};
    candidates[candidates_cnt++] = (AutocompleteType) {"/ratio", 0};
    candidates[candidates_cnt++] = (AutocompleteType) {"/intel", 0};
    candidates[candidates_cnt++] = (AutocompleteType) {"/time", 0};
    candidates[candidates_cnt++] = (AutocompleteType) {"/admin", 0};
    candidates[candidates_cnt++] = (AutocompleteType) {"/ping", 0};

    // valuate all strings
    for (int k = 0; k < candidates_cnt; k++) {
        for (int i = 0; i < strlen(candidates[k].str) && i < strlen(s); i++) {
            if (candidates[k].str[i] == s[i])
                candidates[k].acceptance += 2;
            else if (tolower(candidates[k].str[i]) == tolower(s[i]) || s[i] == '*') {
                candidates[k].acceptance++;
            } else {
                candidates[k].acceptance = 0;
                break;
            }
        }
    }

    qsort(candidates, candidates_cnt, sizeof(AutocompleteType), autocomplete_type_cmp);
    return (strlen(candidates[0].str) > 0 && candidates[0].acceptance > 0) ? candidates[0].str : NULL;
}

static void hud_ingame_keyboard(int key, int action, int mods, int internal) {
    if (show_exit) {
        if (action == WINDOW_PRESS) {
            if (key == WINDOW_KEY_NO) {
                show_exit = 0;
                window_mousemode(WINDOW_CURSOR_DISABLED);
            }

            if (key == WINDOW_KEY_YES) hud_change(&hud_serverlist);
        }

        return;
    }

    if (chat_input_mode != CHAT_NO_INPUT && action == WINDOW_PRESS && key == WINDOW_KEY_TAB && strlen(chat[0][0]) > 0) {
        // autocomplete word
        char * incomplete = strrchr(chat[0][0], ' ') + 1;
        if (incomplete == (char*)1)
            incomplete = chat[0][0];
        const char * match = hud_ingame_completeword(incomplete);
        if (match && strlen(match) + strlen(chat[0][0]) < 128)
            strcpy(incomplete, match);
    }

    if (chat_input_mode == CHAT_NO_INPUT) {
        if (action == WINDOW_RELEASE) {
            if (key == WINDOW_KEY_COMMAND) {
                window_textinput(1);
                chat_input_mode = CHAT_ALL_INPUT;
                strcpy(chat[0][0], "/");
            }

            if (key == WINDOW_KEY_YES) {
                window_textinput(1);
                chat_input_mode = CHAT_TEAM_INPUT;
                chat[0][0][0] = 0;
            }

            if (key == WINDOW_KEY_CHAT) {
                window_textinput(1);
                chat_input_mode = CHAT_ALL_INPUT;
                chat[0][0][0] = 0;
            }
        } else if (action == WINDOW_PRESS) {
            if (!network_connected) {
                if (key == WINDOW_KEY_F1) {
                    camera.mode = CAMERAMODE_SELECTION;
                }
                if (key == WINDOW_KEY_F2) {
                    camera.mode = CAMERAMODE_FPS;
                }
                if (key == WINDOW_KEY_F3) {
                    camera.mode = CAMERAMODE_SPECTATOR;
                }
                if (key == WINDOW_KEY_F4) {
                    camera.mode = CAMERAMODE_BODYVIEW;
                }
                if (key == WINDOW_KEY_SNEAK) {
                    log_debug("%f,%f,%f,%f,%f", camera.pos.x, camera.pos.y, camera.pos.z, camera.rot.x, camera.rot.y);
                    players[local_player.id].pos.x = 256.0F;
                    players[local_player.id].pos.y = 63.0F;
                    players[local_player.id].pos.z = 256.0F;
                }
                if (key == WINDOW_KEY_CROUCH) {
                    players[local_player.id].alive = !players[local_player.id].alive;
                }
            }

            if (key == WINDOW_KEY_LASTTOOL) {
                int tmp = players[local_player.id].held_item;
                players[local_player.id].held_item = local_player.last_tool;
                local_player.last_tool = tmp;
                player_on_held_item_change(players + local_player.id);
            }

            if (key == WINDOW_KEY_VOLUME_UP) {
                settings.volume = min(settings.volume + 1, 10);
            }

            if (key == WINDOW_KEY_VOLUME_DOWN) {
                settings.volume = max(settings.volume - 1, 0);
            }

            if (key == WINDOW_KEY_VOLUME_UP || key == WINDOW_KEY_VOLUME_DOWN) {
                sound_volume(settings.volume / 10.0F);
                char volstr[64];
                sprintf(volstr, "Volume: %i", settings.volume);
                chat_add(0, Red, volstr, sizeof(volstr), ASCII);
            }

            if ((key == WINDOW_KEY_CURSOR_UP || key == WINDOW_KEY_CURSOR_DOWN || key == WINDOW_KEY_CURSOR_LEFT || key == WINDOW_KEY_CURSOR_RIGHT)
               && camera.mode == CAMERAMODE_FPS && players[local_player.id].held_item == TOOL_BLOCK) {
                if (local_player.color[X] < 0 || local_player.color[Y] < 0) {
                    local_player.color[X] = 3; local_player.color[Y] = 0;
                } else {
                    switch (key) {
                        case WINDOW_KEY_CURSOR_LEFT: {
                            local_player.color[X]--;
                            if (local_player.color[X] < 0)
                                local_player.color[X] = 7;
                            break;
                        }

                        case WINDOW_KEY_CURSOR_RIGHT: {
                            local_player.color[X]++;
                            if (local_player.color[X] > 7)
                                local_player.color[X] = 0;
                            break;
                        }

                        case WINDOW_KEY_CURSOR_UP: {
                            local_player.color[Y]--;
                            if (local_player.color[Y] < 0)
                                local_player.color[Y] = 7;
                            break;
                        }

                        case WINDOW_KEY_CURSOR_DOWN: {
                            local_player.color[Y]++;
                            if (local_player.color[Y] > 7)
                                local_player.color[Y] = 0;
                            break;
                        }
                    }
                }

                players[local_player.id].block = texture_block_color(local_player.color[X], local_player.color[Y]);
                network_updateColor();
            }

            if (key == WINDOW_KEY_RELOAD && camera.mode == CAMERAMODE_FPS
               && players[local_player.id].held_item == TOOL_GUN) {
                weapon_reload();
            }

            if (key == WINDOW_KEY_SNEAK && (camera.mode == CAMERAMODE_BODYVIEW || camera.mode == CAMERAMODE_SPECTATOR)) {
                cameracontroller_bodyview_mode = !cameracontroller_bodyview_mode;
            }

            for (int k = 0; k < list_size(&config_keybind); k++) {
                Keybind * keybind = list_get(&config_keybind, k);

                if (keybind->key == internal) {
                    size_t size = strsize(keybind->value, sizeof(keybind->value));
                    if (size > 0) broadcast_chat(CHAT_ALL, keybind->value, size);

                    break;
                }
            }

            if (screen_current == SCREEN_NONE && camera.mode == CAMERAMODE_FPS) {
                unsigned char tool_switch = 0;
                switch (key) {
                    case WINDOW_KEY_TOOL1:
                        if (players[local_player.id].held_item != TOOL_SPADE) {
                            local_player.last_tool = players[local_player.id].held_item;
                            players[local_player.id].held_item = TOOL_SPADE;
                            tool_switch = 1;
                        }
                        break;
                    case WINDOW_KEY_TOOL2:
                        if (players[local_player.id].held_item != TOOL_BLOCK) {
                            local_player.last_tool = players[local_player.id].held_item;
                            players[local_player.id].held_item = TOOL_BLOCK;
                            tool_switch = 1;
                        }
                        break;
                    case WINDOW_KEY_TOOL3:
                        if (players[local_player.id].held_item != TOOL_GUN) {
                            local_player.last_tool = players[local_player.id].held_item;
                            players[local_player.id].held_item = TOOL_GUN;
                            tool_switch = 1;
                        }
                        break;
                    case WINDOW_KEY_TOOL4:
                        if (players[local_player.id].held_item != TOOL_GRENADE) {
                            local_player.last_tool = players[local_player.id].held_item;
                            players[local_player.id].held_item = TOOL_GRENADE;
                            tool_switch = 1;
                        }
                        break;
                }

                if (tool_switch) {
                    sound_create(SOUND_LOCAL, sound(SOUND_SWITCH), 0.0F, 0.0F, 0.0F);
                    player_on_held_item_change(players + local_player.id);
                }
            }

            if (screen_current == SCREEN_NONE) {
                if (key == WINDOW_KEY_CHANGETEAM) {
                    screen_current = SCREEN_TEAM_SELECT;
                    return;
                }
                if (key == WINDOW_KEY_CHANGEWEAPON) {
                    screen_current = SCREEN_GUN_SELECT;
                    return;
                }
            }

            static int new_team = -1;

            if (screen_current == SCREEN_TEAM_SELECT) {
                switch (key) {
                    case WINDOW_KEY_SELECT1: new_team = TEAM_1;         break;
                    case WINDOW_KEY_SELECT2: new_team = TEAM_SPECTATOR; break;
                    case WINDOW_KEY_SELECT3: new_team = TEAM_2;         break;
                    default:                 new_team = -1;             break;
                }

                if (new_team >= 0) {
                    if (network_logged_in) {
                        PacketChangeTeam contained;
                        contained.player_id = local_player.id;
                        contained.team      = new_team;
                        sendPacketChangeTeam(&contained, 0);

                        screen_current = SCREEN_NONE;
                        return;
                    } else {
                        if (new_team == TEAM_SPECTATOR) {
                            network_join_game(new_team, WEAPON_RIFLE);
                            screen_current = SCREEN_NONE;
                        } else if (default_gun >= 0) {
                            network_join_game(new_team, default_gun);
                            screen_current = SCREEN_NONE;
                        } else {
                            screen_current = SCREEN_GUN_SELECT;
                        }
                        return;
                    }
                }

                if ((key == WINDOW_KEY_CHANGETEAM || key == WINDOW_KEY_ESCAPE)
                   && (!network_connected || (network_connected && network_logged_in))) {
                    screen_current = SCREEN_NONE;
                    return;
                }
            }

            if (screen_current == SCREEN_GUN_SELECT) {
                int new_gun = -1;
                switch (key) {
                    case WINDOW_KEY_SELECT1: new_gun = WEAPON_RIFLE;   break;
                    case WINDOW_KEY_SELECT2: new_gun = WEAPON_SMG;     break;
                    case WINDOW_KEY_SELECT3: new_gun = WEAPON_SHOTGUN; break;
                    default:                 new_gun = -1;             break;
                }

                if (new_gun >= 0) {
                    if (network_logged_in) {
                        PacketChangeWeapon contained;
                        contained.player_id = local_player.id;
                        contained.weapon    = new_gun;

                        sendPacketChangeWeapon(&contained, 0);
                    } else network_join_game(new_team >= 0 ? new_team : default_team, new_gun);

                    screen_current = SCREEN_NONE;
                    return;
                }

                if ((key == WINDOW_KEY_CHANGEWEAPON || key == WINDOW_KEY_ESCAPE)
                   && (!network_connected || (network_connected && network_logged_in))) {
                    screen_current = SCREEN_NONE;
                    return;
                }
            }

            if (key == WINDOW_KEY_ESCAPE) {
                show_exit ^= 1;
                window_mousemode(show_exit ? WINDOW_CURSOR_ENABLED : WINDOW_CURSOR_DISABLED);
                return;
            }

            if (key == WINDOW_KEY_PICKCOLOR && players[local_player.id].held_item == TOOL_BLOCK) {
                players[local_player.id].item_disabled = window_time();
                players[local_player.id].items_show_start = window_time();
                players[local_player.id].items_show = 1;

                CameraHit hit;
                camera_hit_fromplayer(&hit, local_player.id, 128.0F);

                local_player.color[X] = local_player.color[Y] = -1;

                switch (hit.type) {
                    case CAMERA_HITTYPE_BLOCK: {
                        float dmg = (100.0F - map_damage_get(hit.x, hit.y, hit.z)) / 100.0F * 0.75F + 0.25F;
                        TrueColor color = map_get(hit.x, hit.y, hit.z);
                        players[local_player.id].block.r = color.r * dmg;
                        players[local_player.id].block.g = color.g * dmg;
                        players[local_player.id].block.b = color.b * dmg;
                        break;
                    }
                    case CAMERA_HITTYPE_PLAYER:
                        players[local_player.id].block = players[hit.player_id].block;
                        break;
                    case CAMERA_HITTYPE_NONE:
                    default:
                        players[local_player.id].block.r = fog_color[0] * 255;
                        players[local_player.id].block.g = fog_color[1] * 255;
                        players[local_player.id].block.b = fog_color[2] * 255;
                        break;
                }

                network_updateColor();
            }
        }
    } else {
        if (action != WINDOW_RELEASE) {
            if (key == WINDOW_KEY_V && mods) {
                const char * clipboard = window_clipboard();
                if (clipboard) strcat(chat[0][0], clipboard);
            }

            if (key == WINDOW_KEY_ESCAPE || key == WINDOW_KEY_ENTER) {
                size_t size = strsize(chat[0][0], sizeof(chat[0][0]));

                if (key == WINDOW_KEY_ENTER && size > 0)
                    broadcast_chat(chat_input_mode == CHAT_ALL_INPUT ? CHAT_ALL : CHAT_TEAM, chat[0][0], size);

                window_textinput(0);
                chat_input_mode = CHAT_NO_INPUT;
            }

            if (key == WINDOW_KEY_BACKSPACE) {
                size_t len = strlen(chat[0][0]);
                if (len > 0) {
                    while (CONT(chat[0][0][--len]) && len > 0);
                    chat[0][0][len] = 0;
                }
            }
        }
    }
}

static void hud_ingame_touch(void * finger, int action, float x, float y, float dx, float dy) {
    window_setmouseloc(x, y);
    WindowFinger * f = (WindowFinger *) finger;

    if (action != TOUCH_MOVE) {
        int k = 0;
        while (hud_ingame_onscreencontrol(k, NULL, -1)) {
            if (is_inside_centered(f->start.x, settings.window_height - f->start.y,
                                  settings.window_height * (0.2F + 0.175F * k), settings.window_height * 0.96F,
                                  settings.window_height * 0.15F, settings.window_height * 0.1F)) {
                hud_ingame_onscreencontrol(k, NULL, (action == TOUCH_DOWN) ? 1 : 0);
                return;
            }
            k++;
        }
        if (is_inside_centered(f->start.x, settings.window_height - f->start.y,
                              settings.window_width - settings.window_height * 0.075F, settings.window_height * 0.6F,
                              settings.window_height * 0.15F, settings.window_height * 0.1F)) {
            hud_ingame_onscreencontrol(64, NULL, (action == TOUCH_DOWN) ? 1 : 0);
            return;
        }
        if (is_inside_centered(f->start.x, settings.window_height - f->start.y,
                              settings.window_width - settings.window_height * 0.075F, settings.window_height * 0.45F,
                              settings.window_height * 0.15F, settings.window_height * 0.1F)) {
            hud_ingame_onscreencontrol(65, NULL, (action == TOUCH_DOWN) ? 1 : 0);
            return;
        }
    }

    if (screen_current == SCREEN_TEAM_SELECT && action == TOUCH_UP) {
        if (x < settings.window_width / 3)
            hud_ingame_keyboard(WINDOW_KEY_TOOL1, WINDOW_PRESS, 0, 0);
        if (x > settings.window_width / 3 * 2)
            hud_ingame_keyboard(WINDOW_KEY_TOOL2, WINDOW_PRESS, 0, 0);
        if (x > settings.window_width / 3 && x < settings.window_width / 3 * 2)
            hud_ingame_keyboard(WINDOW_KEY_TOOL3, WINDOW_PRESS, 0, 0);
        return;
    }

    if (screen_current == SCREEN_GUN_SELECT && action == TOUCH_UP) {
        if (x < settings.window_width / 3)
            hud_ingame_keyboard(WINDOW_KEY_TOOL1, WINDOW_PRESS, 0, 0);
        if (x > settings.window_width / 3 * 2)
            hud_ingame_keyboard(WINDOW_KEY_TOOL3, WINDOW_PRESS, 0, 0);
        if (x > settings.window_width / 3 && x < settings.window_width / 3 * 2)
            hud_ingame_keyboard(WINDOW_KEY_TOOL2, WINDOW_PRESS, 0, 0);
        return;
    }

    if (screen_current == SCREEN_NONE) {
        if (action == TOUCH_DOWN && x > settings.window_width - settings.window_height * 0.25F
           && y < settings.window_height * 0.25F) {
            window_pressed_keys[WINDOW_KEY_MAP] = !window_pressed_keys[WINDOW_KEY_MAP];
            return;
        }

        if ((camera.mode == CAMERAMODE_FPS || camera.mode == CAMERAMODE_SPECTATOR)
           && norm2f(f->start.x, f->start.y, settings.window_height * 0.3F, settings.window_height * 0.7F) <
              sqrf(settings.window_height * 0.15F)) {
            float mx = clamp(-settings.window_height * 0.2F, settings.window_height * 0.2F, x - settings.window_height * 0.3F);
            float my = clamp(-settings.window_height * 0.2F, settings.window_height * 0.2F, y - settings.window_height * 0.7F);

            hud_ingame_touch_x = mx;
            hud_ingame_touch_y = -my;
            if (absf(mx) > settings.window_height * 0.045F) {
                window_pressed_keys[WINDOW_KEY_LEFT] = mx < 0;
                window_pressed_keys[WINDOW_KEY_RIGHT] = mx > 0;
            } else {
                window_pressed_keys[WINDOW_KEY_LEFT] = 0;
                window_pressed_keys[WINDOW_KEY_RIGHT] = 0;
            }
            if (absf(my) > settings.window_height * 0.045F) {
                window_pressed_keys[WINDOW_KEY_UP] = my < 0;
                window_pressed_keys[WINDOW_KEY_DOWN] = my > 0;
            } else {
                window_pressed_keys[WINDOW_KEY_UP] = 0;
                window_pressed_keys[WINDOW_KEY_DOWN] = 0;
            }
            // window_pressed_keys[WINDOW_KEY_CROUCH] = (window_time()-f->down_time)>0.25F &&
            // absf(mx)<settings.window_height*0.06F && absf(my)<settings.window_height*0.06F;
            window_pressed_keys[WINDOW_KEY_SPRINT]
                = absf(mx) > settings.window_height * 0.19F || absf(my) > settings.window_height * 0.19F;
            if (action == TOUCH_UP) {
                window_pressed_keys[WINDOW_KEY_LEFT] = 0;
                window_pressed_keys[WINDOW_KEY_RIGHT] = 0;
                window_pressed_keys[WINDOW_KEY_UP] = 0;
                window_pressed_keys[WINDOW_KEY_DOWN] = 0;
                window_pressed_keys[WINDOW_KEY_SPRINT] = 0;
                // window_pressed_keys[WINDOW_KEY_CROUCH] = 0;
                hud_ingame_touch_x = 0;
                hud_ingame_touch_y = 0;
            }
            return;
        }

        if (camera.mode == CAMERAMODE_BODYVIEW && action == TOUCH_UP) {
            if (x < settings.window_width / 2)
                hud_ingame_mouseclick(0, 0, WINDOW_MOUSE_LMB, WINDOW_PRESS, 0);
            if (x > settings.window_width / 2)
                hud_ingame_mouseclick(0, 0, WINDOW_MOUSE_RMB, WINDOW_PRESS, 0);
            return;
        }

        if (1) {
            camera.rot.x -= dx * 0.002F;
            camera.rot.y += dy * 0.002F;
            camera_overflow_adjust();
            return;
        }
    }
}

HUD hud_ingame = {
    hud_ingame_init,
    hud_ingame_render3D,
    hud_ingame_render,
    hud_ingame_keyboard,
    hud_ingame_mouselocation,
    hud_ingame_mouseclick,
    hud_ingame_scroll,
    hud_ingame_touch,
    hud_ingame_focus,
    hud_ingame_hover,
    NULL,
    1,
    0,
    NULL,
};

/*         HUD_SERVERLIST START        */

static http_t * request_serverlist = NULL;
static http_t * request_news = NULL;
static int server_count = 0;
static int player_count = 0;
static Server * serverlist;

static int serverlist_con_established;

typedef struct News {
    Texture * image;
    char caption[65];
    char url[129];
    float tile_size;
    int color;
    struct News * next;
} News;

static News serverlist_news;

static int serverlist_news_exists = 0;
static char serverlist_input[128];

typedef int (*serverlist_sort)(const Server *, const Server *);

static serverlist_sort hud_serverlist_sort_chosen = NULL;
bool serverlist_descending = true;

static int hud_serverlist_cmp(const void * a, const void * b) {
    const Server * A = (const Server*) a;
    const Server * B = (const Server*) b;

    return serverlist_descending ? hud_serverlist_sort_chosen(A, B) : -hud_serverlist_sort_chosen(A, B);
}

static void hud_serverlist_sort(serverlist_sort cmp) {
    if (cmp != hud_serverlist_sort_chosen) {
        hud_serverlist_sort_chosen = cmp;
        serverlist_descending      = true;
    } else serverlist_descending = !serverlist_descending;

    qsort(serverlist, server_count, sizeof(Server), hud_serverlist_cmp);
}

static int hud_serverlist_sort_default(const void * a, const void * b) {
    const Server * A = (const Server*) a;
    const Server * B = (const Server*) b;

    if (strcmp(A->country, "LAN") == 0)
        return -1;

    if (strcmp(B->country, "LAN") == 0)
        return 1;

    if (A->current == B->current) {
        if (A->ping == B->ping)
            return strcmp(A->name, B->name);
        else {
            if (A->ping < 0) return +1;
            if (B->ping < 0) return -1;

            return A->ping - B->ping;
        }
    }

    return B->current - A->current;
}

static int hud_serverlist_sort_players(const Server * a, const Server * b) {
    return b->current - a->current;
}

static int hud_serverlist_sort_name(const Server * a, const Server * b) {
    return strcmp(a->name, b->name);
}

static int hud_serverlist_sort_map(const Server * a, const Server * b) {
    return strcmp(a->map, b->map);
}

static int hud_serverlist_sort_mode(const Server * a, const Server * b) {
    return strcmp(a->gamemode, b->gamemode);
}

static int hud_serverlist_sort_ping(const Server * a, const Server * b) {
    if (a->ping < 0) return +1;
    if (b->ping < 0) return -1;

    return a->ping - b->ping;
}

static void hud_serverlist_pingupdate(void * entry, float time_delta, char * aos) {
    pthread_mutex_lock(&serverlist_lock);
    if (!entry) {
        for (int k = 0; k < server_count; k++)
            if (!strcmp(serverlist[k].identifier, aos)) {
                serverlist[k].ping = ceil(time_delta * 1000.0F);
                break;
            }
    } else {
        serverlist = realloc(serverlist, (++server_count) * sizeof(Server));
        memcpy(serverlist + server_count - 1, entry, sizeof(Server));
    }

    qsort(serverlist, server_count, sizeof(Server), hud_serverlist_sort_default);
    pthread_mutex_unlock(&serverlist_lock);
}

bool offline = false;

char serverlist_url[] = "http://services.buildandshoot.com/serverlist.json", newslist_url[] = "http://aos.party/bs/news/";

static void hud_serverlist_init() {
    ping_stop();
    network_disconnect();
    window_title(NULL);
    rpc_seti(RPC_VALUE_SLOTS, 0);

    window_mousemode(WINDOW_CURSOR_ENABLED);

    pthread_mutex_lock(&serverlist_lock);
    player_count = server_count = 0;
    pthread_mutex_unlock(&serverlist_lock);

    if (!offline) {
        request_serverlist = http_get(serverlist_url, NULL);

        if (!serverlist_news_exists)
            request_news = http_get(newslist_url, NULL);
    }

    serverlist_con_established = request_serverlist != NULL;
    *serverlist_input = 0;

    window_textinput(1);

    hud_serverlist_sort_chosen = NULL;
    serverlist_descending = true;

    ping_start(hud_serverlist_pingupdate);
}

static void server_c(char * address, char * name, Version version) {
    if (file_exists(address)) {
        void * data = file_load(address);
        map_vxl_load(data, file_size(address));
        free(data);
        chunk_rebuild_all();
        camera.mode = CAMERAMODE_FPS;
        players[local_player.id].pos.x = map_size_x / 2.0F;
        players[local_player.id].pos.y = map_size_y - 1.0F;
        players[local_player.id].pos.z = map_size_z / 2.0F;
        window_title(address);
        hud_change(&hud_ingame);
    } else {
        window_title(name);
        if (name && address) {
            rpc_setv(RPC_VALUE_SERVERNAME, name);
            rpc_setv(RPC_VALUE_SERVERURL, address);
            rpc_seti(RPC_VALUE_SLOTS, 32);
        } else {
            rpc_seti(RPC_VALUE_SLOTS, 0);
        }

        if (network_connect_string(address, version))
            hud_change(&hud_ingame);
    }
}

static Texture * hud_serverlist_ui_images(int icon_id, bool * resize) {
    if (icon_id >= 32) {
        News * current = &serverlist_news;
        int index = 32;
        while (current) {
            if (index == icon_id)
                return current->image;

            index++;
            current = current->next;
        }
    }

    switch (icon_id) {
        case MU_ICON_EXPANDED:  return texture(TEXTURE_UI_EXPANDED);
        case MU_ICON_COLLAPSED: return texture(TEXTURE_UI_COLLAPSED);
        default:                return NULL;
    }
}

static void hud_render_tab_button(mu_Context * ctx, float scale, const char * tabname, HUD * tabptr) {
    if (hud_active == tabptr)
        mu_text_color(ctx, 255, 255, 0);

    if (mu_button_ex(ctx, tabname, 0, MU_OPT_ALIGNCENTER | (hud_active == tabptr ? MU_OPT_NOINTERACT : 0)))
        hud_change(tabptr);

    mu_text_color_default(ctx);
}

static int hud_header_render(mu_Context * ctx, float scale, const char * text) {
    glColor3f(0.5F, 0.5F, 0.5F);
    float t = window_time() * 0.03125F;
    texture_draw_sector(texture(TEXTURE_UI_BG), 0.0F, settings.window_height, settings.window_width, settings.window_height, t,
                        t, settings.window_width / 512.0F, settings.window_height / 512.0F);

    mu_Rect frame = mu_rect(settings.window_width * 0.125F, 0, settings.window_width * 0.75F, settings.window_height);

    int retval = mu_begin_window_ex(ctx, "Main", frame, MU_OPT_NOFRAME | MU_OPT_NOTITLE | MU_OPT_NORESIZE);

    if (retval) {
        mu_Container * cnt = mu_get_current_container(ctx); cnt->rect = frame;

        int width = cnt->body.w;
        mu_layout_row(ctx, 4, (int[]) {0.166F * width, 0.166F * width, 0.166F * width, -1}, 0);

        hud_render_tab_button(ctx, scale, "Servers",  &hud_serverlist);
        hud_render_tab_button(ctx, scale, "Settings", &hud_settings);
        hud_render_tab_button(ctx, scale, "Controls", &hud_controls);

        mu_text_color_default(ctx); mu_button_ex(ctx, text, 0, MU_OPT_ALIGNRIGHT | MU_OPT_NOINTERACT);
    }

    return retval;
}

static void hud_sort_button_render(mu_Context * ctx, float scale, const char * name, serverlist_sort cmp) {
    if (hud_serverlist_sort_chosen == cmp)
        mu_text_color(ctx, 255, 255, 0);

    if (mu_button_ex(ctx, name, 0, MU_OPT_ALIGNCENTER)) {
        pthread_mutex_lock(&serverlist_lock);
        hud_serverlist_sort(cmp);
        pthread_mutex_unlock(&serverlist_lock);
    }

    mu_text_color_default(ctx);
}

static void hud_serverlist_render(mu_Context * ctx, float scale) {
    char total_str[128]; sprintf(total_str, server_count > 0 ? "%i players on %i servers" : "No servers", player_count, server_count);

    char * join_address = NULL, * join_name = NULL; Version join_version = UNKNOWN;

    if (hud_header_render(ctx, scale, total_str)) {
        mu_layout_row(ctx, 1, (int[]) {-1}, settings.window_height * 0.3F);

        if (serverlist_news_exists && settings.show_news) {
            mu_begin_panel(ctx, "News");
            mu_layout_row(ctx, 0, NULL, 0);

            News * current = &serverlist_news;
            int index = 0;
            while (current) {
                mu_layout_begin_column(ctx);
                float size = settings.window_height * 0.3F - ctx->text_height(ctx->style->font) * 4.125F;
                mu_layout_row(ctx, 1, (int[]) {size * current->tile_size}, size);
                if (mu_button_ex(ctx, NULL, 32 + index, MU_OPT_NOFRAME)) {
                    if (!strncmp("aos://", current->url, 6)) {
                        join_address = current->url;
                        join_name    = current->caption;
                    } else file_url(current->url);
                }
                mu_layout_height(ctx, 0);
                mu_text_color(ctx, BYTE0(current->color), BYTE1(current->color), BYTE2(current->color));
                mu_text(ctx, current->caption);
                mu_text_color_default(ctx);
                mu_layout_end_column(ctx);
                index++;
                current = current->next;
            }

            mu_end_panel(ctx);
        }

        int a = ctx->text_width(ctx->style->font, "Refresh", 0) * 1.6F;
        int b = ctx->text_width(ctx->style->font, "Join", 0) * 2.0F;
        mu_layout_row(ctx, 3, (int[]) {-a - b, -a, -1}, 0);

        if (mu_textbox(ctx, serverlist_input, sizeof(serverlist_input)) & MU_RES_SUBMIT)
            join_address = serverlist_input;

        if (mu_button_ex(ctx, "Join", 0, MU_OPT_ALIGNCENTER))
            join_address = serverlist_input;

        if (mu_button_ex(ctx, "Refresh", 0, MU_OPT_ALIGNCENTER) && !request_serverlist)
            hud_serverlist_init();

        mu_layout_row(ctx, 1, (int[]) {-1}, -1);

        mu_begin_panel(ctx, "Servers");
        int width = mu_get_current_container(ctx)->body.w;

        int flag_width = ctx->style->size.y + ctx->style->padding * 2;
        mu_layout_row(ctx, 5, (int[]) {0.12F * width, 0.415F * width, 0.22F * width, 0.12F * width, -1}, 0);

        hud_sort_button_render(ctx, scale, "Players", hud_serverlist_sort_players);
        hud_sort_button_render(ctx, scale, "Name",    hud_serverlist_sort_name);
        hud_sort_button_render(ctx, scale, "Map",     hud_serverlist_sort_map);
        hud_sort_button_render(ctx, scale, "Mode",    hud_serverlist_sort_mode);
        hud_sort_button_render(ctx, scale, "Ping",    hud_serverlist_sort_ping);

        mu_layout_row(ctx, 6,
                      (int[]) {0.12F * width, flag_width, 0.415F * width - flag_width - ctx->style->spacing * 2,
                               0.22F * width, 0.12F * width, -1},
                      0);

        pthread_mutex_lock(&serverlist_lock);
        if (server_count > 0) {
            for (int k = 0; k < server_count; k++) {
                if (strstr(serverlist[k].name, serverlist_input) || strstr(serverlist[k].identifier, serverlist_input)
                 || strstr(serverlist[k].map, serverlist_input)  || strstr(serverlist[k].gamemode, serverlist_input)) {
                    int f = ((serverlist[k].current && serverlist[k].current < serverlist[k].max)
                             || serverlist[k].current < 0) ? 1 : 2;

                    if (serverlist[k].current >= 0)
                        sprintf(total_str, "%i/%i", serverlist[k].current, serverlist[k].max);
                    else
                        strcpy(total_str, "-");

                    mu_push_id(ctx, &serverlist[k].identifier, strlen(serverlist[k].identifier));

                    bool join = false;

                    float ratio = ((float) serverlist[k].current) / ((float) serverlist[k].max);

                    if (ratio >= 1.0)
                        mu_text_color(ctx, 255 / f, 0, 0);
                    else if (ratio >= 0.75)
                        mu_text_color(ctx, 255 / f, 255 / f, 0);
                    else
                        mu_text_color(ctx, 230 / f, 230 / f, 230 / f);

                    if (mu_button_ex(ctx, total_str, 0, MU_OPT_NOFRAME | MU_OPT_ALIGNCENTER))
                        join = true;

                    mu_text_color(ctx, 230 / f, 230 / f, 230 / f);

                    if (mu_button_ex(ctx, "", texture_flag_index(serverlist[k].country) + HUD_FLAG_INDEX_START,
                                    MU_OPT_NOFRAME))
                        join = true;

                    if (mu_button_ex(ctx, serverlist[k].name, 0, MU_OPT_NOFRAME))
                        join = true;

                    if (mu_button_ex(ctx, serverlist[k].map, 0, MU_OPT_NOFRAME))
                        join = true;

                    if (mu_button_ex(ctx, serverlist[k].gamemode, 0, MU_OPT_NOFRAME | MU_OPT_ALIGNCENTER))
                        join = true;

                    if (serverlist[k].ping >= 0) {
                        if (serverlist[k].ping < 110)
                            mu_text_color(ctx, 0, 255 / f, 0);
                        else if (serverlist[k].ping < 200)
                            mu_text_color(ctx, 255 / f, 255 / f, 0);
                        else
                            mu_text_color(ctx, 255 / f, 0, 0);
                    }

                    sprintf(total_str, "%i", serverlist[k].ping);
                    if (mu_button_ex(ctx, (serverlist[k].ping >= 0) ? total_str : "?", 0,
                                    MU_OPT_NOFRAME | MU_OPT_ALIGNCENTER))
                        join = true;

                    mu_pop_id(ctx);

                    if (join) {
                        join_address = serverlist[k].identifier;
                        join_name    = serverlist[k].name;
                        join_version = serverlist[k].version;
                    }
                }
            }
        } else {
            mu_layout_row(ctx, 1, (int[]) {-1}, 0);
            mu_button_ex(ctx, "Fetching servers...", 0, MU_OPT_NOFRAME | MU_OPT_ALIGNCENTER);
        }

        pthread_mutex_unlock(&serverlist_lock);
        mu_text_color_default(ctx);
        mu_end_panel(ctx);

        mu_end_window(ctx);
    }

    if (window_time() - chat_popup_timer < chat_popup_duration
       && mu_begin_window_ex(ctx, "Disconnected from server", mu_rect(200, 250, 300, 100),
                             MU_OPT_HOLDFOCUS | MU_OPT_NORESIZE | MU_OPT_NOCLOSE)) {
        mu_Container * cnt = mu_get_current_container(ctx);
        mu_bring_to_front(ctx, cnt);
        cnt->rect = mu_rect((settings.window_width - 300 * scale) / 2, 250 * scale, 300 * scale, 100 * scale);
        mu_layout_row(ctx, 2, (int[]) {ctx->text_width(ctx->style->font, "Reason:", 0) * 1.5F, -1}, 0);
        mu_text(ctx, "Reason:");
        mu_text(ctx, chat_popup);
        mu_end_window(ctx);
    }

    if (request_news) {
        switch (http_process(request_news)) {
            case HTTP_STATUS_COMPLETED: {
                JSON_Value * js = json_parse_string(request_news->response_data);
                JSON_Array * news = json_value_get_array(js);
                int news_entries = json_array_get_count(news);

                News * current = &serverlist_news;
                memset(current, 0, sizeof(News));

                for (int k = 0; k < news_entries; k++) {
                    JSON_Object * s = json_array_get_object(news, k);
                    if (json_object_get_string(s, "caption"))
                        strncpy(current->caption, json_object_get_string(s, "caption"), sizeof(current->caption) - 1);

                    if (json_object_get_string(s, "url"))
                        strncpy(current->url, json_object_get_string(s, "url"), sizeof(current->url) - 1);

                    current->tile_size = json_object_get_number(s, "tilesize");
                    current->color     = json_object_get_number(s, "color");
                    current->image     = NULL;

                    if (json_object_get_string(s, "image")) {
                        char * img = (char *) json_object_get_string(s, "image");
                        size_t imglen = strlen(img);

                        if (imglen > 0) {
                            int size = base64_decode(img, imglen);

                            unsigned char * buffer; unsigned int width, height;
                            lodepng_decode32(&buffer, &width, &height, (uint8_t *) img, size);

                            current->image = texture_alloc();
                            texture_create_buffer(current->image, "image", width, height, buffer, 1);
                            texture_filter(current->image, TEXTURE_FILTER_LINEAR);
                        }
                    }

                    current->next = (k < news_entries - 1) ? malloc(sizeof(News)) : NULL;
                    current = current->next;
                }

                json_value_free(js);
                http_release(request_news);
                serverlist_news_exists = 1;
                request_news = NULL;
                break;
            }

            case HTTP_STATUS_FAILED: {
                http_release(request_news);
                request_news = NULL;
                break;
            }

            default: break;
        }
    }

    if (request_serverlist) {
        switch (http_process(request_serverlist)) {
            case HTTP_STATUS_PENDING: break;

            case HTTP_STATUS_COMPLETED: {
                JSON_Value * js = json_parse_string(request_serverlist->response_data);
                JSON_Array * servers = json_value_get_array(js);

                pthread_mutex_lock(&serverlist_lock);

                int begin = server_count; server_count += json_array_get_count(servers);
                serverlist = realloc(serverlist, server_count * sizeof(Server));
                CHECK_ALLOCATION_ERROR(serverlist)

                player_count = 0;
                for (int k = begin; k < server_count; k++) {
                    JSON_Object * s = json_array_get_object(servers, k - begin);
                    memset(&serverlist[k], 0, sizeof(Server));

                    serverlist[k].current = (int) json_object_get_number(s, "players_current");
                    serverlist[k].max     = (int) json_object_get_number(s, "players_max");
                    serverlist[k].ping    = -1;

                    strnzcpy(serverlist[k].name,       json_object_get_string(s, "name"),       sizeof(serverlist[k].name));
                    strnzcpy(serverlist[k].map,        json_object_get_string(s, "map"),        sizeof(serverlist[k].map));
                    strnzcpy(serverlist[k].gamemode,   json_object_get_string(s, "game_mode"),  sizeof(serverlist[k].gamemode));
                    strnzcpy(serverlist[k].identifier, json_object_get_string(s, "identifier"), sizeof(serverlist[k].identifier));
                    strnzcpy(serverlist[k].country,    json_object_get_string(s, "country"),    sizeof(serverlist[k].country));

                    serverlist[k].version = json_get_game_version(s);

                    Address addr;

                    if (network_identifier_split(serverlist[k].identifier, &addr))
                        ping_check(addr.ip, addr.port, serverlist[k].identifier);

                    player_count += serverlist[k].current;
                }

                qsort(serverlist, server_count, sizeof(Server), hud_serverlist_sort_default);
                pthread_mutex_unlock(&serverlist_lock);

                http_release(request_serverlist);
                json_value_free(js);
                request_serverlist = NULL;
                break;
            }

            case HTTP_STATUS_FAILED: {
                http_release(request_serverlist);
                hud_serverlist_init();
                break;
            }
        }
    }

    if (join_address) server_c(join_address, join_name, join_version);
}

static void hud_serverlist_touch(void * finger, int action, float x, float y, float dx, float dy) {
    window_setmouseloc(x, y);
    /*switch (action) {
        case TOUCH_DOWN: hud_serverlist_mouseclick(x, y, WINDOW_MOUSE_LMB, WINDOW_PRESS, 0); break;
        case TOUCH_MOVE: hud_serverlist_mouselocation(x, y); break;
        case TOUCH_UP: hud_serverlist_mouseclick(x, y, WINDOW_MOUSE_LMB, WINDOW_RELEASE, 0); break;
    }*/
}

HUD hud_serverlist = {
    hud_serverlist_init,
    NULL,
    hud_serverlist_render,
    NULL,
    NULL,
    NULL,
    NULL,
    hud_serverlist_touch,
    NULL,
    NULL,
    hud_serverlist_ui_images,
    0,
    0,
    NULL,
};

/*         HUD_SETTINGS START        */

static void hud_settings_init() {
    memcpy(&settings_tmp, &settings, sizeof(Options));
}

static int int_slider_defaults(mu_Context * ctx, Setting * setting) {
    int k = setting->defaults_length - 1;
    while (k > 0 && setting->defaults[k] > *(int*) setting->value)
        k--;

    float tmp = k;

    mu_push_id(ctx, setting, sizeof(setting));
    int res = mu_slider_ex(ctx, &tmp, 0, setting->defaults_length - 1, 0, "", MU_OPT_ALIGNCENTER);

    if (res & MU_RES_CHANGE)
        *(int*) setting->value = setting->defaults[(int) round(tmp)];

    if (setting->label_callback) {
        char buf[64];
        setting->label_callback(buf, sizeof(buf), setting->defaults[(int) round(tmp)], (int) round(tmp));
        mu_draw_control_text(ctx, buf, ctx->last_rect, MU_COLOR_TEXT, MU_OPT_ALIGNCENTER);
    }

    mu_pop_id(ctx);
    return res;
}

static int int_slider(mu_Context * ctx, int * value, int low, int high) {
    float tmp = *value;
    mu_push_id(ctx, &value, sizeof(value));
    int res = mu_slider_ex(ctx, &tmp, low, high, 0, "%.0f", MU_OPT_ALIGNCENTER);
    mu_pop_id(ctx);
    *value = round(tmp);
    return res;
}

static int int_number(mu_Context * ctx, int * value) {
    float tmp = *value;
    mu_push_id(ctx, &value, sizeof(value));
    int res = mu_number_ex(ctx, &tmp, 1, "%.0f", MU_OPT_ALIGNCENTER);
    mu_pop_id(ctx);
    *value = max(round(tmp), 0);
    return res;
}

static void hud_bool(mu_Context * ctx, bool * value) {
    mu_push_id(ctx, &value, sizeof(value));

    if (mu_button(ctx, *value ? "Yes" : "No"))
        *value = !(*value);

    mu_pop_id(ctx);
}

static Texture * hud_settings_ui_images(int icon_id, bool * resize) {
    switch (icon_id) {
        case MU_ICON_EXPANDED:  return texture(TEXTURE_UI_EXPANDED);
        case MU_ICON_COLLAPSED: return texture(TEXTURE_UI_COLLAPSED);
        default:                return NULL;
    }
}

static void hud_settings_render(mu_Context * ctx, float scale) {
    if (hud_header_render(ctx, scale, BETTERSPADES_VERSION_SUMMARY)) {
        mu_layout_row(ctx, 1, (int[]) {-1}, -1);
        mu_begin_panel(ctx, "Content");

        int width = mu_get_current_container(ctx)->body.w;

        char * category = NULL; int open = 0;

        for (int k = 0; k < list_size(&config_settings); k++) {
            Setting * a = list_get(&config_settings, k);

            if (!category || strcmp(category, a->category)) {
                category = a->category;
                open = mu_header_ex(ctx, a->category, MU_OPT_EXPANDED);
            }

            if (open) {
                mu_layout_row(ctx, 3, (int[]) {0.50F * width, -0.05F * width, -1}, 0);

                switch (a->type) {
                    case CONFIG_TYPE_STRING: {
                        mu_text(ctx, a->name);
                        mu_textbox(ctx, a->value, a->max + 1);
                        break;
                    }
                    case CONFIG_TYPE_INT: {
                        if (a->max == 1 && a->min == 0) {
                            mu_text(ctx, a->name);
                            hud_bool(ctx, a->value);
                        } else if (a->defaults_length > 0) {
                            mu_text(ctx, a->name);
                            int_slider_defaults(ctx, a);
                        } else if (a->max == INT_MAX) {
                            mu_text(ctx, a->name);
                            int_number(ctx, a->value);
                        } else {
                            mu_text(ctx, a->name);
                            int_slider(ctx, a->value, a->min, a->max);
                        }
                        break;
                    }
                    case CONFIG_TYPE_FLOAT: {
                        mu_text(ctx, a->name);
                        if (a->max == INT_MAX) {
                            mu_number(ctx, a->value, 0.1F);
                            *((float*) a->value) = max(a->min, *((float*) a->value));
                        } else {
                            mu_slider(ctx, a->value, a->min, a->max);
                        }
                        break;
                    }
                }

                if (*a->help) {
                    mu_push_id(ctx, &a->value, sizeof(a->value));

                    if (mu_begin_popup(ctx, "Help")) {
                        mu_layout_row(ctx, 1, (int[]) {ctx->text_width(ctx->style->font, a->help, 0)}, 0);
                        mu_text(ctx, a->help);
                        mu_end_popup(ctx);
                    }

                    if (mu_button(ctx, "?"))
                        mu_open_popup(ctx, "Help");

                    mu_pop_id(ctx);
                } else {
                    mu_layout_next(ctx);
                }
            }
        }

        mu_layout_row(ctx, 3, (int[]) {0.25F * width, 0.50F * width, -1}, 0);
        mu_layout_next(ctx);

        if (mu_button(ctx, "Apply changes")) {
            memcpy(&settings, &settings_tmp, sizeof(Options));
            window_fromsettings();
            sound_volume(settings.volume / 10.0F);
            config_save();
        }

        if (mu_header_ex(ctx, "Help", MU_OPT_EXPANDED)) {
            mu_layout_row(ctx, 1, (int[]) {-1}, -1);
            mu_text(ctx,
                    "To edit a value directly, [SHIFT]+LMB on its container to change it using the keyboard. You can "
                    "also drag on a container to modify its value relative to its current one.\n\nWhen finished click "
                    "[Apply changes] so that your settings are not lost.");
        }

        mu_end_panel(ctx);

        mu_end_window(ctx);
    }
}

static void hud_settings_touch(void * finger, int action, float x, float y, float dx, float dy) {
    window_setmouseloc(x, y);
}

HUD hud_settings = {
    hud_settings_init,
    NULL,
    hud_settings_render,
    NULL,
    NULL,
    NULL,
    NULL,
    hud_settings_touch,
    NULL,
    NULL,
    hud_settings_ui_images,
    0,
    0,
    NULL,
};

/*         HUD_CONTROLS START        */

static int * hud_controls_edit;

static void hud_controls_init() {
    hud_controls_edit = NULL;
}

static void hud_controls_render(mu_Context * ctx, float scale) {
    if (hud_header_render(ctx, scale, BETTERSPADES_VERSION_SUMMARY)) {
        mu_layout_row(ctx, 1, (int[]) {-1}, -1);
        mu_begin_panel(ctx, "Content");

        char * category = NULL;
        int open = 0;
        for (int k = 0; k < list_size(&config_keys); k++) {
            ConfigKeyPair * a = list_get(&config_keys, k);

            if (*a->display) {
                if (!category || strcmp(category, a->category)) {
                    category = a->category;

                    open = mu_header_ex(ctx, a->category, MU_OPT_EXPANDED);
                }

                if (open) {
                    int width = mu_get_current_container(ctx)->body.w;
                    if (a->def != a->original) {
                        mu_layout_row(ctx, 4,
                                      (int[]) {0.50F * width, ctx->text_width(ctx->style->font, "Reset", 0) * 1.5F,
                                               -0.05F * width, -1},
                                      0);
                    } else {
                        mu_layout_row(ctx, 3, (int[]) {0.50F * width, -0.05F * width, -1}, 0);
                    }

                    mu_push_id(ctx, a->display, sizeof(a->display));
                    mu_text(ctx, a->display);

                    if (a->def != a->original && mu_button(ctx, "Reset")) {
                        a->def = a->original;
                        config_save();
                    }

                    char name[32]; window_keyname(a->def, name, sizeof(name));

                    if (hud_controls_edit == &a->def)
                        mu_text_color(ctx, 255, 0, 0);

                    if (mu_button(ctx, name))
                        hud_controls_edit = (hud_controls_edit == &a->def) ? NULL : &a->def;

                    mu_text_color_default(ctx);
                    mu_pop_id(ctx);

                    mu_push_id(ctx, a->name, sizeof(a->name));

                    if (mu_begin_popup(ctx, "Help")) {
                        mu_layout_row(ctx, 1, (int[]) {ctx->text_width(ctx->style->font, a->name, 0)}, 0);
                        mu_text(ctx, a->name);
                        mu_end_popup(ctx);
                    }

                    if (mu_button(ctx, "?"))
                        mu_open_popup(ctx, "Help");

                    mu_pop_id(ctx);
                }
            }
        }

        if (mu_header_ex(ctx, "Key bindings", MU_OPT_EXPANDED)) {
            int width = mu_get_current_container(ctx)->body.w;
            mu_layout_row(ctx, 3, (int[]) {0.50F * width, -0.05F * width, -1}, 0);

            int idel = -1;

            for (int k = 0; k < list_size(&config_keybind); k++) {
                Keybind * keybind = list_get(&config_keybind, k);

                mu_textbox(ctx, keybind->value, sizeof(keybind->value));

                char keyname[32]; window_keyname(keybind->key, keyname, sizeof(keyname));

                mu_push_id(ctx, &k, sizeof(k));

                if (hud_controls_edit == &keybind->key)
                    mu_text_color(ctx, 255, 0, 0);

                if (mu_button(ctx, keyname))
                    hud_controls_edit = (hud_controls_edit == &keybind->key) ? NULL : &keybind->key;

                mu_text_color_default(ctx);

                if (mu_button(ctx, "X")) idel = k;

                mu_pop_id(ctx);
            }

            char buf[] = "\0"; mu_textbox_ex(ctx, buf, 0, MU_OPT_NOINTERACT);
            mu_button_ex(ctx, NULL, 0, MU_OPT_NOINTERACT);

            if (mu_button(ctx, "+")) {
                Keybind * keybind = list_add(&config_keybind, NULL);
                keybind->key = 0; memset(keybind->value, 0, sizeof(keybind->value));
            }

            mu_layout_row(ctx, 3, (int[]) {0.25F * width, 0.50F * width, -1}, 0);

            mu_layout_next(ctx);

            if (mu_button(ctx, "Save"))
                config_save();

            if (idel >= 0) list_remove(&config_keybind, idel);
        }

        mu_end_panel(ctx);

        mu_end_window(ctx);
    }
}

static void hud_controls_touch(void * finger, int action, float x, float y, float dx, float dy) {
    window_setmouseloc(x, y);
}

static void hud_controls_keyboard(int key, int action, int mods, int internal) {
    if (hud_controls_edit) {
        *hud_controls_edit = internal;
        hud_controls_edit = NULL;
        config_save();
    }
}

HUD hud_controls = {
    hud_controls_init,
    NULL,
    hud_controls_render,
    hud_controls_keyboard,
    NULL,
    NULL,
    NULL,
    hud_controls_touch,
    NULL,
    NULL,
    hud_settings_ui_images,
    0,
    0,
    NULL,
};
