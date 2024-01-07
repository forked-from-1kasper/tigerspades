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

#include <enet/enet.h>
#include <math.h>
#include <string.h>

#include <libdeflate.h>

#include <BetterSpades/texture.h>
#include <BetterSpades/common.h>
#include <BetterSpades/sound.h>
#include <BetterSpades/weapon.h>
#include <BetterSpades/grenade.h>
#include <BetterSpades/camera.h>
#include <BetterSpades/cameracontroller.h>
#include <BetterSpades/file.h>
#include <BetterSpades/hud.h>
#include <BetterSpades/map.h>
#include <BetterSpades/player.h>
#include <BetterSpades/network.h>
#include <BetterSpades/particle.h>
#include <BetterSpades/texture.h>
#include <BetterSpades/chunk.h>
#include <BetterSpades/config.h>

void (*packets[256])(void * data, int len) = {NULL};

int network_connected = 0;
int network_logged_in = 0;
int network_map_transfer = 0;
int network_received_packets = 0;
int network_map_cached = 0;

Position network_pos_last;
Orientation network_orient_last;

float network_pos_update           = 0.0F;
float network_orient_update        = 0.0F;
unsigned char network_keys_last    = 0;
unsigned char network_buttons_last = 0;
unsigned char network_tool_last    = 255;

void * compressed_chunk_data;
int compressed_chunk_data_size;
int compressed_chunk_data_offset = 0;
int compressed_chunk_data_estimate = 0;

struct network_stat network_stats[40];
float network_stats_last = 0.0F;

ENetHost * client;
ENetPeer * peer;

char network_custom_reason[17];

const char * network_reason_disconnect(int code) {
    if (*network_custom_reason)
        return network_custom_reason;
    switch (code) {
        case 1:  return "Banned";
        case 2:  return "Connection limit";
        case 3:  return "Wrong protocol";
        case 4:  return "Server full";
        case 5:  return "Server shutdown";
        case 10: return "Kicked";
        case 20: return "Invalid name";
        default: return "Unknown";
    }
}

static void printJoinMsg(int team, char * name) {
    char * t;
    switch (team) {
        case TEAM_1: t = gamestate.team_1.name; break;
        case TEAM_2: t = gamestate.team_2.name; break;
        default:
        case TEAM_SPECTATOR: t = "Spectator"; break;
    }

    char s[64]; sprintf(s, "%s joined the %s team", name, t);
    chat_add(0, Red, s, UTF8);
}

void network_join_game(unsigned char team, unsigned char weapon) {
    struct PacketExistingPlayer contained;
    contained.player_id = local_player_id;
    contained.team      = team;
    contained.weapon    = weapon;
    contained.held_item = TOOL_GUN;
    contained.kills     = 0;
    contained.blue      = players[local_player_id].block.b;
    contained.green     = players[local_player_id].block.g;
    contained.red       = players[local_player_id].block.r;

    encodeMagic(contained.name, settings.name, strlen(settings.name), sizeof(contained.name));
    network_send(PACKET_EXISTINGPLAYER_ID, &contained, sizeof(contained) - sizeof(contained.name) + strlen(contained.name) + 1);
}

void read_PacketMapChunk(void * data, int len) {
    // increase allocated memory if it is not enough to store the next chunk
    if (compressed_chunk_data_offset + len > compressed_chunk_data_size) {
        compressed_chunk_data_size += 1024 * 1024;
        compressed_chunk_data = realloc(compressed_chunk_data, compressed_chunk_data_size);
        CHECK_ALLOCATION_ERROR(compressed_chunk_data)
    }

    // accept any chunk length for “superior” performance, as pointed out by github/NotAFile
    memcpy(compressed_chunk_data + compressed_chunk_data_offset, data, len);
    compressed_chunk_data_offset += len;
}

void read_PacketChatMessage(void * data, int len) {
    struct PacketChatMessage * p = (struct PacketChatMessage*) data;

    Codepage codepage = CP437; uint8_t * msg = p->message;
    if (*msg == 0xFF) { msg++; codepage = UTF8; }

    char n[32] = {0}; char m[256];
    switch (p->chat_type) {
        case CHAT_ERROR: sound_create(SOUND_LOCAL, &sound_beep2, 0.0F, 0.0F, 0.0F);
        case CHAT_BIG: chat_showpopup(msg, 5.0F, Red, codepage); return;
        case CHAT_INFO: chat_showpopup(msg, 5.0F, White, codepage); return;
        case CHAT_WARNING:
            sound_create(SOUND_LOCAL, &sound_beep1, 0.0F, 0.0F, 0.0F);
            chat_showpopup(msg, 5.0F, Yellow, codepage);
            return;
        case CHAT_SYSTEM:
            if (p->player_id == 255) {
                strncpy(network_custom_reason, msg, 16);
                return; // don’t add message to chat
            }
            m[0] = 0;
            break;
        case CHAT_ALL:
        case CHAT_TEAM:
            if (p->player_id < PLAYERS_MAX && players[p->player_id].connected) {
                switch (players[p->player_id].team) {
                    case TEAM_1: sprintf(n, "%s (%s)", players[p->player_id].name, gamestate.team_1.name); break;
                    case TEAM_2: sprintf(n, "%s (%s)", players[p->player_id].name, gamestate.team_2.name); break;
                    case TEAM_SPECTATOR: sprintf(n, "%s (Spectator)", players[p->player_id].name); break;
                }
                sprintf(m, "%s: ", n);
            } else {
                sprintf(m, ": ");
            }
            break;
    }

    size_t m_remaining = sizeof(m) - 1 - strlen(m);
    size_t body_len = len - offsetof(struct PacketChatMessage, message);
    if (body_len > m_remaining) {
        body_len = m_remaining;
    }

    reencode(m + strlen(m), msg, codepage, UTF8);

    TrueColor color = {0, 0, 0, 255};
    switch (p->chat_type) {
        case CHAT_SYSTEM: color.r = 255; break;
        case CHAT_TEAM: {
            switch (players[p->player_id].connected ? players[p->player_id].team : players[local_player_id].team) {
                case TEAM_1: color.r = gamestate.team_1.red; color.g = gamestate.team_1.green; color.b = gamestate.team_1.blue; break;
                case TEAM_2: color.r = gamestate.team_2.red; color.g = gamestate.team_2.green; color.b = gamestate.team_2.blue; break;
                default: break;
            }
            break;
        }

        default: color.r = color.g = color.b = 255; break;
    }

    chat_add(0, color, m, UTF8);
}

