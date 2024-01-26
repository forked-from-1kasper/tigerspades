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

#ifndef NETWORK_H
#define NETWORK_H

#include <BetterSpades/common.h>

#define MASKON(X)   (1 << X)
#define MASKOFF(X) ~(1 << X)

#define SETBIT(dest, bit, value) { dest &= MASKOFF(bit); dest |= (value << bit); }
#define HASBIT(dest, bit) ((dest) & MASKON(bit))

typedef struct { char ip[32]; int port; Version version; } Address;

const char * network_reason_disconnect(int code);

unsigned int network_ping(void);
void network_send(int id, void * data, int len);
void network_updateColor(void);
void network_disconnect(void);
int network_identifier_split(char * addr, Address *);
int network_connect(Address *);
int network_connect_string(char * addr, Version);
int network_update(void);
int network_status(void);
void network_init(void);

void network_join_game(unsigned char team, unsigned char weapon);

void read_PacketMapChunk(void * data, int len);
void read_PacketChatMessage(void * data, int len);
void read_PacketBlockAction(void * data, int len);
void read_PacketBlockLine(void * data, int len);
void read_PacketStateData(void * data, int len);
void read_PacketFogColor(void * data, int len);
void read_PacketExistingPlayer(void * data, int len);
void read_PacketCreatePlayer(void * data, int len);
void read_PacketPlayerLeft(void * data, int len);
void read_PacketMapStart(void * data, int len);
void read_PacketWorldUpdate(void * data, int len);
void read_PacketPositionData(void * data, int len);
void read_PacketOrientationData(void * data, int len);
void read_PacketSetColor(void * data, int len);
void read_PacketInputData(void * data, int len);
void read_PacketWeaponInput(void * data, int len);
void read_PacketSetTool(void * data, int len);
void read_PacketKillAction(void * data, int len);
void read_PacketShortPlayerData(void * data, int len);
void read_PacketGrenade(void * data, int len);
void read_PacketSetHP(void * data, int len);
void read_PacketRestock(void * data, int len);
void read_PacketChangeWeapon(void * data, int len);
void read_PacketWeaponReload(void * data, int len);
void read_PacketMoveObject(void * data, int len);
void read_PacketIntelCapture(void * data, int len);
void read_PacketIntelDrop(void * data, int len);
void read_PacketIntelPickup(void * data, int len);
void read_PacketTerritoryCapture(void * data, int len);
void read_PacketProgressBar(void * data, int len);
void read_PacketHandshakeInit(void * data, int len);
void read_PacketVersionGet(void * data, int len);

extern void (*packets[256])(void * data, int len);
extern int network_connected;
extern int network_logged_in;
extern int network_map_transfer;
extern int network_received_packets;

extern float network_pos_update;
extern float network_orient_update;
extern unsigned char network_keys_last;
extern unsigned char network_buttons_last;
extern unsigned char network_tool_last;

enum {
    VERSION_075 = 3,
    VERSION_076 = 4
};

extern void * compressed_chunk_data;
extern int compressed_chunk_data_size;
extern int compressed_chunk_data_offset;
extern int compressed_chunk_data_estimate;

extern struct network_stat {
    int outgoing;
    int ingoing;
    int avg_ping;
} network_stats[40];

extern float network_stats_last;

#pragma pack(push, 1)

#define PACKET_HANDSHAKEINIT_ID 31
struct PacketHandshakeInit {
    int challenge;
};

#define PACKET_HANDSHAKERETURN_ID 32
struct PacketHandshakeReturn {
    int challenge;
};

#define PACKET_VERSIONGET_ID 33

#define PACKET_VERSIONSEND_ID 34
struct PacketVersionSend {
    unsigned char client;
    unsigned char major, minor, revision;
    char operatingsystem[64];
};

#define PACKET_MAPCHUNK_ID 19

#define PACKET_POSITIONDATA_ID 0
struct PacketPositionData {
    float x, y, z;
};

#define PACKET_ORIENTATIONDATA_ID 1
struct PacketOrientationData {
    float x, y, z;
};

#define PACKET_WORLDUPDATE_ID 2
struct PacketWorldUpdate075 {
    float x, y, z;
    float ox, oy, oz;
};

struct PacketWorldUpdate076 {
    unsigned char player_id;
    float x, y, z;
    float ox, oy, oz;
};

#define PACKET_INPUTDATA_ID 3
struct PacketInputData {
    unsigned char player_id;
    unsigned char keys;
};

