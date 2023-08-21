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

#ifndef PLAYER_H
#define PLAYER_H

#include <stdbool.h>

#include <BetterSpades/aabb.h>
#include <BetterSpades/network.h>

#define PLAYERS_MAX 256 // just because 32 players are not enough
#define TEAM_1 0
#define TEAM_2 1
#define TEAM_SPECTATOR 255

typedef struct {
    char name[11];
    unsigned char red, green, blue;
} Team;

extern struct GameState {
    Team team_1, team_2;
    unsigned char gamemode_type;
    union Gamemodes gamemode;
    struct {
        unsigned char team_capturing, tent;
        float progress, rate, update;
    } progressbar;
} gamestate;

#define GAMEMODE_CTF 0
#define GAMEMODE_TC 1

extern int button_map[3];

extern unsigned char local_player_id;
extern unsigned char local_player_health;
extern unsigned char local_player_blocks;
extern unsigned char local_player_grenades;
extern unsigned char local_player_ammo, local_player_ammo_reserved;
extern unsigned char local_player_respawn_time;
extern float local_player_death_time;
extern unsigned char local_player_respawn_cnt_last;
extern unsigned char local_player_newteam;
extern unsigned char local_player_lasttool;

extern float local_player_last_damage_timer;
extern float local_player_last_damage_x;
extern float local_player_last_damage_y;
extern float local_player_last_damage_z;

extern char local_player_drag_active;
extern int local_player_drag_x;
extern int local_player_drag_y;
extern int local_player_drag_z;

extern int player_intersection_type;
extern int player_intersection_player;
extern float player_intersection_dist;

typedef struct {
    bool head;
    bool torso;
    bool leg_left;
    bool leg_right;
    bool arms;

    struct {
        float head;
        float torso;
        float leg_left;
        float leg_right;
        float arms;
    } distance;
} Hit;

bool player_intersection_exists(Hit * s);
int player_intersection_choose(Hit * s, float * distance);

typedef struct {
    float x, y, z;
} Position;

typedef struct {
    float x, y, z;
} Orientation;

typedef struct {
    float x, y, z;
} Velocity;

typedef struct {
    char name[17];
    Position pos;
    Orientation orientation;
    AABB bb_2d;
    Orientation orientation_smooth;
    Position gun_pos;
    Position casing_dir;
    float gun_shoot_timer;
    int ammo, ammo_reserved;
    float spade_use_timer;
    unsigned char spade_used, spade_use_type;
    unsigned int score;
    unsigned char team, weapon, held_item;
    unsigned char alive, connected;
    float item_showup, item_disabled, items_show_start;
    unsigned char items_show;
    TrueColor block;

    struct {
        unsigned char keys, buttons;
    } input;

    struct {
        float lmb, rmb;
    } start;

    struct {
        unsigned char jump, airborne, wade;
        float lastclimb;
        Velocity velocity;
        Position eye;
    } physics;

    struct {
        float feet_started, feet_started_cycle;
        char feet_cylce;
        float tool_started;
    } sound;
} Player;

extern Player players[PLAYERS_MAX];
// pyspades/pysnip/piqueserver sometimes uses ids that are out of range

void player_on_held_item_change(Player * p);
int player_can_spectate(Player * p);
float player_section_height(int section);
void player_init(void);
float player_height(const Player * p);
float player_height2(const Player * p);
void player_reposition(Player * p);
void player_update(float dt, int locked);
void player_render_all(void);
void player_render(Player * p, int id);
void player_collision(const Player * p, Ray * ray, Hit * intersects);
void player_reset(Player * p);
int player_move(Player * p, float fsynctics, int id);
int player_uncrouch(Player * p);

#endif