TrueColor white = {0xFF, 0xFF, 0xFF, 0xFF};
void read_PacketBlockAction(void * data, int len) {
    struct PacketBlockAction * p = (struct PacketBlockAction*) data;

    int x = letohs32(p->x), y = letohs32(p->y), z = letohs32(p->z);

    switch (p->action_type) {
        case ACTION_DESTROY:
            if (63 - z > 0) {
                TrueColor col = map_get(x, 63 - z, y);
                map_set(x, 63 - z, y, white);
                map_update_physics(x, 63 - z, y);
                particle_create(col, x + 0.5F, 63 - z + 0.5F, y + 0.5F, 2.5F, 1.0F, 8, 0.1F, 0.25F);
            }
            break;
        case ACTION_GRENADE:
            for (int j = (63 - z) - 1; j <= (63 - z) + 1; j++) {
                for (int k = y - 1; k <= y + 1; k++) {
                    for (int i = x - 1; i <= x + 1; i++) {
                        if (j > 1) {
                            map_set(i, j, k, white);
                            map_update_physics(i, j, k);
                        }
                    }
                }
            }
            break;
        case ACTION_SPADE:
            if ((63 - z - 1) > 1) {
                map_set(x, 63 - z - 1, y, white);
                map_update_physics(x, 63 - z - 1, y);
            }
            if ((63 - z + 0) > 1) {
                TrueColor col = map_get(x, 63 - z, y);
                map_set(x, 63 - z + 0, y, white);
                map_update_physics(x, 63 - z + 0, y);
                particle_create(col, x + 0.5F, 63 - z + 0.5F, y + 0.5F, 2.5F, 1.0F, 8, 0.1F, 0.25F);
            }
            if ((63 - z + 1) > 1) {
                map_set(x, 63 - z + 1, y, white);
                map_update_physics(x, 63 - z + 1, y);
            }
            break;
        case ACTION_BUILD:
            if (p->player_id < PLAYERS_MAX) {
                bool play_sound = map_isair(x, 63 - z, y);

                TrueColor color = {
                    players[p->player_id].block.r,
                    players[p->player_id].block.g,
                    players[p->player_id].block.b,
                    255
                };

                map_set(x, 63 - z, y, color);

                if (play_sound)
                    sound_create(SOUND_WORLD, &sound_build, x + 0.5F, 63 - z + 0.5F, y + 0.5F);
            }
            break;
    }
}

void read_PacketBlockLine(void * data, int len) {
    struct PacketBlockLine * p = (struct PacketBlockLine*) data;
    if (p->player_id >= PLAYERS_MAX) return;

    int sx = letohs32(p->sx), sy = letohs32(p->sy), sz = letohs32(p->sz);
    int ex = letohs32(p->ex), ey = letohs32(p->ey), ez = letohs32(p->ez);

    TrueColor color = {
        players[p->player_id].block.r,
        players[p->player_id].block.g,
        players[p->player_id].block.b,
        255
    };

    if (sx == ex && sy == ey && sz == ez) {
        map_set(sx, 63 - sz, sy, color);
    } else {
        Point blocks[64];
        int len = map_cube_line(sx, sy, sz, ex, ey, ez, blocks);
        while (len > 0) {
            if (map_isair(blocks[len - 1].x, 63 - blocks[len - 1].z, blocks[len - 1].y)) {
                map_set(blocks[len - 1].x, 63 - blocks[len - 1].z, blocks[len - 1].y, color);
            }
            len--;
        }
    }
    sound_create(SOUND_WORLD, &sound_build, (sx + ex) * 0.5F + 0.5F, (63 - sz + 63 - ez) * 0.5F + 0.5F,
                 (sy + ey) * 0.5F + 0.5F);
}

void read_PacketStateData(void * data, int len) {
    struct PacketStateData * p = (struct PacketStateData*) data;

    decodeMagic(gamestate.team_1.name, p->team_1_name, sizeof(gamestate.team_1.name));

    gamestate.team_1.red   = p->team_1_red;
    gamestate.team_1.green = p->team_1_green;
    gamestate.team_1.blue  = p->team_1_blue;

    decodeMagic(gamestate.team_2.name, p->team_2_name, sizeof(gamestate.team_2.name));

    gamestate.team_2.red   = p->team_2_red;
    gamestate.team_2.green = p->team_2_green;
    gamestate.team_2.blue  = p->team_2_blue;

    gamestate.gamemode_type = p->gamemode;

    if (p->gamemode == GAMEMODE_CTF) {
        gamestate.gamemode.ctf.team_1_score  = p->gamemode_data.ctf.team_1_score;
        gamestate.gamemode.ctf.team_2_score  = p->gamemode_data.ctf.team_2_score;
        gamestate.gamemode.ctf.capture_limit = p->gamemode_data.ctf.capture_limit;
        gamestate.gamemode.ctf.intels        = p->gamemode_data.ctf.intels;

        if (p->gamemode_data.ctf.intels & MASKON(TEAM_1_INTEL))
            gamestate.gamemode.ctf.team_1_intel_location.held.player_id = p->gamemode_data.ctf.team_1_intel_location.held.player_id;
        else {
            gamestate.gamemode.ctf.team_1_intel_location.dropped.x = letohf(p->gamemode_data.ctf.team_1_intel_location.dropped.x);
            gamestate.gamemode.ctf.team_1_intel_location.dropped.y = letohf(p->gamemode_data.ctf.team_1_intel_location.dropped.y);
            gamestate.gamemode.ctf.team_1_intel_location.dropped.z = letohf(p->gamemode_data.ctf.team_1_intel_location.dropped.z);
        }

        if (p->gamemode_data.ctf.intels & MASKON(TEAM_2_INTEL))
            gamestate.gamemode.ctf.team_2_intel_location.held.player_id = p->gamemode_data.ctf.team_2_intel_location.held.player_id;
        else {
            gamestate.gamemode.ctf.team_2_intel_location.dropped.x = letohf(p->gamemode_data.ctf.team_2_intel_location.dropped.x);
            gamestate.gamemode.ctf.team_2_intel_location.dropped.y = letohf(p->gamemode_data.ctf.team_2_intel_location.dropped.y);
            gamestate.gamemode.ctf.team_2_intel_location.dropped.z = letohf(p->gamemode_data.ctf.team_2_intel_location.dropped.z);
        }

        gamestate.gamemode.ctf.team_1_base.x = letohf(p->gamemode_data.ctf.team_1_base.x);
        gamestate.gamemode.ctf.team_1_base.y = letohf(p->gamemode_data.ctf.team_1_base.y);
        gamestate.gamemode.ctf.team_1_base.z = letohf(p->gamemode_data.ctf.team_1_base.z);

        gamestate.gamemode.ctf.team_2_base.x = letohf(p->gamemode_data.ctf.team_2_base.x);
        gamestate.gamemode.ctf.team_2_base.y = letohf(p->gamemode_data.ctf.team_2_base.y);
        gamestate.gamemode.ctf.team_2_base.z = letohf(p->gamemode_data.ctf.team_2_base.z);
    }

    else if (p->gamemode == GAMEMODE_TC) {
        gamestate.gamemode.tc.territory_count = p->gamemode_data.tc.territory_count;

        for (size_t i = 0; i < sizeof(gamestate.gamemode.tc.territory) / sizeof(Territory); i++) {
            gamestate.gamemode.tc.territory[i].x    = letohf(p->gamemode_data.tc.territory[i].x);
            gamestate.gamemode.tc.territory[i].y    = letohf(p->gamemode_data.tc.territory[i].y);
            gamestate.gamemode.tc.territory[i].z    = letohf(p->gamemode_data.tc.territory[i].z);
            gamestate.gamemode.tc.territory[i].team = p->gamemode_data.tc.territory[i].team;
        }
    }
    else log_error("Unknown gamemode!");

    sound_create(SOUND_LOCAL, &sound_intro, 0.0F, 0.0F, 0.0F);

    fog_color[0] = p->fog_red   / 255.0F;
    fog_color[1] = p->fog_green / 255.0F;
    fog_color[2] = p->fog_blue  / 255.0F;

    local_player_id       = p->player_id;
    local_player_health   = 100;
    local_player_blocks   = 50;
    local_player_grenades = 3;
    weapon_set(false);

    players[local_player_id].block.r = 111;
    players[local_player_id].block.g = 111;
    players[local_player_id].block.b = 111;

    if (default_team >= 0 && default_gun >= 0) {
        network_join_game(default_team, default_gun);
        screen_current = SCREEN_NONE;
    } else if (default_team == TEAM_SPECTATOR) {
        network_join_game(default_team, WEAPON_RIFLE);
        screen_current = SCREEN_NONE;
    } else if (default_team >= 0) {
        screen_current = SCREEN_GUN_SELECT;
    } else {
        screen_current = SCREEN_TEAM_SELECT;
    }

    camera_mode          = CAMERAMODE_SELECTION;
    network_map_transfer = 0;
    chat_popup_duration  = 0;

    log_info("map data was %i bytes", compressed_chunk_data_offset);
    if (!network_map_cached) {
        int avail_size = 1024 * 1024;
        void * decompressed = malloc(avail_size);
        CHECK_ALLOCATION_ERROR(decompressed)
        size_t decompressed_size;
        struct libdeflate_decompressor * d = libdeflate_alloc_decompressor();
        while (1) {
            int r = libdeflate_zlib_decompress(d, compressed_chunk_data, compressed_chunk_data_offset, decompressed,
                                               avail_size, &decompressed_size);
            // switch not fancy enough here, breaking out of the loop is not aesthetic
            if (r == LIBDEFLATE_INSUFFICIENT_SPACE) {
                avail_size += 1024 * 1024;
                decompressed = realloc(decompressed, avail_size);
                CHECK_ALLOCATION_ERROR(decompressed)
            }
            if (r == LIBDEFLATE_SUCCESS) {
                map_vxl_load(decompressed, decompressed_size);
/*#ifndef USE_TOUCH
                char filename[128];
                sprintf(filename, "cache/%08X.vxl", libdeflate_crc32(0, decompressed, decompressed_size));
                log_info("%s", filename);
                FILE* f = fopen(filename, "wb");
                fwrite(decompressed, 1, decompressed_size, f);
                fclose(f);
#endif*/
                chunk_rebuild_all();
                break;
            }
            if (r == LIBDEFLATE_BAD_DATA || r == LIBDEFLATE_SHORT_OUTPUT)
                break;
        }
        free(decompressed);
        free(compressed_chunk_data);
        libdeflate_free_decompressor(d);
    }
}