enum {
    INPUT_UP     = 0,
    INPUT_DOWN   = 1,
    INPUT_LEFT   = 2,
    INPUT_RIGHT  = 3,
    INPUT_JUMP   = 4,
    INPUT_CROUCH = 5,
    INPUT_SNEAK  = 6,
    INPUT_SPRINT = 7
};

#define PACKET_WEAPONINPUT_ID 4
struct PacketWeaponInput {
    unsigned char player_id;
    unsigned char input;
};

enum {
    BUTTON_PRIMARY   = 0,
    BUTTON_SECONDARY = 1
};

#define PACKET_MOVEOBJECT_ID 11
struct PacketMoveObject {
    unsigned char object_id;
    unsigned char team;
    float x, y, z;
};

enum {
    TEAM_1_FLAG = 0,
    TEAM_2_FLAG = 1,
    TEAM_1_BASE = 2,
    TEAM_2_BASE = 3
};

#define PACKET_INTELPICKUP_ID 24
struct PacketIntelPickup {
    unsigned char player_id;
};

#define PACKET_INTELCAPTURE_ID 23
struct PacketIntelCapture {
    unsigned char player_id;
    unsigned char winning;
};

#define PACKET_INTELDROP_ID 25
struct PacketIntelDrop {
    unsigned char player_id;
    float x, y, z;
};

#define PACKET_WEAPONRELOAD_ID 28
struct PacketWeaponReload {
    unsigned char player_id;
    unsigned char ammo;
    unsigned char reserved;
};

#define PACKET_SETHP_ID 5
struct PacketSetHP {
    unsigned char hp;
    unsigned char type;
    float x, y, z;
};

#define DAMAGE_SOURCE_FALL 0
#define DAMAGE_SOURCE_GUN  1

#define PACKET_HIT_ID 5
struct PacketHit {
    unsigned char player_id;
    unsigned char hit_type;
};

enum {
    HITTYPE_TORSO = 0,
    HITTYPE_HEAD  = 1,
    HITTYPE_ARMS  = 2,
    HITTYPE_LEGS  = 3,
    HITTYPE_SPADE = 4
};

#define PACKET_KILLACTION_ID 16
struct PacketKillAction {
    unsigned char player_id;
    unsigned char killer_id;
    unsigned char kill_type;
    unsigned char respawn_time;
};

enum {
    KILLTYPE_WEAPON      = 0,
    KILLTYPE_HEADSHOT    = 1,
    KILLTYPE_MELEE       = 2,
    KILLTYPE_GRENADE     = 3,
    KILLTYPE_FALL        = 4,
    KILLTYPE_TEAMCHANGE  = 5,
    KILLTYPE_CLASSCHANGE = 6
};

#define PACKET_RESTOCK_ID 26
struct PacketRestock {
    unsigned char player_id;
};

#define PACKET_GRENADE_ID 6
struct PacketGrenade {
    unsigned char player_id;
    float fuse_length;
    float x, y, z;
    float vx, vy, vz;
};

#define PACKET_MAPSTART_ID 18
struct PacketMapStart075 {
    unsigned int map_size;
};
struct PacketMapStart076 {
    unsigned int map_size;
    unsigned int crc32;
    char map_name[64];
};

#define PACKET_MAPCACHED_ID 31
struct PacketMapCached {
    unsigned char cached;
};

#define PACKET_PLAYERLEFT_ID 20
struct PacketPlayerLeft {
    unsigned char player_id;
};

#define PACKET_EXISTINGPLAYER_ID 9
struct PacketExistingPlayer {
    unsigned char player_id;
    unsigned char team;
    unsigned char weapon;
    unsigned char held_item;
    unsigned int  kills;
    unsigned char blue, green, red;
    char name[17];
};

typedef enum {
    WEAPON_RIFLE   = 0,
    WEAPON_SMG     = 1,
    WEAPON_SHOTGUN = 2,
    WEAPON_MIN     = WEAPON_RIFLE,
    WEAPON_MAX     = WEAPON_SHOTGUN,
    WEAPON_DEFAULT = WEAPON_RIFLE
} Weapon;

#define WEAPON(w) ((w) > WEAPON_MAX ? WEAPON_DEFAULT : (w))

#define PACKET_CREATEPLAYER_ID 12
struct PacketCreatePlayer {
    unsigned char player_id;
    unsigned char weapon;
    unsigned char team;
    float x, y, z;
    char name[17];
};

#define PACKET_BLOCKACTION_ID 13
struct PacketBlockAction {
    unsigned char player_id;
    unsigned char action_type;
    int x, y, z;
};

enum {
    ACTION_BUILD   = 0,
    ACTION_DESTROY = 1,
    ACTION_SPADE   = 2,
    ACTION_GRENADE = 3
};

