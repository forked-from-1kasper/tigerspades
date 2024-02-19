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

#include <string.h>
#include <ctype.h>
#include <math.h>

#include <libdeflate.h>
#include <enet/enet.h>

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
#include <BetterSpades/unicode.h>

void (*packets[256])(uint8_t * data, int len) = {NULL};

int network_connected = 0;
int network_logged_in = 0;
int network_map_transfer = 0;
int network_received_packets = 0;
int network_map_cached = 0;

Vector3f network_pos_last, network_orient_last;

float network_pos_update           = 0.0F;
float network_orient_update        = 0.0F;
unsigned char network_keys_last    = 0;
unsigned char network_buttons_last = 0;
unsigned char network_tool_last    = 255;

uint8_t * compressed_chunk_data;
int compressed_chunk_data_size;
int compressed_chunk_data_offset = 0;
int compressed_chunk_data_estimate = 0;

NetworkStat network_stats[40];
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

    char buff[64]; sprintf(buff, "%s joined the %s team", name, t);
    chat_add(0, Red, buff, sizeof(buff), UTF8);
}

static uint8_t network_buffer[512];

static void network_send(int id, int len) {
    if (network_connected) {
        network_stats[0].outgoing += len + 1;
        network_buffer[0] = id;
        enet_peer_send(peer, 0, enet_packet_create(network_buffer, len + 1, ENET_PACKET_FLAG_RELIABLE));
    }
}

void network_join_game(unsigned char team, unsigned char weapon) {
    char namebuff[17]; encodeMagic(namebuff, sizeof(namebuff), settings.name, sizeof(settings.name));

    PacketExistingPlayer contained;
    contained.player_id = local_player.id;
    contained.team      = team;
    contained.weapon    = weapon;
    contained.held_item = TOOL_GUN;
    contained.kills     = 0;
    contained.color     = players[local_player.id].block;
    contained.name      = namebuff;

    sendPacketExistingPlayer(&contained, strsize(namebuff, sizeof(namebuff)));
}