void read_PacketFogColor(void * data, int len) {
    struct PacketFogColor * p = (struct PacketFogColor*) data;
    fog_color[0] = p->red   / 255.0F;
    fog_color[1] = p->green / 255.0F;
    fog_color[2] = p->blue  / 255.0F;
}

void read_PacketExistingPlayer(void * data, int len) {
    struct PacketExistingPlayer * p = (struct PacketExistingPlayer*) data;
    if (p->player_id < PLAYERS_MAX) {
        decodeMagic(players[p->player_id].name, p->name, sizeof(players[p->player_id].name));
        if (!players[p->player_id].connected) printJoinMsg(p->team, players[p->player_id].name);

        player_reset(&players[p->player_id]);
        players[p->player_id].connected     = 1;
        players[p->player_id].alive         = 1;
        players[p->player_id].team          = p->team;
        players[p->player_id].weapon        = p->weapon;
        players[p->player_id].held_item     = p->held_item;
        players[p->player_id].score         = letohs32(p->kills);
        players[p->player_id].block.r       = p->red;
        players[p->player_id].block.g       = p->green;
        players[p->player_id].block.b       = p->blue;
        players[p->player_id].ammo          = weapon_ammo(p->weapon);
        players[p->player_id].ammo_reserved = weapon_ammo_reserved(p->weapon);
    }
}

void read_PacketCreatePlayer(void * data, int len) {
    struct PacketCreatePlayer * p = (struct PacketCreatePlayer*) data;
    if (p->player_id < PLAYERS_MAX) {
        decodeMagic(players[p->player_id].name, p->name, sizeof(players[p->player_id].name));
        if (!players[p->player_id].connected) printJoinMsg(p->team, players[p->player_id].name);

        player_reset(&players[p->player_id]);
        players[p->player_id].connected = 1;
        players[p->player_id].alive     = 1;
        players[p->player_id].team      = p->team;
        players[p->player_id].held_item = TOOL_GUN;
        players[p->player_id].weapon    = p->weapon;
        players[p->player_id].pos.x     = letohf(p->x);
        players[p->player_id].pos.y     = 63.0F - letohf(p->z);
        players[p->player_id].pos.z     = letohf(p->y);

        players[p->player_id].orientation.x = players[p->player_id].orientation_smooth.x = (p->team == TEAM_1) ? 1.0F : -1.0F;
        players[p->player_id].orientation.y = players[p->player_id].orientation_smooth.y = 0.0F;
        players[p->player_id].orientation.z = players[p->player_id].orientation_smooth.z = 0.0F;

        players[p->player_id].block.r = 111;
        players[p->player_id].block.g = 111;
        players[p->player_id].block.b = 111;

        players[p->player_id].ammo          = weapon_ammo(p->weapon);
        players[p->player_id].ammo_reserved = weapon_ammo_reserved(p->weapon);

        if (p->player_id == local_player_id) {
            if (p->team == TEAM_SPECTATOR) {
                camera_x = letohf(p->x);
                camera_y = 63.0F - letohf(p->z);
                camera_z = letohf(p->y);
            }

            camera_mode           = (p->team == TEAM_SPECTATOR) ? CAMERAMODE_SPECTATOR : CAMERAMODE_FPS;
            camera_rot_x          = (p->team == TEAM_1) ? 0.5F * PI : 1.5F * PI;
            camera_rot_y          = 0.5F * PI;
            network_logged_in     = 1;
            local_player_health   = 100;
            local_player_blocks   = 50;
            local_player_grenades = 3;
            local_player_lasttool = TOOL_GUN;

            weapon_set(false);
        }
    }
}

void read_PacketPlayerLeft(void * data, int len) {
    struct PacketPlayerLeft * p = (struct PacketPlayerLeft*) data;
    if (p->player_id < PLAYERS_MAX) {
        players[p->player_id].connected = 0;
        players[p->player_id].alive = 0;
        players[p->player_id].score = 0;
        char s[32];
        sprintf(s, "%s disconnected", players[p->player_id].name);
        chat_add(0, Red, s, UTF8);
    }
}