#define PACKET_BLOCKLINE_ID 14
struct PacketBlockLine {
    unsigned char player_id;
    int sx, sy, sz;
    int ex, ey, ez;
};

#define PACKET_SETCOLOR_ID 8
struct PacketSetColor {
    unsigned char player_id;
    unsigned char blue, green, red;
};

#define PACKET_SHORTPLAYERDATA_ID 10
struct PacketShortPlayerData {
    unsigned char player_id;
    unsigned char team;
    unsigned char weapon;
};

#define PACKET_SETTOOL_ID 7
struct PacketSetTool {
    unsigned char player_id;
    unsigned char tool;
};

typedef enum {
    TOOL_SPADE   = 0,
    TOOL_BLOCK   = 1,
    TOOL_GUN     = 2,
    TOOL_GRENADE = 3,
    TOOL_MIN     = TOOL_SPADE,
    TOOL_MAX     = TOOL_GRENADE,
    TOOL_DEFAULT = TOOL_GUN
} Tool;

#define TOOL(t) (t > TOOL_MAX ? TOOL_DEFAULT : (t))

#define PACKET_CHATMESSAGE_ID 17
struct PacketChatMessage {
    unsigned char player_id;
    unsigned char chat_type;
    unsigned char message[255];
};

enum {
    CHAT_ALL     = 0,
    CHAT_TEAM    = 1,
    CHAT_SYSTEM  = 2,
    CHAT_BIG     = 3,
    CHAT_INFO    = 4,
    CHAT_WARNING = 5,
    CHAT_ERROR   = 6
};

#define PACKET_FOGCOLOR_ID 27
struct PacketFogColor {
    unsigned char alpha, blue, green, red;
};

#define PACKET_CHANGETEAM_ID 29
struct PacketChangeTeam {
    unsigned char player_id;
    unsigned char team;
};

#define PACKET_CHANGEWEAPON_ID 30
struct PacketChangeWeapon {
    unsigned char player_id;
    unsigned char weapon;
};

#define PACKET_STATEDATA_ID 15
typedef struct {
    float x, y, z;
    unsigned char team;
} Territory;

struct PacketStateData {
    unsigned char player_id;
    unsigned char fog_blue, fog_green, fog_red;
    unsigned char team_1_blue, team_1_green, team_1_red;
    unsigned char team_2_blue, team_2_green, team_2_red;
    char team_1_name[10];
    char team_2_name[10];
    unsigned char gamemode;

    union Gamemodes {
        struct GM_CTF {
            unsigned char team_1_score;
            unsigned char team_2_score;
            unsigned char capture_limit;
            unsigned char intels;

            union intel_location {
                struct {
                    unsigned char player_id;
                    unsigned char padding[11];
                } held;
                struct {
                    float x, y, z;
                } dropped;
            } team_1_intel_location;
            union intel_location team_2_intel_location;
            struct {
                float x, y, z;
            } team_1_base;
            struct {
                float x, y, z;
            } team_2_base;
        } ctf;

        struct GM_TC {
            unsigned char territory_count;
            Territory territory[16];
        } tc;
    } gamemode_data;
};

enum {
    TEAM_1_INTEL = 0,
    TEAM_2_INTEL = 1
};

#define PACKET_TERRITORYCAPTURE_ID 21
struct PacketTerritoryCapture {
    unsigned char tent, winning, team;
};

#define PACKET_PROGRESSBAR_ID 22
struct PacketProgressBar {
    unsigned char tent;
    unsigned char team_capturing;
    char rate;
    float progress;
};

#define PACKET_EXTINFO_ID 60
struct PacketExtInfo {
    unsigned char length;
    struct PacketExtInfoEntry {
        unsigned char id;
        unsigned char version;
    } entries[256];
};

struct PacketPlayerProperties {
    uint8_t  subID;
    uint8_t  player_id;
    uint8_t  health;
    uint8_t  blocks;
    uint8_t  grenades;
    uint8_t  ammo_clip;
    uint8_t  ammo_reserved;
    uint32_t score;
};

typedef struct {
    uint8_t index;
    float   x, y, z;
    float   value;
    uint8_t origin;
} PacketBulletTrace;

#define PACKET_EXT_BASE 0x40

enum Extension {
    EXT_PLAYER_PROPERTIES = 0x00,
    EXT_TRACE_BULLETS     = 0x10,
    EXT_256PLAYERS        = 0xC0,
    EXT_MESSAGES          = 0xC1,
    EXT_KICKREASON        = 0xC2,
};

#pragma pack(pop)

#endif