#define PACKET_INCOMPLETE 0
#define PACKET_EXTRA      0
#define PACKET_SERVERSIDE 0
#define begin(T) void send##T(T * contained, size_t len) \
                 { write##T(network_buffer + 1, contained); \
                   network_send(id##T, size##T + len); }
#include <AceOfSpades/packets.h>

#define READPACKET(T, contained, src) T contained; read##T(src, &contained)

void getPacketPositionData(uint8_t * data, int len) {
    READPACKET(PacketPositionData, p, data);
    players[local_player.id].pos = ntohv3f(p.pos);
}

void getPacketOrientationData(uint8_t * data, int len) {
    READPACKET(PacketOrientationData, p, data);
    players[local_player.id].orientation = ntohov3f(p.orient);
}

void getPacketInputData(uint8_t * data, int len) {
    READPACKET(PacketInputData, p, data);

    if (p.player_id < PLAYERS_MAX) {
        if (p.player_id != local_player.id)
            players[p.player_id].input.keys = p.keys;

        players[p.player_id].physics.jump = HASBIT(p.keys, INPUT_JUMP) > 0;
    }
}

void getPacketWeaponInput(uint8_t * data, int len) {
    READPACKET(PacketWeaponInput, p, data);

    if (p.player_id < PLAYERS_MAX && p.player_id != local_player.id) {
        players[p.player_id].input.buttons = p.input;

        float time = window_time();

        if (HASBIT(p.input, BUTTON_PRIMARY))
            players[p.player_id].start.lmb = time;
        if (HASBIT(p.input, BUTTON_SECONDARY))
            players[p.player_id].start.rmb = time;
    }
}

void handlePacketGrenade(PacketGrenade * p) {
    grenade_add(&(Grenade) {
        .team        = players[p->player_id].team,
        .fuse_length = p->fuse_length,
        .pos         = ntohv3f(p->pos),
        .velocity    = ntohov3f(p->vel),
    });
}

void getPacketGrenade(uint8_t * data, int len) {
    READPACKET(PacketGrenade, p, data);
    handlePacketGrenade(&p);
}

void getPacketSetTool(uint8_t * data, int len) {
    READPACKET(PacketSetTool, p, data);

    if (p.player_id < PLAYERS_MAX && p.tool < 4)
        players[p.player_id].held_item = TOOL(p.tool);
}

void getPacketSetColor(uint8_t * data, int len) {
    READPACKET(PacketSetColor, p, data);

    if (p.player_id < PLAYERS_MAX)
        players[p.player_id].block = p.color;
}

void getPacketExistingPlayer(uint8_t * data, int len) {
    READPACKET(PacketExistingPlayer, p, data);

    if (p.player_id < PLAYERS_MAX) {
        decodeMagic(players[p.player_id].name, sizeof(players[p.player_id].name), p.name, len - sizePacketExistingPlayer);

        if (!players[p.player_id].connected) printJoinMsg(p.team, players[p.player_id].name);

        player_reset(&players[p.player_id]);
        players[p.player_id].connected     = 1;
        players[p.player_id].alive         = 1;
        players[p.player_id].team          = TEAM(p.team);
        players[p.player_id].weapon        = WEAPON(p.weapon);
        players[p.player_id].held_item     = TOOL(p.held_item);
        players[p.player_id].score         = p.kills;
        players[p.player_id].block         = p.color;
        players[p.player_id].ammo          = weapon_ammo(p.weapon);
        players[p.player_id].ammo_reserved = weapon_ammo_reserved(p.weapon);
    }
}

void getPacketBlockAction(uint8_t * data, int len) {
    READPACKET(PacketBlockAction, p, data);
    int x = p.pos.x, y = p.pos.y, z = p.pos.z;

    switch (p.action_type) {
        case ACTION_DESTROY: {
            if (63 - z > 0) {
                map_set(x, 63 - z, y, White);
                map_update_physics(x, 63 - z, y);

                TrueColor col = map_get(x, 63 - z, y);
                particle_create(col, x + 0.5F, 63 - z + 0.5F, y + 0.5F, 2.5F, 1.0F, 8, 0.1F, 0.25F);
            }
            break;
        }

        case ACTION_GRENADE: {
            for (int j = (63 - z) - 1; j <= (63 - z) + 1; j++) {
                for (int k = y - 1; k <= y + 1; k++) {
                    for (int i = x - 1; i <= x + 1; i++) {
                        if (j > 1) {
                            map_set(i, j, k, White);
                            map_update_physics(i, j, k);
                        }
                    }
                }
            }
            break;
        }

        case ACTION_SPADE: {
            if ((63 - z - 1) > 1) {
                map_set(x, 63 - z - 1, y, White);
                map_update_physics(x, 63 - z - 1, y);
            }
            if ((63 - z + 0) > 1) {
                map_set(x, 63 - z + 0, y, White);
                map_update_physics(x, 63 - z + 0, y);

                TrueColor col = map_get(x, 63 - z, y);
                particle_create(col, x + 0.5F, 63 - z + 0.5F, y + 0.5F, 2.5F, 1.0F, 8, 0.1F, 0.25F);
            }
            if ((63 - z + 1) > 1) {
                map_set(x, 63 - z + 1, y, White);
                map_update_physics(x, 63 - z + 1, y);
            }
            break;
        }

        case ACTION_BUILD: {
            if (p.player_id < PLAYERS_MAX) {
                bool play_sound = map_isair(x, 63 - z, y);

                TrueColor color = {
                    players[p.player_id].block.r,
                    players[p.player_id].block.g,
                    players[p.player_id].block.b,
                    255
                };

                map_set(x, 63 - z, y, color);

                if (play_sound)
                    sound_create(SOUND_WORLD, &sound_build, x + 0.5F, 63 - z + 0.5F, y + 0.5F);
            }
            break;
        }
    }
}

void getPacketBlockLine(uint8_t * data, int len) {
    READPACKET(PacketBlockLine, p, data);

    if (p.player_id >= PLAYERS_MAX) return;

    int sx = p.start.x, sy = p.start.y, sz = p.start.z;
    int ex = p.end.x,   ey = p.end.y,   ez = p.end.z;

    TrueColor color = {
        players[p.player_id].block.r,
        players[p.player_id].block.g,
        players[p.player_id].block.b,
        255
    };

    if (sx == ex && sy == ey && sz == ez) {
        map_set(sx, 63 - sz, sy, color);
    } else {
        Vector3i blocks[64];
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

void getPacketChatMessage(uint8_t * data, int len) {
    READPACKET(PacketChatMessage, p, data);

    size_t size = len - sizePacketChatMessage;

    Codepage codepage = CP437; char * msg = p.message;
    if (*((uint8_t *) msg) == 0xFF) { msg++; size--; codepage = UTF8; }

    char n[32] = {0}; char m[256];
    switch (p.chat_type) {
        case CHAT_ERROR: sound_create(SOUND_LOCAL, &sound_beep2, 0.0F, 0.0F, 0.0F);
        case CHAT_BIG: chat_showpopup(msg, size, codepage, 5.0F, Red); return;
        case CHAT_INFO: chat_showpopup(msg, size, codepage, 5.0F, White); return;
        case CHAT_WARNING:
            sound_create(SOUND_LOCAL, &sound_beep1, 0.0F, 0.0F, 0.0F);
            chat_showpopup(msg, size, codepage, 5.0F, Yellow);
            return;
        case CHAT_SYSTEM:
            if (p.player_id == 255) {
                strncpy(network_custom_reason, msg, 16);
                return; // don’t add message to chat
            }
            m[0] = 0;
            break;
        case CHAT_ALL:
        case CHAT_TEAM:
            if (p.player_id < PLAYERS_MAX && players[p.player_id].connected) {
                switch (players[p.player_id].team) {
                    case TEAM_1: sprintf(n, "%s (%s)", players[p.player_id].name, gamestate.team_1.name); break;
                    case TEAM_2: sprintf(n, "%s (%s)", players[p.player_id].name, gamestate.team_2.name); break;
                    case TEAM_SPECTATOR: sprintf(n, "%s (Spectator)", players[p.player_id].name); break;
                }
                sprintf(m, "%s: ", n);
            } else {
                sprintf(m, ": ");
            }
            break;
    }

    {
        size_t offset = strlen(m);
        convert(m + offset, sizeof(m) - offset, UTF8, msg, size, codepage);
    }

    TrueColor color = {0, 0, 0, 255};
    switch (p.chat_type) {
        case CHAT_SYSTEM: color.r = 255; break;
        case CHAT_TEAM: {
            switch (players[p.player_id].connected ? players[p.player_id].team : players[local_player.id].team) {
                case TEAM_1: color.r = gamestate.team_1.color.r; color.g = gamestate.team_1.color.g; color.b = gamestate.team_1.color.b; break;
                case TEAM_2: color.r = gamestate.team_2.color.r; color.g = gamestate.team_2.color.g; color.b = gamestate.team_2.color.b; break;
                default: break;
            }
            break;
        }

        default: color.r = color.g = color.b = 255; break;
    }

    chat_add(0, color, m, sizeof(m), UTF8);
}

static inline void addExtInfoEntry(uint8_t id, uint8_t version, size_t * index) {
    PacketExtInfoEntry extension;
    extension.id      = id;
    extension.version = version;

    size_t offset = 1 + sizePacketExtInfo + *index; // skip packet id byte & header
    *index += writePacketExtInfoEntry(network_buffer + offset, &extension);
}

void getPacketExtInfo(uint8_t * data, int len) {
    READPACKET(PacketExtInfo, p, data);

    if (len >= sizePacketExtInfo + p.length * sizePacketExtInfoEntry) {
        if (p.length > 0) {
            PacketExtInfoEntry ext;

            log_info("Server supports the following extensions:");
            for (int k = 0; k < p.length; k++) {
                readPacketExtInfoEntry(data + sizePacketExtInfo + k * sizePacketExtInfoEntry, &ext);

                log_info("Extension 0x%02X of version %i", ext.id, ext.version);
                if (ext.id >= 192) log_info("(which is packetless)");
            }
        } else log_info("Server does not support extensions");

        size_t index = 0, length = 0;

        addExtInfoEntry(EXT_PLAYER_PROPERTIES, 1, &index); length++;
        addExtInfoEntry(EXT_256PLAYERS,        1, &index); length++;
        addExtInfoEntry(EXT_MESSAGES,          1, &index); length++;
        addExtInfoEntry(EXT_KICKREASON,        1, &index); length++;
        addExtInfoEntry(EXT_TRACE_BULLETS,     1, &index); length++;

        PacketExtInfo reply; reply.length = length;
        sendPacketExtInfo(&reply, index);
    }
}

void getPacketWorldUpdate(uint8_t * data, int len) {
    if (len > 0) {
        bool is075 = (len % sizePacketWorldUpdate075) == 0;
        bool is076 = (len % sizePacketWorldUpdate076) == 0;

        size_t index = 0;

        if (is075) {
            for (int k = 0; k < len / sizePacketWorldUpdate075; k++) { // supports up to 256 players
                PacketWorldUpdate075 p; index += readPacketWorldUpdate075(data + index, &p);

                if (players[k].connected && players[k].alive && k != local_player.id) {
                    Vector3f r = ntohv3f(p.pos);

                    if (normv3f(players[k].pos, r) > 0.01F)
                        players[k].pos = r;

                    players[k].orientation = ntohov3f(p.orient);
                }
            }
        } else if (is076) {
            for (int k = 0; k < len / sizePacketWorldUpdate076; k++) {
                PacketWorldUpdate076 p; index += readPacketWorldUpdate076(data + index, &p);

                if (players[p.player_id].connected && players[p.player_id].alive && p.player_id != local_player.id) {
                    Vector3f r = ntohv3f(p.pos);

                    if (normv3f(players[k].pos, r) > 0.01F)
                        players[p.player_id].pos = r;

                    players[p.player_id].orientation = ntohov3f(p.orient);
                }
            }
        }
    }
}

void getPacketSetHP(uint8_t * data, int len) {
    READPACKET(PacketSetHP, p, data);

    local_player.health = p.hp;

    if (p.type == DAMAGE_SOURCE_GUN) {
        local_player.last_damage_timer = window_time();
        sound_create(SOUND_LOCAL, &sound_hitplayer, 0.0F, 0.0F, 0.0F);
    }

    local_player.last_damage = ntohv3f(p.pos);
}

void getPacketShortPlayerData(uint8_t * data, int len) {
    // should never be received
    log_warn("Unexpected ShortPlayerDataPacket");
}

void getPacketMoveObject(uint8_t * data, int len) {
    READPACKET(PacketMoveObject, p, data);

    if (gamestate.gamemode_type == GAMEMODE_CTF) {
        switch (p.object_id) {
            case TEAM_1_BASE: gamestate.gamemode.ctf.team_1_base = p.pos; break;
            case TEAM_2_BASE: gamestate.gamemode.ctf.team_2_base = p.pos; break;
            case TEAM_1_FLAG: {
                gamestate.gamemode.ctf.intels &= MASKOFF(TEAM_1_INTEL);
                gamestate.gamemode.ctf.team_1_intel_location.dropped = p.pos;
                break;
            }
            case TEAM_2_FLAG: {
                gamestate.gamemode.ctf.intels &= MASKOFF(TEAM_2_INTEL);
                gamestate.gamemode.ctf.team_2_intel_location.dropped = p.pos;
                break;
            }
        }
    }

    if (gamestate.gamemode_type == GAMEMODE_TC && p.object_id < gamestate.gamemode.tc.territory_count) {
        gamestate.gamemode.tc.territory[p.object_id].pos  = p.pos;
        gamestate.gamemode.tc.territory[p.object_id].team = TEAM(p.team);
    }
}

void getPacketCreatePlayer(uint8_t * data, int len) {
    READPACKET(PacketCreatePlayer, p, data);

    if (p.player_id < PLAYERS_MAX) {
        decodeMagic(players[p.player_id].name, sizeof(players[p.player_id].name), p.name, len - sizePacketCreatePlayer);

        if (!players[p.player_id].connected) printJoinMsg(p.team, players[p.player_id].name);

        player_reset(&players[p.player_id]);
        players[p.player_id].connected = 1;
        players[p.player_id].alive     = 1;
        players[p.player_id].team      = TEAM(p.team);
        players[p.player_id].held_item = TOOL_GUN;
        players[p.player_id].weapon    = WEAPON(p.weapon);
        players[p.player_id].pos       = ntohv3f(p.pos);

        Vector3f o = {p.team == TEAM_1 ? 1.0F : -1.0F, 0.0F, 0.0F};

        players[p.player_id].orientation        = o;
        players[p.player_id].orientation_smooth = o;

        players[p.player_id].block.r = 111;
        players[p.player_id].block.g = 111;
        players[p.player_id].block.b = 111;

        players[p.player_id].ammo          = weapon_ammo(p.weapon);
        players[p.player_id].ammo_reserved = weapon_ammo_reserved(p.weapon);

        if (p.player_id == local_player.id) {
            if (p.team == TEAM_SPECTATOR)
                camera.pos = ntohv3f(p.pos);

            camera.mode            = (p.team == TEAM_SPECTATOR) ? CAMERAMODE_SPECTATOR : CAMERAMODE_FPS;
            camera.rot.x           = (p.team == TEAM_1) ? 0.5F * PI : 1.5F * PI;
            camera.rot.y           = 0.5F * PI;
            network_logged_in      = 1;
            local_player.health    = 100;
            local_player.blocks    = 50;
            local_player.grenades  = 3;
            local_player.last_tool = TOOL_GUN;

            weapon_set(false);
        }
    }
}

static inline uint8_t readu8le(uint8_t * buff)
{ size_t dummy = 0; return getu8le(buff, &dummy); }

static inline Vector3f readv3f(uint8_t * buff)
{ size_t dummy = 0; return getv3f(buff, &dummy); }

void getPacketStateData(uint8_t * data, int len) {
    PacketStateData p; size_t index = readPacketStateData(data, &p);

    decodeMagic(gamestate.team_1.name, sizeof(gamestate.team_1.name), (char *) p.team_1_name.data, p.team_1_name.size);
    gamestate.team_1.color = p.team_1;

    decodeMagic(gamestate.team_2.name, sizeof(gamestate.team_2.name), (char *) p.team_2_name.data, p.team_2_name.size);
    gamestate.team_2.color = p.team_2;

    gamestate.gamemode_type = p.gamemode;

    if (p.gamemode == GAMEMODE_CTF) {
        CTFStateData ctf; index += readCTFStateData(data + index, &ctf);
        gamestate.gamemode.ctf.team_1_score  = ctf.team_1_score;
        gamestate.gamemode.ctf.team_2_score  = ctf.team_2_score;
        gamestate.gamemode.ctf.capture_limit = ctf.capture_limit;
        gamestate.gamemode.ctf.intels        = ctf.intels;

        if (HASBIT(ctf.intels, TEAM_1_INTEL))
            gamestate.gamemode.ctf.team_1_intel_location.held = readu8le(ctf.team_1_intel_location.data);
        else
            gamestate.gamemode.ctf.team_1_intel_location.dropped = readv3f(ctf.team_1_intel_location.data);

        if (HASBIT(ctf.intels, TEAM_2_INTEL))
            gamestate.gamemode.ctf.team_2_intel_location.held = readu8le(ctf.team_2_intel_location.data);
        else
            gamestate.gamemode.ctf.team_2_intel_location.dropped = readv3f(ctf.team_2_intel_location.data);

        gamestate.gamemode.ctf.team_1_base = ctf.team_1_base;
        gamestate.gamemode.ctf.team_2_base = ctf.team_2_base;
    } else if (p.gamemode == GAMEMODE_TC) {
        TCStateData tc; index += readTCStateData(data + index, &tc);
        gamestate.gamemode.tc.territory_count = tc.territory_count;

        for (size_t i = 0; i < tc.territory_count; i++) {
            TCTerritory territory; index += readTCTerritory(data + index, &territory);

            gamestate.gamemode.tc.territory[i].pos  = territory.pos;
            gamestate.gamemode.tc.territory[i].team = territory.team;
        }
    } else log_error("Unknown gamemode (%d)!", p.gamemode);

    sound_create(SOUND_LOCAL, &sound_intro, 0.0F, 0.0F, 0.0F);

    fog_color[0] = ((float) p.fog.r) / 255.0F;
    fog_color[1] = ((float) p.fog.g) / 255.0F;
    fog_color[2] = ((float) p.fog.b) / 255.0F;

    local_player.id       = p.player_id;
    local_player.health   = 100;
    local_player.blocks   = 50;
    local_player.grenades = 3;
    weapon_set(false);

    players[local_player.id].block.r = 111;
    players[local_player.id].block.g = 111;
    players[local_player.id].block.b = 111;

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

    camera.mode          = CAMERAMODE_SELECTION;
    network_map_transfer = 0;
    chat_popup_duration  = 0;

    log_info("map data was %i bytes", compressed_chunk_data_offset);
    if (!network_map_cached) {
        int avail_size = 1024 * 1024;
        void * decompressed = malloc(avail_size);
        CHECK_ALLOCATION_ERROR(decompressed)
        size_t decompressed_size;
        struct libdeflate_decompressor * d = libdeflate_alloc_decompressor();

        for (;;) {
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
//#ifndef USE_TOUCH
                char filename[128];
                sprintf(filename, "cache/%08X.vxl", libdeflate_crc32(0, decompressed, decompressed_size));
                log_info("%s", filename);
                FILE* f = fopen(filename, "wb");
                fwrite(decompressed, 1, decompressed_size, f);
                fclose(f);
//#endif
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

void player_reset_toggleable_input() {
    if (config_key(WINDOW_KEY_CROUCH)->toggle)
        window_pressed_keys[WINDOW_KEY_CROUCH] = 0;

    if (config_key(WINDOW_KEY_SPRINT)->toggle)
        window_pressed_keys[WINDOW_KEY_SPRINT] = 0;
}

void getPacketKillAction(uint8_t * data, int len) {
    READPACKET(PacketKillAction, p, data);

    if (p.player_id < PLAYERS_MAX && p.killer_id < PLAYERS_MAX && p.kill_type >= 0 && p.kill_type < 7) {
        if (p.player_id == local_player.id) {
            player_reset_toggleable_input();

            local_player.death_time       = window_time();
            local_player.respawn_time     = p.respawn_time;
            local_player.respawn_cnt_last = 255;
            sound_create(SOUND_LOCAL, &sound_death, 0.0F, 0.0F, 0.0F);

            if (p.player_id != p.killer_id) {
                local_player.last_damage_timer = local_player.death_time;
                local_player.last_damage       = players[p.killer_id].pos;

                cameracontroller_death_init(local_player.id, players[p.killer_id].pos);
            } else {
                cameracontroller_death_init(local_player.id, (Vector3f) {0.0F, 0.0F, 0.0F});
            }
        }

        players[p.player_id].alive         = 0;
        players[p.player_id].input.keys    = 0;
        players[p.player_id].input.buttons = 0;

        if (p.player_id != p.killer_id)
            players[p.killer_id].score++;

        char * gun_name[3] = {"Rifle", "SMG", "Shotgun"};
        char m[256];
        switch (p.kill_type) {
            case KILLTYPE_WEAPON:
                sprintf(m, "%s killed %s (%s)", players[p.killer_id].name, players[p.player_id].name,
                        gun_name[players[p.killer_id].weapon]);
                break;
            case KILLTYPE_HEADSHOT:
                sprintf(m, "%s killed %s (Headshot)", players[p.killer_id].name, players[p.player_id].name);
                break;
            case KILLTYPE_MELEE:
                sprintf(m, "%s killed %s (Spade)", players[p.killer_id].name, players[p.player_id].name);
                break;
            case KILLTYPE_GRENADE:
                sprintf(m, "%s killed %s (Grenade)", players[p.killer_id].name, players[p.player_id].name);
                break;
            case KILLTYPE_FALL: sprintf(m, "%s fell too far", players[p.player_id].name); break;
            case KILLTYPE_TEAMCHANGE: sprintf(m, "%s changed teams", players[p.player_id].name); break;
            case KILLTYPE_CLASSCHANGE: sprintf(m, "%s changed weapons", players[p.player_id].name); break;
        }

        if (p.killer_id == local_player.id || p.player_id == local_player.id) {
            chat_add(1, Red, m, sizeof(m), UTF8);
        } else {
            switch (players[p.killer_id].team) {
                case TEAM_1: {
                    chat_add(1, (TrueColor) {gamestate.team_1.color.r, gamestate.team_1.color.g, gamestate.team_1.color.b, 255}, m, sizeof(m), UTF8);
                    break;
                }

                case TEAM_2: {
                    chat_add(1, (TrueColor) {gamestate.team_2.color.r, gamestate.team_2.color.g, gamestate.team_2.color.b, 255}, m, sizeof(m), UTF8);
                    break;
                }
            }
        }
    }
}

void getPacketMapStart(uint8_t * data, int len) {
    // ffs someone fix the wrong map size of 1.5mb
    compressed_chunk_data_size = 1024 * 1024;
    compressed_chunk_data      = malloc(compressed_chunk_data_size);
    CHECK_ALLOCATION_ERROR(compressed_chunk_data)
    compressed_chunk_data_offset = 0;
    network_logged_in            = 0;
    network_map_transfer         = 1;
    network_map_cached           = 0;

    if (len == sizePacketMapStart075) {
        READPACKET(PacketMapStart075, p, data);
        compressed_chunk_data_estimate = p.map_size;
    } else {
        READPACKET(PacketMapStart076, p, data);
        compressed_chunk_data_estimate = p.map_size;

        data[len - 1] = 0;
        log_info("map name: %s", p.map_name);
        log_info("map crc32: 0x%08X", p.crc32);

        char filename[128];
        sprintf(filename, "cache/%02X%02X%02X%02X.vxl",
            BYTE0(p.crc32),
            BYTE1(p.crc32),
            BYTE2(p.crc32),
            BYTE3(p.crc32)
        );

        log_info("%s", filename);

        if (file_exists(filename)) {
            network_map_cached = 1;
            void * mapd = file_load(filename);
            map_vxl_load(mapd, file_size(filename));
            free(mapd);
            chunk_rebuild_all();
        }

        PacketMapCached reply; reply.cached = network_map_cached;
        sendPacketMapCached(&reply, 0);
    }

    trajectories_reset();

    player_init(); camera.mode = CAMERAMODE_SELECTION;
}

void getPacketMapChunk(uint8_t * data, int len) {
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

void getPacketPlayerLeft(uint8_t * data, int len) {
    READPACKET(PacketPlayerLeft, p, data);

    if (p.player_id < PLAYERS_MAX) {
        players[p.player_id].connected = 0;
        players[p.player_id].alive     = 0;
        players[p.player_id].score     = 0;

        char buff[32]; sprintf(buff, "%s disconnected", players[p.player_id].name);
        chat_add(0, Red, buff, sizeof(buff), UTF8);
    }
}

void getPacketTerritoryCapture(uint8_t * data, int len) {
    READPACKET(PacketTerritoryCapture, p, data);

    if (gamestate.gamemode_type == GAMEMODE_TC && p.tent < gamestate.gamemode.tc.territory_count) {
        gamestate.gamemode.tc.territory[p.tent].team = TEAM(p.team);
        sound_create(SOUND_LOCAL, p.winning ? &sound_horn : &sound_pickup, 0.0F, 0.0F, 0.0F);
        char x = (int) (gamestate.gamemode.tc.territory[p.tent].pos.x / 64.0F) + 'A';
        char y = (int) (gamestate.gamemode.tc.territory[p.tent].pos.y / 64.0F) + '1';

        char * team_name = NULL;

        switch (p.team) {
            case TEAM_1: team_name = gamestate.team_1.name; break;
            case TEAM_2: team_name = gamestate.team_2.name; break;
        }

        if (team_name != NULL) {
            char capture_str[128];

            sprintf(capture_str, "%s have captured %c%c", team_name, x, y);
            chat_add(0, Red, capture_str, sizeof(capture_str), UTF8);

            if (p.winning) {
                sprintf(capture_str, "%s Team Wins!", team_name);
                chat_showpopup(capture_str, sizeof(capture_str), UTF8, 5.0F, Red);
            }
        }
    }
}

void getPacketProgressBar(uint8_t * data, int len) {
    READPACKET(PacketProgressBar, p, data);

    if (gamestate.gamemode_type == GAMEMODE_TC && p.tent < gamestate.gamemode.tc.territory_count) {
        gamestate.progressbar.progress       = clamp(0.0F, 1.0F, p.progress);
        gamestate.progressbar.rate           = p.rate;
        gamestate.progressbar.tent           = p.tent;
        gamestate.progressbar.team_capturing = p.team_capturing;
        gamestate.progressbar.update         = window_time();
    }
}

void getPacketIntelCapture(uint8_t * data, int len) {
    READPACKET(PacketIntelCapture, p, data);

    if (gamestate.gamemode_type == GAMEMODE_CTF && p.player_id < PLAYERS_MAX) {
        char capture_str[128];
        switch (players[p.player_id].team) {
            case TEAM_1:
                gamestate.gamemode.ctf.team_1_score++;
                sprintf(capture_str, "%s has captured the %s Intel", players[p.player_id].name, gamestate.team_2.name);
                break;
            case TEAM_2:
                gamestate.gamemode.ctf.team_2_score++;
                sprintf(capture_str, "%s has captured the %s Intel", players[p.player_id].name, gamestate.team_1.name);
                break;
        }
        sound_create(SOUND_LOCAL, p.winning ? &sound_horn : &sound_pickup, 0.0F, 0.0F, 0.0F);
        players[p.player_id].score += 10;
        chat_add(0, Red, capture_str, sizeof(capture_str), UTF8);

        if (p.winning) {
            char * name = NULL;

            switch (players[p.player_id].team) {
                case TEAM_1: name = gamestate.team_1.name; break;
                case TEAM_2: name = gamestate.team_2.name; break;
            }

            sprintf(capture_str, "%s Team Wins!", name);
            chat_showpopup(capture_str, sizeof(capture_str), UTF8, 5.0F, Red);

            gamestate.gamemode.ctf.team_1_score = 0;
            gamestate.gamemode.ctf.team_2_score = 0;
        }
    }
}

void getPacketIntelPickup(uint8_t * data, int len) {
    READPACKET(PacketIntelPickup, p, data);

    if (gamestate.gamemode_type == GAMEMODE_CTF && p.player_id < PLAYERS_MAX) {
        char pickup_str[128];
        switch (players[p.player_id].team) {
            case TEAM_1: {
                gamestate.gamemode.ctf.intels |= MASKON(TEAM_2_INTEL); // pickup opposing team’s intel
                gamestate.gamemode.ctf.team_2_intel_location.held = p.player_id;
                sprintf(pickup_str, "%s has the %s Intel", players[p.player_id].name, gamestate.team_2.name);
                break;
            }

            case TEAM_2: {
                gamestate.gamemode.ctf.intels |= MASKON(TEAM_1_INTEL);
                gamestate.gamemode.ctf.team_1_intel_location.held = p.player_id;
                sprintf(pickup_str, "%s has the %s Intel", players[p.player_id].name, gamestate.team_1.name);
                break;
            }
        }

        chat_add(0, Red, pickup_str, sizeof(pickup_str), UTF8);
        sound_create(SOUND_LOCAL, &sound_pickup, 0.0F, 0.0F, 0.0F);
    }
}

void getPacketIntelDrop(uint8_t * data, int len) {
    READPACKET(PacketIntelDrop, p, data);

    if (gamestate.gamemode_type == GAMEMODE_CTF && p.player_id < PLAYERS_MAX) {
        char drop_str[128];
        switch (players[p.player_id].team) {
            case TEAM_1:
                gamestate.gamemode.ctf.intels &= MASKOFF(TEAM_2_INTEL); // drop opposing team's intel
                gamestate.gamemode.ctf.team_2_intel_location.dropped = p.pos;
                sprintf(drop_str, "%s has dropped the %s Intel", players[p.player_id].name, gamestate.team_2.name);
                break;
            case TEAM_2:
                gamestate.gamemode.ctf.intels &= MASKOFF(TEAM_1_INTEL);
                gamestate.gamemode.ctf.team_1_intel_location.dropped = p.pos;
                sprintf(drop_str, "%s has dropped the %s Intel", players[p.player_id].name, gamestate.team_1.name);
                break;
        }

        chat_add(0, Red, drop_str, sizeof(drop_str), UTF8);
    }
}

void getPacketRestock(uint8_t * data, int len) {
    local_player.health   = 100;
    local_player.blocks   = 50;
    local_player.grenades = 3;
    weapon_set(true);

    sound_create(SOUND_LOCAL, &sound_switch, 0.0F, 0.0F, 0.0F);
}

void getPacketFogColor(uint8_t * data, int len) {
    READPACKET(PacketFogColor, p, data);
    fog_color[0] = ((float) p.color.r) / 255.0F;
    fog_color[1] = ((float) p.color.g) / 255.0F;
    fog_color[2] = ((float) p.color.b) / 255.0F;
}

void getPacketWeaponReload(uint8_t * data, int len) {
    READPACKET(PacketWeaponReload, p, data);

    if (p.player_id == local_player.id) {
        local_player.ammo          = p.ammo;
        local_player.ammo_reserved = p.reserved;
    } else {
        sound_create_sticky(weapon_sound_reload(players[p.player_id].weapon), players + p.player_id, p.player_id);

        // don’t use values from packet which somehow are never correct
        players[p.player_id].ammo          = weapon_ammo(players[p.player_id].weapon);
        players[p.player_id].ammo_reserved = weapon_ammo_reserved(players[p.player_id].weapon);
    }
}

void getPacketChangeWeapon(uint8_t * data, int len) {
    READPACKET(PacketChangeWeapon, p, data);

    if (p.player_id < PLAYERS_MAX) {
        if (p.player_id == local_player.id) {
            log_warn("Unexpected ChangeWeaponPacket");
            return;
        }

        players[p.player_id].weapon = WEAPON(p.weapon);
    }
}

void getPacketHandshakeInit(uint8_t * data, int len) {
    READPACKET(PacketHandshakeInit, p, data);

    PacketHandshakeReturn reply; reply.challenge = p.challenge;
    sendPacketHandshakeReturn(&reply, 0);
}

#if HACKS_ENABLED
    static char operatingsystem[] = OS " " ARCH " (w/ hacks)";
#else
    static char operatingsystem[] = OS " " ARCH;
#endif

void getPacketVersionGet(uint8_t * data, int len) {
    PacketVersionSend reply;
    reply.client          = 'B';
    reply.major           = BETTERSPADES_MAJOR;
    reply.minor           = BETTERSPADES_MINOR;
    reply.revision        = BETTERSPADES_PATCH;
    reply.operatingsystem = operatingsystem;

    sendPacketVersionSend(&reply, sizeof(operatingsystem));
}

void getPacketPlayerProperties(uint8_t * data, int len) {
    READPACKET(PacketPlayerProperties, p, data);

    if (len >= sizePacketPlayerProperties && p.subID == 0) {
        players[p.player_id].ammo          = p.ammo_clip;
        players[p.player_id].ammo_reserved = p.ammo_reserved;
        players[p.player_id].score         = p.score;

        if (p.player_id == local_player.id) {
            local_player.health        = p.health;
            local_player.blocks        = p.blocks;
            local_player.grenades      = p.grenades;
            local_player.ammo          = p.ammo_clip;
            local_player.ammo_reserved = p.ammo_reserved;
        }
    }
}

void getPacketBulletTrace(uint8_t * data, int len) {
    READPACKET(PacketBulletTrace, p, data);

    if (projectiles.head == NULL || projectiles.size == 0 || projectiles.length == 0) return;

    size_t index = p.index % projectiles.size;

    Trajectory * t = (Trajectory *) (projectiles.head + index * WIDTH(projectiles));

    if (p.origin) { t->index = p.index; t->begin = t->end = 0; }
    if (t->index != p.index) return;

    t->data[t->end].x     = p.pos.x;
    t->data[t->end].y     = 64.0F - p.pos.z; // ???
    t->data[t->end].z     = p.pos.y;
    t->data[t->end].value = p.value;

    NEXT(t->end, projectiles);

    if (t->begin == t->end) NEXT(t->begin, projectiles);
}

void network_updateColor() {
    PacketSetColor contained;
    contained.player_id = local_player.id;
    contained.color     = players[local_player.id].block;
    sendPacketSetColor(&contained, 0);
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
    memset(network_stats, 0, sizeof(NetworkStat) * 40);
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

    static const char popup[] = "No response";

    chat_showpopup(popup, sizeof(popup), UTF8, 3.0F, Red);
    enet_peer_reset(peer);
    return 0;
}

const char * get_version_name(Version version) {
    switch (version) {
        case VER075: return "0.75";
        case VER076: return "0.76";
        default:     return "0.7X";
    }
}

int network_connect(Address * addr) {
    log_info("Connecting to %s at port %i (protocol version %s)", addr->ip, addr->port, get_version_name(addr->version));
    if (network_connected) network_disconnect();

    switch (addr->version) {
        case VER075: {
            if (network_connect_sub(addr->ip, addr->port, VERSION_075)) return 1;
            network_connected = 0; return 0;
        }

        case VER076: {
            if (network_connect_sub(addr->ip, addr->port, VERSION_076)) return 1;
            network_connected = 0; return 0;
        }

        default: {
            if (network_connect_sub(addr->ip, addr->port, VERSION_075)) return 1;
            if (network_connect_sub(addr->ip, addr->port, VERSION_076)) return 1;
            network_connected = 0; return 0;
        }
    }
}

int network_identifier_split(char * str, Address * addr) {
    while (*str && isspace(*str)) str++; // skip trailing whitespace

    if (strstr(str, "aos://") != str) return 0;
    str += 6; // skip that “aos://” prefix

    char * colon = strchr(str, ':');
    addr->port = colon ? strtoul(colon + 1, NULL, 10) : 32887;

    size_t len = strlen(str), iplen = colon ? colon - str : len;

    if (memchr(str, '.', iplen)) {
        strncpy(addr->ip, str, iplen);
        addr->ip[iplen] = 0;
    } else {
        unsigned int ip = strtoul(str, NULL, 10);
        sprintf(addr->ip, "%i.%i.%i.%i", ip & 255, (ip >> 8) & 255, (ip >> 16) & 255, (ip >> 24) & 255);
    }

    if (strcmp(str + len - 5, ":0.75") == 0)
        addr->version = VER075;
    else if (strcmp(str + len - 5, ":0.76") == 0)
        addr->version = VER076;
    else addr->version = UNKNOWN;

    return 1;
}

int network_connect_string(char * str, Version version) {
    Address addr;

    if (!network_identifier_split(str, &addr))
        return 0;

    if (version != UNKNOWN) addr.version = version;

    return network_connect(&addr);
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
                    chat_showpopup(network_reason_disconnect(event.data), sizeof(network_custom_reason), UTF8, 10.0F, Red);
                    log_error("server disconnected! reason: %s", network_reason_disconnect(event.data));
                    event.peer->data = NULL;
                    network_connected = 0;
                    network_logged_in = 0;

                    return 0;
                }

                default: break;
            }
        }

        if (network_logged_in && players[local_player.id].team != TEAM_SPECTATOR && players[local_player.id].alive) {
            if (players[local_player.id].input.keys != network_keys_last) {
                PacketInputData contained;
                contained.player_id = local_player.id;
                contained.keys      = players[local_player.id].input.keys;

                sendPacketInputData(&contained, 0);

                network_keys_last = players[local_player.id].input.keys;
            }

            if ((players[local_player.id].input.buttons != network_buttons_last) &&
               !HASBIT(players[local_player.id].input.keys, INPUT_SPRINT)) {
                PacketWeaponInput contained;
                contained.player_id = local_player.id;
                contained.input     = players[local_player.id].input.buttons;

                sendPacketWeaponInput(&contained, 0);

                network_buttons_last = players[local_player.id].input.buttons;
            }

            if (players[local_player.id].held_item != network_tool_last) {
                PacketSetTool contained;
                contained.player_id = local_player.id;
                contained.tool      = players[local_player.id].held_item;

                sendPacketSetTool(&contained, 0);

                network_tool_last = players[local_player.id].held_item;
            }

            if (window_time() - network_pos_update > 1.0F
               && norm3f(network_pos_last.x, network_pos_last.y, network_pos_last.z,
                         players[local_player.id].pos.x,
                         players[local_player.id].pos.y,
                         players[local_player.id].pos.z) > 0.01F) {
                network_pos_update = window_time();
                network_pos_last   = players[local_player.id].pos;

                PacketPositionData contained;
                contained.pos = htonv3f(players[local_player.id].pos);
                sendPacketPositionData(&contained, 0);
            }

            if (window_time() - network_orient_update > (1.0F / 120.0F)
               && angle3f(network_orient_last.x, network_orient_last.y, network_orient_last.z,
                          players[local_player.id].orientation.x,
                          players[local_player.id].orientation.y,
                          players[local_player.id].orientation.z)
                   > 0.5F / 180.0F * PI) {
                network_orient_update = window_time();
                network_orient_last   = players[local_player.id].orientation;

                PacketOrientationData contained;
                contained.orient = htonov3f(players[local_player.id].orientation);
                sendPacketOrientationData(&contained, 0);
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

    packets[idPacketPositionData]                  = getPacketPositionData;
    packets[idPacketOrientationData]               = getPacketOrientationData;
    packets[idPacketInputData]                     = getPacketInputData;
    packets[idPacketWeaponInput]                   = getPacketWeaponInput;
    packets[idPacketGrenade]                       = getPacketGrenade;
    packets[idPacketSetTool]                       = getPacketSetTool;
    packets[idPacketSetColor]                      = getPacketSetColor;
    packets[idPacketExistingPlayer]                = getPacketExistingPlayer;
    packets[idPacketBlockAction]                   = getPacketBlockAction;
    packets[idPacketBlockLine]                     = getPacketBlockLine;
    packets[idPacketChatMessage]                   = getPacketChatMessage;
    packets[idPacketExtInfo]                       = getPacketExtInfo;

    packets[idPacketWorldUpdate]                   = getPacketWorldUpdate;
    packets[idPacketSetHP]                         = getPacketSetHP;
    packets[idPacketShortPlayerData]               = getPacketShortPlayerData;
    packets[idPacketMoveObject]                    = getPacketMoveObject;
    packets[idPacketCreatePlayer]                  = getPacketCreatePlayer;
    packets[idPacketStateData]                     = getPacketStateData;
    packets[idPacketKillAction]                    = getPacketKillAction;
    packets[idPacketMapStart]                      = getPacketMapStart;
    packets[idPacketMapChunk]                      = getPacketMapChunk;
    packets[idPacketPlayerLeft]                    = getPacketPlayerLeft;
    packets[idPacketTerritoryCapture]              = getPacketTerritoryCapture;
    packets[idPacketProgressBar]                   = getPacketProgressBar;
    packets[idPacketIntelCapture]                  = getPacketIntelCapture;
    packets[idPacketIntelPickup]                   = getPacketIntelPickup;
    packets[idPacketIntelDrop]                     = getPacketIntelDrop;
    packets[idPacketRestock]                       = getPacketRestock;
    packets[idPacketFogColor]                      = getPacketFogColor;
    packets[idPacketWeaponReload]                  = getPacketWeaponReload;
    packets[idPacketChangeWeapon]                  = getPacketChangeWeapon;
    packets[idPacketHandshakeInit]                 = getPacketHandshakeInit;
    packets[idPacketVersionGet]                    = getPacketVersionGet;

    packets[PACKET_EXT_BASE + EXT_PLAYER_PROPERTIES] = getPacketPlayerProperties;
    packets[PACKET_EXT_BASE + EXT_TRACE_BULLETS]     = getPacketBulletTrace;
}