void read_PacketMapStart(void * data, int len) {
    // ffs someone fix the wrong map size of 1.5mb
    compressed_chunk_data_size = 1024 * 1024;
    compressed_chunk_data = malloc(compressed_chunk_data_size);
    CHECK_ALLOCATION_ERROR(compressed_chunk_data)
    compressed_chunk_data_offset = 0;
    network_logged_in = 0;
    network_map_transfer = 1;
    network_map_cached = 0;

    if (len == sizeof(struct PacketMapStart075)) {
        struct PacketMapStart075 * p = (struct PacketMapStart075*) data;
        compressed_chunk_data_estimate = letohu32(p->map_size);
    } else {
        struct PacketMapStart076 * p = (struct PacketMapStart076*) data;
        compressed_chunk_data_estimate = letohu32(p->map_size);
        p->map_name[sizeof(p->map_name) - 1] = 0;
        log_info("map name: %s", p->map_name);
        log_info("map crc32: 0x%08X", p->crc32);

        char filename[128];
        sprintf(filename, "cache/%02X%02X%02X%02X.vxl",
            BYTE0(p->crc32),
            BYTE1(p->crc32),
            BYTE2(p->crc32),
            BYTE3(p->crc32)
        );

        log_info("%s", filename);

        if (file_exists(filename)) {
            network_map_cached = 1;
            void * mapd = file_load(filename);
            map_vxl_load(mapd, file_size(filename));
            free(mapd);
            chunk_rebuild_all();
        }

        struct PacketMapCached c;
        c.cached = network_map_cached;
        network_send(PACKET_MAPCACHED_ID, &c, sizeof(c));
    }

    player_init();
    bullet_traces_reset();
    camera_mode = CAMERAMODE_SELECTION;
}

void read_PacketWorldUpdate(void * data, int len) {
    if (len > 0) {
        int is_075 = (len % sizeof(struct PacketWorldUpdate075) == 0);
        int is_076 = (len % sizeof(struct PacketWorldUpdate076) == 0);

        if (is_075) {
            for (int k = 0; k < (len / sizeof(struct PacketWorldUpdate075)); k++) { // supports up to 256 players
                struct PacketWorldUpdate075 * p
                    = (struct PacketWorldUpdate075*) (data + k * sizeof(struct PacketWorldUpdate075));
                float x = letohf(p->x), y = letohf(p->y), z = letohf(p->z);
                if (players[k].connected && players[k].alive && k != local_player_id) {
                    if (distance3D(players[k].pos.x, players[k].pos.y, players[k].pos.z, x, 63.0F - z, y)
                       > 0.1F * 0.1F) {
                        players[k].pos.x = x;
                        players[k].pos.y = 63.0F - z;
                        players[k].pos.z = y;
                    }
                    players[k].orientation.x = letohf(p->ox);
                    players[k].orientation.y = -letohf(p->oz);
                    players[k].orientation.z = letohf(p->oy);
                }
            }
        } else {
            if (is_076) {
                for (int k = 0; k < (len / sizeof(struct PacketWorldUpdate076)); k++) {
                    struct PacketWorldUpdate076 * p
                        = (struct PacketWorldUpdate076*) (data + k * sizeof(struct PacketWorldUpdate076));
                    float x = letohf(p->x), y = letohf(p->y), z = letohf(p->z);
                    if (players[p->player_id].connected && players[p->player_id].alive
                       && p->player_id != local_player_id) {
                        if (distance3D(players[k].pos.x, players[k].pos.y, players[k].pos.z, x, 63.0F - z, y)
                           > 0.1F * 0.1F) {
                            players[p->player_id].pos.x = x;
                            players[p->player_id].pos.y = 63.0F - z;
                            players[p->player_id].pos.z = y;
                        }
                        players[p->player_id].orientation.x = letohf(p->ox);
                        players[p->player_id].orientation.y = -letohf(p->oz);
                        players[p->player_id].orientation.z = letohf(p->oy);
                    }
                }
            }
        }
    }
}

void read_PacketPositionData(void * data, int len) {
    struct PacketPositionData * p = (struct PacketPositionData*) data;
    players[local_player_id].pos.x = letohf(p->x);
    players[local_player_id].pos.y = 63.0F - letohf(p->z);
    players[local_player_id].pos.z = letohf(p->y);
}

void read_PacketOrientationData(void * data, int len) {
    struct PacketOrientationData * p = (struct PacketOrientationData*) data;
    players[local_player_id].orientation.x = letohf(p->x);
    players[local_player_id].orientation.y = -letohf(p->z);
    players[local_player_id].orientation.z = letohf(p->y);
}

void read_PacketSetColor(void * data, int len) {
    struct PacketSetColor * p = (struct PacketSetColor*) data;
    if (p->player_id < PLAYERS_MAX) {
        players[p->player_id].block.r = p->red;
        players[p->player_id].block.g = p->green;
        players[p->player_id].block.b = p->blue;
    }
}

void read_PacketInputData(void * data, int len) {
    struct PacketInputData * p = (struct PacketInputData*) data;
    if (p->player_id < PLAYERS_MAX) {
        if (p->player_id != local_player_id)
            players[p->player_id].input.keys = p->keys;

        players[p->player_id].physics.jump = (p->keys & MASKON(INPUT_JUMP)) > 0;
    }
}

void read_PacketWeaponInput(void * data, int len) {
    struct PacketWeaponInput * p = (struct PacketWeaponInput*) data;
    if (p->player_id < PLAYERS_MAX && p->player_id != local_player_id) {
        players[p->player_id].input.buttons = p->input;

        if (p->input & MASKON(BUTTON_PRIMARY))
            players[p->player_id].start.lmb = window_time();
        if (p->input & MASKON(BUTTON_SECONDARY))
            players[p->player_id].start.rmb = window_time();
    }
}

void read_PacketSetTool(void * data, int len) {
    struct PacketSetTool * p = (struct PacketSetTool*) data;
    if (p->player_id < PLAYERS_MAX && p->tool < 4) {
        players[p->player_id].held_item = p->tool;
    }
}

void read_PacketKillAction(void * data, int len) {
    struct PacketKillAction * p = (struct PacketKillAction*) data;
    if (p->player_id < PLAYERS_MAX && p->killer_id < PLAYERS_MAX && p->kill_type >= 0 && p->kill_type < 7) {
        if (p->player_id == local_player_id) {
            local_player_death_time = window_time();
            local_player_respawn_time = p->respawn_time;
            local_player_respawn_cnt_last = 255;
            sound_create(SOUND_LOCAL, &sound_death, 0.0F, 0.0F, 0.0F);

            if (p->player_id != p->killer_id) {
                local_player_last_damage_timer = local_player_death_time;
                local_player_last_damage_x = players[p->killer_id].pos.x;
                local_player_last_damage_y = players[p->killer_id].pos.y;
                local_player_last_damage_z = players[p->killer_id].pos.z;
                cameracontroller_death_init(local_player_id, players[p->killer_id].pos.x, players[p->killer_id].pos.y,
                                            players[p->killer_id].pos.z);
            } else {
                cameracontroller_death_init(local_player_id, 0, 0, 0);
            }
        }
        players[p->player_id].alive = 0;
        players[p->player_id].input.keys = 0;
        players[p->player_id].input.buttons = 0;

        if (p->player_id != p->killer_id)
            players[p->killer_id].score++;

        char * gun_name[3] = {"Rifle", "SMG", "Shotgun"};
        char m[256];
        switch (p->kill_type) {
            case KILLTYPE_WEAPON:
                sprintf(m, "%s killed %s (%s)", players[p->killer_id].name, players[p->player_id].name,
                        gun_name[players[p->killer_id].weapon]);
                break;
            case KILLTYPE_HEADSHOT:
                sprintf(m, "%s killed %s (Headshot)", players[p->killer_id].name, players[p->player_id].name);
                break;
            case KILLTYPE_MELEE:
                sprintf(m, "%s killed %s (Spade)", players[p->killer_id].name, players[p->player_id].name);
                break;
            case KILLTYPE_GRENADE:
                sprintf(m, "%s killed %s (Grenade)", players[p->killer_id].name, players[p->player_id].name);
                break;
            case KILLTYPE_FALL: sprintf(m, "%s fell too far", players[p->player_id].name); break;
            case KILLTYPE_TEAMCHANGE: sprintf(m, "%s changed teams", players[p->player_id].name); break;
            case KILLTYPE_CLASSCHANGE: sprintf(m, "%s changed weapons", players[p->player_id].name); break;
        }
        if (p->killer_id == local_player_id || p->player_id == local_player_id) {
            chat_add(1, Red, m, UTF8);
        } else {
            switch (players[p->killer_id].team) {
                case TEAM_1:
                    chat_add(1, (TrueColor) {gamestate.team_1.red, gamestate.team_1.green, gamestate.team_1.blue, 255}, m, UTF8);
                    break;
                case TEAM_2:
                    chat_add(1, (TrueColor) {gamestate.team_2.red, gamestate.team_2.green, gamestate.team_2.blue, 255}, m, UTF8);
                    break;
            }
        }
    }
}

void read_PacketShortPlayerData(void * data, int len) {
    // should never be received
    struct PacketShortPlayerData * p = (struct PacketShortPlayerData*) data;
    log_warn("Unexpected ShortPlayerDataPacket");
}

void read_PacketGrenade(void * data, int len) {
    struct PacketGrenade * p = (struct PacketGrenade*) data;

    grenade_add(&(Grenade) {
        .team        = players[p->player_id].team,
        .fuse_length = letohf(p->fuse_length),
        .pos.x       = letohf(p->x),
        .pos.y       = 63.0F - letohf(p->z),
        .pos.z       = letohf(p->y),
        .velocity.x  = letohf(p->vx),
        .velocity.y  = -letohf(p->vz),
        .velocity.z  = letohf(p->vy),
    });
}

void read_PacketSetHP(void * data, int len) {
    struct PacketSetHP * p = (struct PacketSetHP*) data;
    local_player_health = p->hp;
    if (p->type == DAMAGE_SOURCE_GUN) {
        local_player_last_damage_timer = window_time();
        sound_create(SOUND_LOCAL, &sound_hitplayer, 0.0F, 0.0F, 0.0F);
    }
    local_player_last_damage_x = letohf(p->x);
    local_player_last_damage_y = 63.0F - letohf(p->z);
    local_player_last_damage_z = letohf(p->y);
}

void read_PacketRestock(void * data, int len) {
    struct PacketRestock * p = (struct PacketRestock*) data;
    local_player_health   = 100;
    local_player_blocks   = 50;
    local_player_grenades = 3;
    weapon_set(true);
    sound_create(SOUND_LOCAL, &sound_switch, 0.0F, 0.0F, 0.0F);
}

void read_PacketChangeWeapon(void * data, int len) {
    struct PacketChangeWeapon * p = (struct PacketChangeWeapon*) data;
    if (p->player_id < PLAYERS_MAX) {
        if (p->player_id == local_player_id) {
            log_warn("Unexpected ChangeWeaponPacket");
            return;
        }
        players[p->player_id].weapon = p->weapon;
    }
}

void read_PacketWeaponReload(void * data, int len) {
    struct PacketWeaponReload * p = (struct PacketWeaponReload*) data;
    if (p->player_id == local_player_id) {
        local_player_ammo = p->ammo;
        local_player_ammo_reserved = p->reserved;
    } else {
        sound_create_sticky(weapon_sound_reload(players[p->player_id].weapon), players + p->player_id, p->player_id);
        // dont use values from packet which somehow are never correct
        players[p->player_id].ammo = weapon_ammo(players[p->player_id].weapon);
        players[p->player_id].ammo_reserved = weapon_ammo_reserved(players[p->player_id].weapon);
    }
}

void read_PacketMoveObject(void * data, int len) {
    struct PacketMoveObject * p = (struct PacketMoveObject*) data;
    if (gamestate.gamemode_type == GAMEMODE_CTF) {
        switch (p->object_id) {
            case TEAM_1_BASE:
                gamestate.gamemode.ctf.team_1_base.x = letohf(p->x);
                gamestate.gamemode.ctf.team_1_base.y = letohf(p->y);
                gamestate.gamemode.ctf.team_1_base.z = letohf(p->z);
                break;
            case TEAM_2_BASE:
                gamestate.gamemode.ctf.team_2_base.x = letohf(p->x);
                gamestate.gamemode.ctf.team_2_base.y = letohf(p->y);
                gamestate.gamemode.ctf.team_2_base.z = letohf(p->z);
                break;
            case TEAM_1_FLAG:
                gamestate.gamemode.ctf.intels &= MASKOFF(TEAM_1_INTEL);
                gamestate.gamemode.ctf.team_1_intel_location.dropped.x = letohf(p->x);
                gamestate.gamemode.ctf.team_1_intel_location.dropped.y = letohf(p->y);
                gamestate.gamemode.ctf.team_1_intel_location.dropped.z = letohf(p->z);
                break;
            case TEAM_2_FLAG:
                gamestate.gamemode.ctf.intels &= MASKOFF(TEAM_2_INTEL);
                gamestate.gamemode.ctf.team_2_intel_location.dropped.x = letohf(p->x);
                gamestate.gamemode.ctf.team_2_intel_location.dropped.y = letohf(p->y);
                gamestate.gamemode.ctf.team_2_intel_location.dropped.z = letohf(p->z);
                break;
        }
    }

    if (gamestate.gamemode_type == GAMEMODE_TC && p->object_id < gamestate.gamemode.tc.territory_count) {
        gamestate.gamemode.tc.territory[p->object_id].x    = letohf(p->x);
        gamestate.gamemode.tc.territory[p->object_id].y    = letohf(p->y);
        gamestate.gamemode.tc.territory[p->object_id].z    = letohf(p->z);
        gamestate.gamemode.tc.territory[p->object_id].team = p->team;
    }
}

void read_PacketIntelCapture(void * data, int len) {
    struct PacketIntelCapture * p = (struct PacketIntelCapture*) data;
    if (gamestate.gamemode_type == GAMEMODE_CTF && p->player_id < PLAYERS_MAX) {
        char capture_str[128];
        switch (players[p->player_id].team) {
            case TEAM_1:
                gamestate.gamemode.ctf.team_1_score++;
                sprintf(capture_str, "%s has captured the %s Intel", players[p->player_id].name, gamestate.team_2.name);
                break;
            case TEAM_2:
                gamestate.gamemode.ctf.team_2_score++;
                sprintf(capture_str, "%s has captured the %s Intel", players[p->player_id].name, gamestate.team_1.name);
                break;
        }
        sound_create(SOUND_LOCAL, p->winning ? &sound_horn : &sound_pickup, 0.0F, 0.0F, 0.0F);
        players[p->player_id].score += 10;
        chat_add(0, Red, capture_str, UTF8);
        if (p->winning) {
            char * name = NULL;

            switch (players[p->player_id].team) {
                case TEAM_1: name = gamestate.team_1.name; break;
                case TEAM_2: name = gamestate.team_2.name; break;
            }

            sprintf(capture_str, "%s Team Wins!", name);
            chat_showpopup(capture_str, 5.0F, Red, UTF8);

            gamestate.gamemode.ctf.team_1_score = 0;
            gamestate.gamemode.ctf.team_2_score = 0;
        }
    }
}

void read_PacketIntelDrop(void * data, int len) {
    struct PacketIntelDrop * p = (struct PacketIntelDrop*) data;
    if (gamestate.gamemode_type == GAMEMODE_CTF && p->player_id < PLAYERS_MAX) {
        char drop_str[128];
        switch (players[p->player_id].team) {
            case TEAM_1:
                gamestate.gamemode.ctf.intels &= MASKOFF(TEAM_2_INTEL); // drop opposing team's intel
                gamestate.gamemode.ctf.team_2_intel_location.dropped.x = letohf(p->x);
                gamestate.gamemode.ctf.team_2_intel_location.dropped.y = letohf(p->y);
                gamestate.gamemode.ctf.team_2_intel_location.dropped.z = letohf(p->z);
                sprintf(drop_str, "%s has dropped the %s Intel", players[p->player_id].name, gamestate.team_2.name);
                break;
            case TEAM_2:
                gamestate.gamemode.ctf.intels &= MASKOFF(TEAM_1_INTEL);
                gamestate.gamemode.ctf.team_1_intel_location.dropped.x = letohf(p->x);
                gamestate.gamemode.ctf.team_1_intel_location.dropped.y = letohf(p->y);
                gamestate.gamemode.ctf.team_1_intel_location.dropped.z = letohf(p->z);
                sprintf(drop_str, "%s has dropped the %s Intel", players[p->player_id].name, gamestate.team_1.name);
                break;
        }

        chat_add(0, Red, drop_str, UTF8);
    }
}

void read_PacketIntelPickup(void * data, int len) {
    struct PacketIntelPickup * p = (struct PacketIntelPickup*) data;
    if (gamestate.gamemode_type == GAMEMODE_CTF && p->player_id < PLAYERS_MAX) {
        char pickup_str[128];
        switch (players[p->player_id].team) {
            case TEAM_1:
                gamestate.gamemode.ctf.intels |= MASKON(TEAM_2_INTEL); // pickup opposing team’s intel
                gamestate.gamemode.ctf.team_2_intel_location.held.player_id = p->player_id;
                sprintf(pickup_str, "%s has the %s Intel", players[p->player_id].name, gamestate.team_2.name);
                break;
            case TEAM_2:
                gamestate.gamemode.ctf.intels |= MASKON(TEAM_1_INTEL);
                gamestate.gamemode.ctf.team_1_intel_location.held.player_id = p->player_id;
                sprintf(pickup_str, "%s has the %s Intel", players[p->player_id].name, gamestate.team_1.name);
                break;
        }

        chat_add(0, Red, pickup_str, UTF8);
        sound_create(SOUND_LOCAL, &sound_pickup, 0.0F, 0.0F, 0.0F);
    }
}

void read_PacketTerritoryCapture(void * data, int len) {
    struct PacketTerritoryCapture * p = (struct PacketTerritoryCapture*) data;
    if (gamestate.gamemode_type == GAMEMODE_TC && p->tent < gamestate.gamemode.tc.territory_count) {
        gamestate.gamemode.tc.territory[p->tent].team = p->team;
        sound_create(SOUND_LOCAL, p->winning ? &sound_horn : &sound_pickup, 0.0F, 0.0F, 0.0F);
        char x = (int) (gamestate.gamemode.tc.territory[p->tent].x / 64.0F) + 'A';
        char y = (int) (gamestate.gamemode.tc.territory[p->tent].y / 64.0F) + '1';

        char capture_str[128];
        char * team_n;

        switch (p->team) {
            case TEAM_1: team_n = gamestate.team_1.name; break;
            case TEAM_2: team_n = gamestate.team_2.name; break;
        }

        if (team_n) {
            sprintf(capture_str, "%s have captured %c%c", team_n, x, y);
            chat_add(0, Red, capture_str, UTF8);

            if (p->winning) {
                sprintf(capture_str, "%s Team Wins!", team_n);
                chat_showpopup(capture_str, 5.0F, Red, UTF8);
            }
        }
    }
}

void read_PacketProgressBar(void * data, int len) {
    struct PacketProgressBar * p = (struct PacketProgressBar*) data;
    if (gamestate.gamemode_type == GAMEMODE_TC && p->tent < gamestate.gamemode.tc.territory_count) {
        gamestate.progressbar.progress       = max(min(letohf(p->progress), 1.0F), 0.0F);
        gamestate.progressbar.rate           = p->rate;
        gamestate.progressbar.tent           = p->tent;
        gamestate.progressbar.team_capturing = p->team_capturing;
        gamestate.progressbar.update         = window_time();
    }
}

void read_PacketHandshakeInit(void * data, int len) {
    network_send(PACKET_HANDSHAKERETURN_ID, data, len);
}

#if HACKS_ENABLED
    static const char * operatingsystem = OS " " ARCH " (w/ hacks)";
#else
    static const char * operatingsystem = OS " " ARCH;
#endif

void read_PacketVersionGet(void * data, int len) {
    struct PacketVersionSend ver;
    ver.client   = 'B';
    ver.major    = BETTERSPADES_MAJOR;
    ver.minor    = BETTERSPADES_MINOR;
    ver.revision = BETTERSPADES_PATCH;

    strcpy(ver.operatingsystem, operatingsystem);
    network_send(PACKET_VERSIONSEND_ID, &ver, sizeof(ver) - sizeof(ver.operatingsystem) + strlen(operatingsystem));
}

void read_PacketExtInfo(void * data, int len) {
    struct PacketExtInfo * p = (struct PacketExtInfo*) data;
    if (len >= p->length * sizeof(struct PacketExtInfoEntry) + 1) {
        if (p->length > 0) {
            log_info("Server supports the following extensions:");
            for (int k = 0; k < p->length; k++) {
                log_info("Extension 0x%02X of version %i", p->entries[k].id, p->entries[k].version);
                if (p->entries[k].id >= 192) log_info("(which is packetless)");
            }
        } else log_info("Server does not support extensions");

        struct PacketExtInfo reply;
        reply.length = 5;

        reply.entries[0] = (struct PacketExtInfoEntry) {
            .id      = EXT_PLAYER_PROPERTIES,
            .version = 1,
        };
        reply.entries[1] = (struct PacketExtInfoEntry) {
            .id      = EXT_256PLAYERS,
            .version = 1,
        };
        reply.entries[2] = (struct PacketExtInfoEntry) {
            .id      = EXT_MESSAGES,
            .version = 1,
        };
        reply.entries[3] = (struct PacketExtInfoEntry) {
            .id      = EXT_KICKREASON,
            .version = 1,
        };
        reply.entries[4] = (struct PacketExtInfoEntry) {
            .id      = EXT_TRACE_BULLETS,
            .version = 1,
        };

        network_send(PACKET_EXTINFO_ID, &reply, reply.length * sizeof(struct PacketExtInfoEntry) + 1);
    }
}

void read_PacketPlayerProperties(void * data, int len) {
    struct PacketPlayerProperties * p = (struct PacketPlayerProperties*) data;

    if (len >= sizeof(struct PacketPlayerProperties) && p->subID == 0) {
        players[p->player_id].ammo          = p->ammo_clip;
        players[p->player_id].ammo_reserved = p->ammo_reserved;
        players[p->player_id].score         = letohu32(p->score);

        if (p->player_id == local_player_id) {
            local_player_health        = p->health;
            local_player_blocks        = p->blocks;
            local_player_grenades      = p->grenades;
            local_player_ammo          = p->ammo_clip;
            local_player_ammo_reserved = p->ammo_reserved;
        }
    }
}

void read_PacketBulletTrace(void * data, int len) {
    PacketBulletTrace * p = (PacketBulletTrace*) data;

    size_t index = p->index % MAX_TRACES;

    Trace * trace = &traces[index];

    if (p->origin) { trace->index = p->index; trace->begin = trace->end = 0; }
    if (trace->index != p->index) return;

    trace->data[trace->end].x     = letohf(p->x);
    trace->data[trace->end].y     = 64.0F - letohf(p->z);
    trace->data[trace->end].z     = letohf(p->y);
    trace->data[trace->end].value = letohf(p->value);

    NEXT(trace->end);

    if (trace->begin == trace->end)
        NEXT(trace->begin);
}

void network_updateColor() {
    struct PacketSetColor c;
    c.player_id = local_player_id;
    c.red       = players[local_player_id].block.r;
    c.green     = players[local_player_id].block.g;
    c.blue      = players[local_player_id].block.b;
    network_send(PACKET_SETCOLOR_ID, &c, sizeof(c));
}

unsigned char network_send_tmp[512];
void network_send(int id, void * data, int len) {
    if (network_connected) {
        network_stats[0].outgoing += len + 1;
        network_send_tmp[0] = id;
        memcpy(network_send_tmp + 1, data, len);
        enet_peer_send(peer, 0, enet_packet_create(network_send_tmp, len + 1, ENET_PACKET_FLAG_RELIABLE));
    }
}

unsigned int network_ping() {
    return network_connected ? peer->roundTripTime : 0;
}

void network_disconnect() {
    if (network_connected) {
        enet_peer_disconnect(peer, 0);
        network_connected = 0;
        network_logged_in = 0;

        ENetEvent event;
        while (enet_host_service(client, &event, 3000) > 0) {
            switch (event.type) {
                case ENET_EVENT_TYPE_RECEIVE: enet_packet_destroy(event.packet); break;
                case ENET_EVENT_TYPE_DISCONNECT: return;
                default: break;
            }
        }

        enet_peer_reset(peer);
    }
}

int network_connect_sub(char * ip, int port, int version) {
    ENetAddress address;
    ENetEvent event;
    enet_address_set_host(&address, ip);
    address.port = port;
    peer = enet_host_connect(client, &address, 1, version);
    network_logged_in = 0;
    *network_custom_reason = 0;
    memset(network_stats, 0, sizeof(struct network_stat) * 40);
    if (peer == NULL) return 0;

    if (enet_host_service(client, &event, 2500) > 0 && event.type == ENET_EVENT_TYPE_CONNECT) {
        network_received_packets = 0;
        network_connected = 1;

        float start = window_time();
        while (window_time() - start < 1.0F) { // listen connection for 1s, check if server disconnects
            if (!network_update()) {
                enet_peer_reset(peer);
                return 0;
            }
        }

        return 1;
    }

    chat_showpopup("No response", 3.0F, Red, UTF8);
    enet_peer_reset(peer);
    return 0;
}

int network_connect(char * ip, int port) {
    log_info("Connecting to %s at port %i", ip, port);
    if (network_connected) network_disconnect();

    if (network_connect_sub(ip, port, VERSION_075)) return 1;
    if (network_connect_sub(ip, port, VERSION_076)) return 1;

    network_connected = 0;
    return 0;
}

int network_identifier_split(char * addr, char * ip_out, int * port_out) {
    char * ip_start = strstr(addr, "aos://") + 6;
    if ((size_t) ip_start <= 6) return 0;

    char * port_start = strchr(ip_start, ':');
    *port_out = port_start ? strtoul(port_start + 1, NULL, 10) : 32887;

    if (strchr(ip_start, '.')) {
        if (port_start) {
            strncpy(ip_out, ip_start, port_start - ip_start);
            ip_out[port_start - ip_start] = 0;
        } else {
            strcpy(ip_out, ip_start);
        }
    } else {
        unsigned int ip = strtoul(ip_start, NULL, 10);
        sprintf(ip_out, "%i.%i.%i.%i", ip & 255, (ip >> 8) & 255, (ip >> 16) & 255, (ip >> 24) & 255);
    }

    return 1;
}

int network_connect_string(char * addr) {
    char ip[32]; int port;

    if (!network_identifier_split(addr, ip, &port))
        return 0;

    return network_connect(ip, port);
}

int network_update() {
    if (network_connected) {
        if (window_time() - network_stats_last >= 1.0F) {
            for (int k = 39; k > 0; k--)
                network_stats[k] = network_stats[k - 1];

            network_stats[0].ingoing  = 0;
            network_stats[0].outgoing = 0;
            network_stats[0].avg_ping = network_ping();
            network_stats_last        = window_time();
        }

        ENetEvent event;
        while (enet_host_service(client, &event, 0) > 0) {
            switch (event.type) {
                case ENET_EVENT_TYPE_RECEIVE: {
                    network_stats[0].ingoing += event.packet->dataLength;
                    int id = event.packet->data[0];

                    if (*packets[id]) {
                        log_debug("Packet id %i", id);
                        (*packets[id])(event.packet->data + 1, event.packet->dataLength - 1);
                    } else {
                        log_error("Invalid packet id %i, length: %i", id, (int) event.packet->dataLength - 1);
                    }

                    network_received_packets++;
                    enet_packet_destroy(event.packet);
                    break;
                }

                case ENET_EVENT_TYPE_DISCONNECT: {
                    hud_change(&hud_serverlist);
                    chat_showpopup(network_reason_disconnect(event.data), 10.0F, Red, UTF8);
                    log_error("server disconnected! reason: %s", network_reason_disconnect(event.data));
                    event.peer->data = NULL;
                    network_connected = 0;
                    network_logged_in = 0;

                    return 0;
                }

                default: break;
            }
        }

        if (network_logged_in && players[local_player_id].team != TEAM_SPECTATOR && players[local_player_id].alive) {
            if (players[local_player_id].input.keys != network_keys_last) {
                struct PacketInputData in;
                in.player_id = local_player_id;
                in.keys      = players[local_player_id].input.keys;
                network_send(PACKET_INPUTDATA_ID, &in, sizeof(in));

                network_keys_last = players[local_player_id].input.keys;
            }

            if ((players[local_player_id].input.buttons != network_buttons_last) &&
               !(players[local_player_id].input.keys & MASKON(INPUT_SPRINT))) {
                struct PacketWeaponInput in;
                in.player_id = local_player_id;
                in.input     = players[local_player_id].input.buttons;
                network_send(PACKET_WEAPONINPUT_ID, &in, sizeof(in));

                network_buttons_last = players[local_player_id].input.buttons;
            }

            if (players[local_player_id].held_item != network_tool_last) {
                struct PacketSetTool t;
                t.player_id = local_player_id;
                t.tool      = players[local_player_id].held_item;
                network_send(PACKET_SETTOOL_ID, &t, sizeof(t));

                network_tool_last = players[local_player_id].held_item;
            }

            if (window_time() - network_pos_update > 1.0F
               && distance3D(network_pos_last.x, network_pos_last.y, network_pos_last.z, players[local_player_id].pos.x,
                             players[local_player_id].pos.y, players[local_player_id].pos.z)
                   > 0.01F) {
                network_pos_update = window_time();
                network_pos_last   = players[local_player_id].pos;

                struct PacketPositionData pos;
                pos.x = htolef(players[local_player_id].pos.x);
                pos.y = htolef(players[local_player_id].pos.z);
                pos.z = htolef(63.0F - players[local_player_id].pos.y);
                network_send(PACKET_POSITIONDATA_ID, &pos, sizeof(pos));
            }

            if (window_time() - network_orient_update > (1.0F / 120.0F)
               && angle3D(network_orient_last.x, network_orient_last.y, network_orient_last.z,
                          players[local_player_id].orientation.x, players[local_player_id].orientation.y,
                          players[local_player_id].orientation.z)
                   > 0.5F / 180.0F * PI) {
                network_orient_update = window_time();
                network_orient_last   = players[local_player_id].orientation;

                struct PacketOrientationData orient;
                orient.x = htolef(players[local_player_id].orientation.x);
                orient.y = htolef(players[local_player_id].orientation.z);
                orient.z = htolef(-players[local_player_id].orientation.y);
                network_send(PACKET_ORIENTATIONDATA_ID, &orient, sizeof(orient));
            }
        }
    }

    chunk_queue_blocks();

    return 1;
}

int network_status() {
    return network_connected;
}

void network_init() {
    enet_initialize();
    client = enet_host_create(NULL, 1, 1, 0, 0); // limit bandwidth here if you want to
    enet_host_compress_with_range_coder(client);

    packets[PACKET_POSITIONDATA_ID] = read_PacketPositionData;
    packets[PACKET_ORIENTATIONDATA_ID] = read_PacketOrientationData;
    packets[PACKET_WORLDUPDATE_ID] = read_PacketWorldUpdate;
    packets[PACKET_INPUTDATA_ID] = read_PacketInputData;
    packets[PACKET_WEAPONINPUT_ID] = read_PacketWeaponInput;
    packets[PACKET_SETHP_ID] = read_PacketSetHP;
    packets[PACKET_GRENADE_ID] = read_PacketGrenade;
    packets[PACKET_SETTOOL_ID] = read_PacketSetTool;
    packets[PACKET_SETCOLOR_ID] = read_PacketSetColor;
    packets[PACKET_EXISTINGPLAYER_ID] = read_PacketExistingPlayer;
    packets[PACKET_SHORTPLAYERDATA_ID] = read_PacketShortPlayerData;
    packets[PACKET_MOVEOBJECT_ID] = read_PacketMoveObject;
    packets[PACKET_CREATEPLAYER_ID] = read_PacketCreatePlayer;
    packets[PACKET_BLOCKACTION_ID] = read_PacketBlockAction;
    packets[PACKET_BLOCKLINE_ID] = read_PacketBlockLine;
    packets[PACKET_STATEDATA_ID] = read_PacketStateData;
    packets[PACKET_KILLACTION_ID] = read_PacketKillAction;
    packets[PACKET_CHATMESSAGE_ID] = read_PacketChatMessage;
    packets[PACKET_MAPSTART_ID] = read_PacketMapStart;
    packets[PACKET_MAPCHUNK_ID] = read_PacketMapChunk;
    packets[PACKET_PLAYERLEFT_ID] = read_PacketPlayerLeft;
    packets[PACKET_TERRITORYCAPTURE_ID] = read_PacketTerritoryCapture;
    packets[PACKET_PROGRESSBAR_ID] = read_PacketProgressBar;
    packets[PACKET_INTELCAPTURE_ID] = read_PacketIntelCapture;
    packets[PACKET_INTELPICKUP_ID] = read_PacketIntelPickup;
    packets[PACKET_INTELDROP_ID] = read_PacketIntelDrop;
    packets[PACKET_RESTOCK_ID] = read_PacketRestock;
    packets[PACKET_FOGCOLOR_ID] = read_PacketFogColor;
    packets[PACKET_WEAPONRELOAD_ID] = read_PacketWeaponReload;
    // 29
    packets[PACKET_CHANGEWEAPON_ID] = read_PacketChangeWeapon;
    packets[PACKET_HANDSHAKEINIT_ID] = read_PacketHandshakeInit;
    packets[PACKET_VERSIONGET_ID] = read_PacketVersionGet;
    packets[PACKET_EXTINFO_ID] = read_PacketExtInfo;
    packets[PACKET_EXT_BASE + EXT_PLAYER_PROPERTIES] = read_PacketPlayerProperties;
    packets[PACKET_EXT_BASE + EXT_TRACE_BULLETS] = read_PacketBulletTrace;
}
