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
#include <limits.h>

#include <BetterSpades/common.h>
#include <BetterSpades/camera.h>
#include <BetterSpades/player.h>
#include <BetterSpades/sound.h>
#include <BetterSpades/map.h>
#include <BetterSpades/matrix.h>
#include <BetterSpades/model.h>
#include <BetterSpades/font.h>
#include <BetterSpades/cameracontroller.h>
#include <BetterSpades/config.h>
#include <BetterSpades/tracer.h>
#include <BetterSpades/weapon.h>
#include <BetterSpades/window.h>
#include <BetterSpades/particle.h>

struct GameState gamestate;

int button_map[3];

unsigned char local_player_id = 0;
unsigned char local_player_health = 100;
unsigned char local_player_blocks = 50;
unsigned char local_player_grenades = 3;
unsigned char local_player_ammo, local_player_ammo_reserved;
unsigned char local_player_respawn_time = 0;
float local_player_death_time = 0.0F;
unsigned char local_player_respawn_cnt_last = 255;
unsigned char local_player_lasttool;

int default_team = -1, default_gun = -1;

float local_player_last_damage_timer;
float local_player_last_damage_x;
float local_player_last_damage_y;
float local_player_last_damage_z;

char local_player_drag_active = 0;
int local_player_drag_x;
int local_player_drag_y;
int local_player_drag_z;

int player_intersection_type = -1;
int player_intersection_player = 0;
float player_intersection_dist = 1024.0F;

Player players[PLAYERS_MAX];

#define FALL_DAMAGE_VELOCITY 0.58F
#define FALL_SLOW_DOWN 0.24F
#define SQRT 0.70710678F
#define WEAPON_PRIMARY 1
#define FALL_DAMAGE_SCALAR 4096

void player_init() {
    for (int k = 0; k < PLAYERS_MAX; k++) {
        player_reset(&players[k]);
        players[k].score = 0;
    }
}

void player_reset(Player * p) {
    p->connected          = 0;
    p->alive              = 0;
    p->held_item          = TOOL_GUN;
    p->block.r            = 111;
    p->block.g            = 111;
    p->block.b            = 111;
    p->physics.velocity.x = 0.0F;
    p->physics.velocity.y = 0.0F;
    p->physics.velocity.z = 0.0F;
    p->physics.jump       = 0;
    p->physics.airborne   = 0;
    p->physics.wade       = 0;
    p->input.keys         = 0;
    p->input.buttons      = 0;
}

void player_on_held_item_change(Player * p) {
    if (p->input.buttons & MASKON(BUTTON_PRIMARY))
        p->start.lmb = window_time() + 0.8F;

    if (p->input.buttons & MASKON(BUTTON_SECONDARY))
        p->start.rmb = window_time() + 0.8F;

    p->item_disabled    = window_time();
    p->items_show_start = window_time();
    p->item_showup      = window_time() + 0.3F;
    p->items_show       = 1;
}

int player_can_spectate(Player * p) {
    return p->connected
        && ((players[local_player_id].team != TEAM_SPECTATOR && p->team == players[local_player_id].team)
            || (players[local_player_id].team == TEAM_SPECTATOR && p->team != TEAM_SPECTATOR));
}

float player_swing_func(float x) {
    x -= (int)x;
    return (x < 0.5F) ? (x * 4.0F - 1.0F) : (3.0F - x * 4.0F);
}

float player_spade_func(float x) {
    return 1.0F - (x * 5 - (int)(x * 5));
}

float * player_tool_func(const Player * p) {
    static float ret[3];
    ret[0] = ret[1] = ret[2] = 0.0F;
    switch (p->held_item) {
        case TOOL_SPADE: {
            float t = window_time() - p->spade_use_timer;
            if (p->spade_use_type == 1 && t > 0.2F) {
                return ret;
            }
            if (p->spade_use_type == 2 && t > 1.0F) {
                return ret;
            }
            if (p == &players[local_player_id] && camera_mode == CAMERAMODE_FPS) {
                if (p->spade_use_type == 1) {
                    ret[0] = player_spade_func(t) * 90.0F;
                    return ret;
                }
                if (p->spade_use_type == 2) {
                    if (t <= 0.4F) {
                        ret[0] = 60.0F - player_spade_func(t / 2.0F) * 60.0F;
                        ret[1] = -t / 0.4F * 22.5F;
                        return ret;
                    }
                    if (t <= 0.7F) {
                        ret[0] = 60.0F;
                        ret[1] = -22.5F;
                        return ret;
                    }
                    if (t <= 1.0F) {
                        ret[0] = player_spade_func((t - 0.7F) / 5 / 0.3F) * 60.0F;
                        ret[1] = (t - 0.7F) / 0.4F * 22.5F - 22.5F;
                        return ret;
                    }
                }
            } else {
                if (p->input.buttons & MASKON(BUTTON_PRIMARY)) {
                    ret[0] = (player_swing_func((window_time() - p->spade_use_timer) * 2.5F) + 1.0F) / 2.0F * 60.0F;
                    return ret;
                }
                if (p->input.buttons & MASKON(BUTTON_SECONDARY)) {
                    ret[0] = (player_swing_func((window_time() - p->spade_use_timer) * 0.5F) + 1.0F) / 2.0F * 60.0F;
                    return ret;
                }
            }
        }
            // case TOOL_GRENADE:
            /*if (p->input.buttons.lmb && p!=&players[local_player_id]) {
                ret[0] = max(-(window_time()-p->input.buttons.lmb_start)*35.0F,-35.0F);
                return ret;
            } else {
                return ret;
            }*/
    }
    return ret;
}

float * player_tool_translate_func(Player * p) {
    static float ret[3];
    ret[0] = ret[1] = ret[2] = 0.0F;
    if (p == &players[local_player_id] && camera_mode == CAMERAMODE_FPS) {
        if (window_time() - p->item_showup < 0.5F) {
            return ret;
        }
        if (p->held_item == TOOL_GUN
           && window_time() - weapon_last_shot < weapon_delay(players[local_player_id].weapon)) {
            ret[2] = -(weapon_delay(players[local_player_id].weapon) - (window_time() - weapon_last_shot))
                / weapon_delay(players[local_player_id].weapon) * weapon_recoil_anim(players[local_player_id].weapon)
                * (local_player_ammo > 0);
            return ret;
        }

        if (p->held_item == TOOL_SPADE) {
            float t = window_time() - p->spade_use_timer;
            if (t > 1.0F) {
                return ret;
            }
            if (p->spade_use_type == 2) {
                if (t > 0.4F && t <= 0.7F) {
                    ret[2] = (t - 0.4F) / 0.3F * 0.8F;
                    return ret;
                }
                if (t > 0.7F) {
                    ret[2] = (0.3F - (t - 0.7F)) / 0.3F * 0.8F;
                    return ret;
                }
            }
        }
        if (p->held_item == TOOL_GRENADE) {
            if (p->input.buttons & MASKON(BUTTON_PRIMARY)) {
                ret[1] = (window_time() - p->start.lmb) * 1.3F;
                ret[0] = -ret[1];
                return ret;
            } else {
                return ret;
            }
        }
    }
    return ret;
}

float player_height(const Player * p) {
    return (p->input.keys & MASKON(INPUT_CROUCH)) ? 1.05F : 1.1F;
}

float player_height2(const Player * p) {
    return p->alive ? 0.0F : 1.0F;
}

float player_section_height(int section) {
    switch (section) {
        case HITTYPE_HEAD:  return +1.0F;
        case HITTYPE_TORSO: return +0.0F;
        case HITTYPE_ARMS:  return +0.25F;
        case HITTYPE_LEGS:  return -1.0F;
        default:            return +0.0F;
    }
}

bool player_intersection_exists(Hit * s) {
    return s->head || s->torso || s->arms || s->leg_left || s->leg_right;
}

int player_intersection_choose(Hit * s, float * dist) {
    int type;
    *dist = FLT_MAX;

    if (s->arms && s->distance.arms < *dist) {
        type = HITTYPE_ARMS;
        *dist = s->distance.arms;
    }

    if (s->leg_left && s->distance.leg_left < *dist) {
        type = HITTYPE_LEGS;
        *dist = s->distance.leg_left;
    }

    if (s->leg_right && s->distance.leg_right < *dist) {
        type = HITTYPE_LEGS;
        *dist = s->distance.leg_right;
    }

    if (s->torso && s->distance.torso < *dist) {
        type = HITTYPE_TORSO;
        *dist = s->distance.torso;
    }

    if (s->head && s->distance.head < *dist) {
        type = HITTYPE_HEAD;
        *dist = s->distance.head;
    }

    return type;
}

void player_update(float dt, int locked) {
    for (int k = 0; k < PLAYERS_MAX; k++) {
        if (players[k].connected) {
            if (locked) {
                player_move(&players[k], dt, k);
            } else {
                if (k != local_player_id) {
                    // smooth out player orientation
                    players[k].orientation_smooth.x = players[k].orientation_smooth.x * pow(0.9F, dt * 60.0F)
                        + players[k].orientation.x * pow(0.1F, dt * 60.0F);
                    players[k].orientation_smooth.y = players[k].orientation_smooth.y * pow(0.9F, dt * 60.0F)
                        + players[k].orientation.y * pow(0.1F, dt * 60.0F);
                    players[k].orientation_smooth.z = players[k].orientation_smooth.z * pow(0.9F, dt * 60.0F)
                        + players[k].orientation.z * pow(0.1F, dt * 60.0F);
                }
            }
        }
    }
}

void player_render_all() {
    player_intersection_type = -1;
    player_intersection_dist = FLT_MAX;

    Ray ray;
    ray.origin[X] = camera_x;
    ray.origin[Y] = camera_y;
    ray.origin[Z] = camera_z;
    ray.direction[X] = sin(camera_rot_x) * sin(camera_rot_y);
    ray.direction[Y] = cos(camera_rot_y);
    ray.direction[Z] = cos(camera_rot_x) * sin(camera_rot_y);

    for (int k = 0; k < PLAYERS_MAX; k++) {
        if (!players[k].connected || players[k].team == TEAM_SPECTATOR)
            continue;

        if (!(players[k].input.buttons & MASKON(BUTTON_PRIMARY)) &&
            !(players[k].input.buttons & MASKON(BUTTON_SECONDARY))) {
            players[k].spade_used = 0;

            if (players[k].spade_use_type == 1)
                players[k].spade_use_type = 0;

            if (players[k].spade_use_type == 2)
                players[k].spade_use_timer = 0;
        }
        if (players[k].alive && players[k].held_item == TOOL_SPADE
           && ((players[k].input.buttons & MASKON(BUTTON_PRIMARY)) ||
               (players[k].input.buttons & MASKON(BUTTON_SECONDARY)))
           && window_time() - players[k].item_showup >= 0.5F) {
            // now run a hitscan and see if any block or player is in the way
            struct Camera_HitType hit;
            if ((players[k].input.buttons & MASKON(BUTTON_PRIMARY)) &&
                (window_time() - players[k].spade_use_timer > 0.2F)) {
                camera_hit_fromplayer(&hit, k, 4.0F);
                if (hit.y == 0 && hit.type == CAMERA_HITTYPE_BLOCK)
                    hit.type = CAMERA_HITTYPE_NONE;
                switch (hit.type) {
                    case CAMERA_HITTYPE_BLOCK:
                        sound_create(SOUND_WORLD, &sound_hitground, hit.x + 0.5F, hit.y + 0.5F, hit.z + 0.5F);

                        if (k == local_player_id)
                            map_damage(hit.x, hit.y, hit.z, 50);

                        if (k == local_player_id && map_damage_action(hit.x, hit.y, hit.z) && hit.y > 1) {
                            struct PacketBlockAction blk;
                            blk.action_type = ACTION_DESTROY;
                            blk.player_id = local_player_id;
                            blk.x = htoles32(hit.x);
                            blk.y = htoles32(hit.z);
                            blk.z = htoles32(63 - hit.y);
                            network_send(PACKET_BLOCKACTION_ID, &blk, sizeof(blk));
                            local_player_blocks = min(local_player_blocks + 1, 50);
                            // read_PacketBlockAction(&blk,sizeof(blk));
                        } else {
                            particle_create(map_get(hit.x, hit.y, hit.z), hit.xb + 0.5F, hit.yb + 0.5F, hit.zb + 0.5F,
                                            2.5F, 1.0F, 4, 0.1F, 0.25F);
                        }
                        break;
                    case CAMERA_HITTYPE_PLAYER:
                        sound_create_sticky(&sound_spade_whack, players + k, k);
                        particle_create(
                            Red,
                            players[hit.player_id].physics.eye.x,
                            players[hit.player_id].physics.eye.y + player_section_height(hit.player_section),
                            players[hit.player_id].physics.eye.z,
                            3.5F, 1.0F, 8, 0.1F, 0.4F
                        );
                        if (k == local_player_id) {
                            struct PacketHit h;
                            h.player_id = hit.player_id;
                            h.hit_type = HITTYPE_SPADE;
                            network_send(PACKET_HIT_ID, &h, sizeof(h));
                        }
                        break;
                    case CAMERA_HITTYPE_NONE: sound_create_sticky(&sound_spade_woosh, players + k, k); break;
                }
                players[k].spade_use_type = 1;
                players[k].spade_used = 1;
                players[k].spade_use_timer = window_time();
            }

            if ((players[k].input.buttons & MASKON(BUTTON_SECONDARY)) &&
                (window_time() - players[k].spade_use_timer > 1.0F)) {
                if (players[k].spade_used) {
                    camera_hit_fromplayer(&hit, k, 4.0F);
                    if (hit.type == CAMERA_HITTYPE_BLOCK && hit.y > 1) {
                        sound_create(SOUND_WORLD, &sound_hitground, hit.x + 0.5F, hit.y + 0.5F, hit.z + 0.5F);
                        if (k == local_player_id) {
                            struct PacketBlockAction blk;
                            blk.action_type = ACTION_SPADE;
                            blk.player_id = local_player_id;
                            blk.x = htoles32(hit.x);
                            blk.y = htoles32(hit.z);
                            blk.z = htoles32(63 - hit.y);
                            network_send(PACKET_BLOCKACTION_ID, &blk, sizeof(blk));
                        }
                    } else {
                        sound_create_sticky(&sound_spade_woosh, players + k, k);
                    }
                }
                players[k].spade_use_type = 2;
                players[k].spade_used = 1;
                players[k].spade_use_timer = window_time();
            }
        }
        if (k != local_player_id) {
            if (camera_CubeInFrustum(players[k].pos.x, players[k].pos.y, players[k].pos.z, 1.0F, 2.0F)
               && distance2D(players[k].pos.x, players[k].pos.z, camera_x, camera_z)
                   <= pow(settings.render_distance + 2.0F, 2.0F)) {
                Hit intersects = {0};
                player_render(players + k, k);
                player_collision(players + k, &ray, &intersects);
                if (player_intersection_exists(&intersects)) {
                    float d;
                    int type = player_intersection_choose(&intersects, &d);
                    if (d < player_intersection_dist) {
                        player_intersection_dist = d;
                        player_intersection_player = k;
                        player_intersection_type = type;
                    }
                }
            }

            if (players[k].alive && players[k].held_item == TOOL_GUN && (players[k].input.buttons & MASKON(BUTTON_PRIMARY))) {
                if (window_time() - players[k].gun_shoot_timer > weapon_delay(players[k].weapon)
                   && players[k].ammo > 0) {
                    players[k].ammo--;
                    sound_create_sticky(weapon_sound(players[k].weapon), players + k, k);

                    float o[3] = {players[k].orientation.x, players[k].orientation.y, players[k].orientation.z};

                    weapon_spread(&players[k], o);

                    struct Camera_HitType hit;
                    camera_hit(&hit, k, players[k].physics.eye.x, players[k].physics.eye.y + player_height(&players[k]),
                               players[k].physics.eye.z, o[0], o[1], o[2], 128.0F);
                    tracer_pvelocity(o, &players[k]);
                    tracer_add(players[k].weapon, players[k].physics.eye.x,
                               players[k].physics.eye.y + player_height(&players[k]), players[k].physics.eye.z, o[0],
                               o[1], o[2]);
                    particle_create_casing(&players[k]);
                    switch (hit.type) {
                        case CAMERA_HITTYPE_PLAYER: {
                            sound_create_sticky((hit.player_section == HITTYPE_HEAD) ? &sound_spade_whack :
                                                                                       &sound_hitplayer,
                                                players + hit.player_id, hit.player_id);
                            particle_create(
                                Red,
                                players[hit.player_id].physics.eye.x,
                                players[hit.player_id].physics.eye.y + player_section_height(hit.player_section),
                                players[hit.player_id].physics.eye.z,
                                3.5F, 1.0F, 8, 0.1F, 0.4F
                            );
                            break;
                        }
                        case CAMERA_HITTYPE_BLOCK:
                            particle_create(map_get(hit.x, hit.y, hit.z), hit.xb + 0.5F, hit.yb + 0.5F, hit.zb + 0.5F,
                                            2.5F, 1.0F, 4, 0.1F, 0.25F);
                            break;
                    }
                    players[k].gun_shoot_timer = window_time();
                }
            }
        }
    }
}

static float foot_function(const Player * p) {
    float f = (window_time() - p->sound.feet_started_cycle) /
              ((p->input.keys & MASKON(INPUT_SPRINT)) ? (0.5F / 1.3F) : 0.5F);
    f = f * 2.0F - 1.0F;
    return p->sound.feet_cylce ? f : -f;
}

struct hitbox {
    float pivot[3];
    int size[3];
    float scale;
};

static const struct hitbox box_head = {
    .pivot = {2.5F, 2.5F, 0.5F},
    .size = {6, 6, 6},
    .scale = 0.1F,
};

static const struct hitbox box_torso = {
    .pivot = {3.5F, 1.5F, 9.5F},
    .size = {8, 4, 9},
    .scale = 0.1F,
};

static const struct hitbox box_torsoc = {
    .pivot = {3.5F, 6.5F, 6.5F},
    .size = {8, 8, 7},
    .scale = 0.1F,
};

static const struct hitbox box_arm_left = {
    .pivot = {5.5F, -0.5F, 5.5F},
    .size = {2, 9, 6},
    .scale = 0.1F,
};

static const struct hitbox box_arm_right = {
    .pivot = {-3.5F, 4.25F, 1.5F},
    .size = {3, 14, 2},
    .scale = 0.1F,
};

static const struct hitbox box_leg = {
    .pivot = {1.0F, 1.5F, 12.0F},
    .size = {3, 5, 12},
    .scale = 0.1F,
};

static const struct hitbox box_legc = {
    .pivot = {1.0F, 1.5F, 7.0F},
    .size = {3, 7, 8},
    .scale = 0.1F,
};

static bool hitbox_intersection(mat4 model, const struct hitbox * box, Ray * r, float * distance) {
    mat4 inv_model;
    glm_mat4_inv(model, inv_model);

    vec3 origin, dir;
    glm_mat4_mulv3(inv_model, r->origin, 1.0F, origin);
    glm_mat4_mulv3(inv_model, r->direction, 0.0F, dir);

    return aabb_intersection_ray(
        &(AABB) {
            .min = {-box->pivot[0] * box->scale, -box->pivot[2] * box->scale, -box->pivot[1] * box->scale},
            .max = {(box->size[0] - box->pivot[0]) * box->scale, (box->size[2] - box->pivot[2]) * box->scale,
                    (box->size[1] - box->pivot[1]) * box->scale},
        },
        &(Ray) {
            .origin = {origin[0], origin[1], origin[2]},
            .direction = {dir[0], dir[1], dir[2]},
        },
        distance);
}

void player_collision(const Player * p, Ray * ray, Hit * intersects) {
    if (!p->alive || p->team == TEAM_SPECTATOR)
        return;

    float l = sqrt(distance3D(p->orientation_smooth.x, p->orientation_smooth.y, p->orientation_smooth.z, 0, 0, 0));
    float ox = p->orientation_smooth.x / l;
    float oy = p->orientation_smooth.y / l;
    float oz = p->orientation_smooth.z / l;

    const struct hitbox * torso = (p->input.keys & MASKON(INPUT_CROUCH)) ? &box_torsoc : &box_torso;
    const struct hitbox * leg   = (p->input.keys & MASKON(INPUT_CROUCH)) ? &box_legc   : &box_leg;

    float height = player_height(p) - 0.25F;

    float len = sqrt(pow(p->orientation.x, 2.0F) + pow(p->orientation.z, 2.0F));
    float fx = p->orientation.x / len;
    float fy = p->orientation.z / len;

    float a = (p->physics.velocity.x * fx + fy * p->physics.velocity.z) / (fx * fx + fy * fy);
    float b = (p->physics.velocity.z - fy * a) / fx;
    a /= 0.25F;
    b /= 0.25F;

    float dist; // distance

    matrix_push(matrix_model);

    matrix_identity(matrix_model);
    matrix_translate(matrix_model, p->physics.eye.x, p->physics.eye.y + height, p->physics.eye.z);
    float head_scale = sqrt(pow(p->orientation.x, 2.0F) + pow(p->orientation.y, 2.0F) + pow(p->orientation.z, 2.0F));
    matrix_translate(matrix_model, 0.0F, box_head.pivot[2] * (head_scale * box_head.scale - box_head.scale), 0.0F);
    matrix_scale3(matrix_model, head_scale);
    matrix_pointAt(matrix_model, ox, oy, oz);
    matrix_rotate(matrix_model, 90.0F, 0.0F, 1.0F, 0.0F);

    if (hitbox_intersection(matrix_model, &box_head, ray, &dist)) {
        intersects->head = 1;
        intersects->distance.head = dist;
    }

    matrix_identity(matrix_model);
    matrix_translate(matrix_model, p->physics.eye.x, p->physics.eye.y + height, p->physics.eye.z);
    matrix_pointAt(matrix_model, ox, 0.0F, oz);
    matrix_rotate(matrix_model, 90.0F, 0.0F, 1.0F, 0.0F);

    if (hitbox_intersection(matrix_model, torso, ray, &dist)) {
        intersects->torso = 1;
        intersects->distance.torso = dist;
    }

    matrix_push(matrix_model);
    matrix_translate(matrix_model, torso->size[0] * 0.1F * 0.5F - leg->size[0] * 0.1F * 0.5F,
                     -torso->size[2] * 0.1F * ((p->input.keys & MASKON(INPUT_CROUCH)) ? 0.6F : 1.0F),
                     (p->input.keys & MASKON(INPUT_CROUCH)) ? (-torso->size[2] * 0.1F * 0.75F) : 0.0F);
    matrix_rotate(matrix_model, 45.0F * foot_function(p) * a, 1.0F, 0.0F, 0.0F);
    matrix_rotate(matrix_model, 45.0F * foot_function(p) * b, 0.0F, 0.0F, 1.0F);

    if (hitbox_intersection(matrix_model, leg, ray, &dist)) {
        intersects->leg_left = 1;
        intersects->distance.leg_left = dist;
    }

    matrix_pop(matrix_model);
    matrix_translate(matrix_model, -torso->size[0] * 0.1F * 0.5F + leg->size[0] * 0.1F * 0.5F,
                     -torso->size[2] * 0.1F * ((p->input.keys & MASKON(INPUT_CROUCH)) ? 0.6F : 1.0F),
                     p->input.keys & MASKON(INPUT_CROUCH) ? (-torso->size[2] * 0.1F * 0.75F) : 0.0F);
    matrix_rotate(matrix_model, -45.0F * foot_function(p) * a, 1.0F, 0.0F, 0.0F);
    matrix_rotate(matrix_model, -45.0F * foot_function(p) * b, 0.0F, 0.0F, 1.0F);

    if (hitbox_intersection(matrix_model, leg, ray, &dist)) {
        intersects->leg_right = 1;
        intersects->distance.leg_right = dist;
    }

    matrix_identity(matrix_model);
    matrix_translate(matrix_model, p->physics.eye.x, p->physics.eye.y + height, p->physics.eye.z);
    matrix_translate(matrix_model, 0.0F, (p->input.keys & MASKON(INPUT_CROUCH) ? 0.1F : 0.0F) - 0.1F * 2, 0.0F);
    matrix_pointAt(matrix_model, ox, oy, oz);
    matrix_rotate(matrix_model, 90.0F, 0.0F, 1.0F, 0.0F);

    if ((p->input.keys & MASKON(INPUT_SPRINT)) && !(p->input.keys & MASKON(INPUT_CROUCH)))
        matrix_rotate(matrix_model, 45.0F, 1.0F, 0.0F, 0.0F);

    float * angles = player_tool_func(p);
    matrix_rotate(matrix_model, angles[0], 1.0F, 0.0F, 0.0F);
    matrix_rotate(matrix_model, angles[1], 0.0F, 1.0F, 0.0F);

    if (hitbox_intersection(matrix_model, &box_arm_left, ray, &dist)) {
        intersects->arms = 1;
        intersects->distance.arms = dist;
    }

    matrix_rotate(matrix_model, -45.0F, 0.0F, 1.0F, 0.0F);

    if (hitbox_intersection(matrix_model, &box_arm_right, ray, &dist)) {
        intersects->arms = 1;
        intersects->distance.arms = dist;
    }

    matrix_pop(matrix_model);
}

void player_render(Player * p, int id) {
    kv6_calclight(p->pos.x, p->pos.y, p->pos.z);

    if (camera_mode == CAMERAMODE_SPECTATOR && p->team != TEAM_SPECTATOR && !cameracontroller_bodyview_mode) {
        matrix_push(matrix_model);
        matrix_translate(matrix_model, p->pos.x, p->physics.eye.y + player_height(p) + 1.25F, p->pos.z);
        matrix_rotate(matrix_model, camera_rot_x / PI * 180.0F + 180.0F, 0.0F, 1.0F, 0.0F);
        matrix_rotate(matrix_model, -camera_rot_y / PI * 180.0F + 90.0F, 1.0F, 0.0F, 0.0F);
        matrix_scale(matrix_model, 1.0F / 92.0F, 1.0F / 92.0F, 1.0F / 92.0F);
        matrix_upload();

        switch (p->team) {
            case TEAM_1: glColor3ub(gamestate.team_1.red, gamestate.team_1.green, gamestate.team_1.blue); break;
            case TEAM_2: glColor3ub(gamestate.team_2.red, gamestate.team_2.green, gamestate.team_2.blue); break;
        }

        font_select(FONT_FIXEDSYS);
        glEnable(GL_ALPHA_TEST);
        glAlphaFunc(GL_GREATER, 0.5F);
        glDisable(GL_DEPTH_TEST);
        font_centered(0, 0, 4, p->name, UTF8);
        glEnable(GL_DEPTH_TEST);
        glDisable(GL_ALPHA_TEST);
        matrix_pop(matrix_model);
        matrix_upload();
    }

    float l = sqrt(distance3D(p->orientation_smooth.x, p->orientation_smooth.y, p->orientation_smooth.z, 0, 0, 0));
    float ox = p->orientation_smooth.x / l;
    float oy = p->orientation_smooth.y / l;
    float oz = p->orientation_smooth.z / l;

    if (!p->alive) {
        if (id != local_player_id || camera_mode != CAMERAMODE_DEATH) {
            matrix_push(matrix_model);
            matrix_translate(matrix_model, p->pos.x, p->pos.y + 0.25F, p->pos.z);
            matrix_pointAt(matrix_model, ox, 0.0F, oz);
            matrix_rotate(matrix_model, 90.0F, 0.0F, 1.0F, 0.0F);
            if (p->physics.velocity.y < 0.05F && p->pos.y < 1.5F)
                matrix_translate(matrix_model, 0.0F, (sin(window_time() * 1.5F) - 1.0F) * 0.1F, 0.0F);
            matrix_upload();
            kv6_render(&model_playerdead, p->team);
            matrix_pop(matrix_model);
        }

        return;
    }

    float time = window_time() * 1000.0F;

    struct kv6_t * torso = (p->input.keys & MASKON(INPUT_CROUCH)) ? &model_playertorsoc : &model_playertorso;
    struct kv6_t * leg   = (p->input.keys & MASKON(INPUT_CROUCH)) ? &model_playerlegc   : &model_playerleg;
    float height = player_height(p);
    if (id != local_player_id)
        height -= 0.25F;

    float len = sqrt(pow(p->orientation.x, 2.0F) + pow(p->orientation.z, 2.0F));
    float fx = p->orientation.x / len;
    float fy = p->orientation.z / len;

    float a = (p->physics.velocity.x * fx + fy * p->physics.velocity.z) / (fx * fx + fy * fy);
    float b = (p->physics.velocity.z - fy * a) / fx;
    a /= 0.25F;
    b /= 0.25F;

    int render_body = (id != local_player_id || !p->alive || camera_mode != CAMERAMODE_FPS)
        && !((camera_mode == CAMERAMODE_BODYVIEW || camera_mode == CAMERAMODE_SPECTATOR)
             && cameracontroller_bodyview_mode && cameracontroller_bodyview_player == id);
    int render_fpv = (id == local_player_id && camera_mode == CAMERAMODE_FPS)
        || ((camera_mode == CAMERAMODE_BODYVIEW || camera_mode == CAMERAMODE_SPECTATOR)
            && cameracontroller_bodyview_mode && cameracontroller_bodyview_player == id);

    if (render_body) {
        matrix_push(matrix_model);
        matrix_translate(matrix_model, p->physics.eye.x, p->physics.eye.y + height, p->physics.eye.z);
        float head_scale
            = sqrt(pow(p->orientation.x, 2.0F) + pow(p->orientation.y, 2.0F) + pow(p->orientation.z, 2.0F));
        matrix_translate(matrix_model, 0.0F,
                         model_playerhead.zpiv * (head_scale * model_playerhead.scale - model_playerhead.scale), 0.0F);
        matrix_scale3(matrix_model, head_scale);
        matrix_pointAt(matrix_model, ox, oy, oz);
        matrix_rotate(matrix_model, 90.0F, 0.0F, 1.0F, 0.0F);
        matrix_upload();
        kv6_render(&model_playerhead, p->team);
        matrix_pop(matrix_model);

        matrix_push(matrix_model);
        matrix_translate(matrix_model, p->physics.eye.x, p->physics.eye.y + height, p->physics.eye.z);
        matrix_pointAt(matrix_model, ox, 0.0F, oz);
        matrix_rotate(matrix_model, 90.0F, 0.0F, 1.0F, 0.0F);
        matrix_upload();
        kv6_render(torso, p->team);
        matrix_pop(matrix_model);

        if (gamestate.gamemode_type == GAMEMODE_CTF &&
            (((gamestate.gamemode.ctf.intels & MASKON(TEAM_1_INTEL)) &&
              (gamestate.gamemode.ctf.team_1_intel_location.held.player_id == id)) ||
             ((gamestate.gamemode.ctf.intels & MASKON(TEAM_2_INTEL)) &&
              (gamestate.gamemode.ctf.team_2_intel_location.held.player_id == id)))) {
            matrix_push(matrix_model);
            matrix_translate(matrix_model, p->physics.eye.x, p->physics.eye.y + height, p->physics.eye.z);
            matrix_pointAt(matrix_model, -oz, 0.0F, ox);
            matrix_translate(
                matrix_model, (torso->xsiz - model_intel.xsiz) * 0.5F * torso->scale,
                -(torso->zpiv - torso->zsiz * 0.5F + model_intel.zsiz * (p->input.keys & MASKON(INPUT_CROUCH) ? 0.125F : 0.25F)) * torso->scale,
                (torso->ypiv + model_intel.ypiv) * torso->scale
            );

            matrix_scale3(matrix_model, torso->scale / model_intel.scale);

            if (p->input.keys & MASKON(INPUT_CROUCH))
                matrix_rotate(matrix_model, -45.0F, 1.0F, 0.0F, 0.0F);
            matrix_upload();

            int t = TEAM_SPECTATOR;

            if ((gamestate.gamemode.ctf.intels & MASKON(TEAM_1_INTEL))
             && (gamestate.gamemode.ctf.team_1_intel_location.held.player_id == id))
                t = TEAM_1;

            if ((gamestate.gamemode.ctf.intels & MASKON(TEAM_2_INTEL))
             && (gamestate.gamemode.ctf.team_2_intel_location.held.player_id == id))
                t = TEAM_2;

            kv6_render(&model_intel, t);
            matrix_pop(matrix_model);
        }

        matrix_push(matrix_model);
        matrix_translate(matrix_model, p->physics.eye.x, p->physics.eye.y + height, p->physics.eye.z);
        matrix_pointAt(matrix_model, ox, 0.0F, oz);
        matrix_rotate(matrix_model, 90.0F, 0.0F, 1.0F, 0.0F);
        matrix_translate(matrix_model, torso->xsiz * 0.1F * 0.5F - leg->xsiz * 0.1F * 0.5F,
                         -torso->zsiz * 0.1F * (p->input.keys & MASKON(INPUT_CROUCH) ? 0.6F : 1.0F),
                         p->input.keys & MASKON(INPUT_CROUCH) ? (-torso->zsiz * 0.1F * 0.75F) : 0.0F);
        matrix_rotate(matrix_model, 45.0F * foot_function(p) * a, 1.0F, 0.0F, 0.0F);
        matrix_rotate(matrix_model, 45.0F * foot_function(p) * b, 0.0F, 0.0F, 1.0F);
        matrix_upload();
        kv6_render(leg, p->team);
        matrix_pop(matrix_model);

        matrix_push(matrix_model);
        matrix_translate(matrix_model, p->physics.eye.x, p->physics.eye.y + height, p->physics.eye.z);
        matrix_pointAt(matrix_model, ox, 0.0F, oz);
        matrix_rotate(matrix_model, 90.0F, 0.0F, 1.0F, 0.0F);
        matrix_translate(matrix_model, -torso->xsiz * 0.1F * 0.5F + leg->xsiz * 0.1F * 0.5F,
                         -torso->zsiz * 0.1F * (p->input.keys & MASKON(INPUT_CROUCH) ? 0.6F : 1.0F),
                         p->input.keys & MASKON(INPUT_CROUCH) ? (-torso->zsiz * 0.1F * 0.75F) : 0.0F);
        matrix_rotate(matrix_model, -45.0F * foot_function(p) * a, 1.0F, 0.0F, 0.0F);
        matrix_rotate(matrix_model, -45.0F * foot_function(p) * b, 0.0F, 0.0F, 1.0F);
        matrix_upload();
        kv6_render(leg, p->team);
        matrix_pop(matrix_model);
    }

    matrix_push(matrix_model);
    matrix_translate(matrix_model, p->physics.eye.x, p->physics.eye.y + height, p->physics.eye.z);
    if (!render_fpv)
        matrix_translate(matrix_model, 0.0F, (p->input.keys & MASKON(INPUT_CROUCH) ? 0.1F : 0.0F) - 0.1F * 2, 0.0F);
    matrix_pointAt(matrix_model, ox, oy, oz);
    matrix_rotate(matrix_model, 90.0F, 0.0F, 1.0F, 0.0F);
    if (render_fpv)
        matrix_translate(matrix_model, 0.0F, -2 * 0.1F, -2 * 0.1F);

    if (render_fpv && p->alive) {
        float speed = sqrt(pow(p->physics.velocity.x, 2) + pow(p->physics.velocity.z, 2)) / 0.25F;
        float * f = player_tool_translate_func(p);
        matrix_translate(matrix_model, f[0], f[1], 0.1F * player_swing_func(time / 1000.0F) * speed + f[2]);
    }

    if ((p->input.keys & MASKON(INPUT_SPRINT)) && !(p->input.keys & MASKON(INPUT_CROUCH)))
        matrix_rotate(matrix_model, 45.0F, 1.0F, 0.0F, 0.0F);

    if (render_fpv && window_time() - p->item_showup < 0.5F)
        matrix_rotate(matrix_model, 45.0F - (window_time() - p->item_showup) * 90.0F, 1.0F, 0.0F, 0.0F);

    if (!(p->held_item == TOOL_SPADE && render_fpv && camera_mode == CAMERAMODE_FPS)) {
        float * angles = player_tool_func(p);
        matrix_rotate(matrix_model, angles[0], 1.0F, 0.0F, 0.0F);
        matrix_rotate(matrix_model, angles[1], 0.0F, 1.0F, 0.0F);
    }
    if (render_body || settings.player_arms) {
        matrix_upload();
        kv6_render(&model_playerarms, p->team);
    }

    matrix_translate(matrix_model, -3.5F * 0.1F + 0.01F, 0.0F, 10 * 0.1F);
    if (p->held_item == TOOL_SPADE && render_fpv && window_time() - p->item_showup >= 0.5F) {
        float * angles = player_tool_func(p);
        matrix_translate(matrix_model, 0.0F, (model_spade.zpiv - model_spade.zsiz) * 0.05F, 0.0F);
        matrix_rotate(matrix_model, angles[0], 1.0F, 0.0F, 0.0F);
        matrix_rotate(matrix_model, angles[1], 0.0F, 1.0F, 0.0F);
        matrix_translate(matrix_model, 0.0F, -(model_spade.zpiv - model_spade.zsiz) * 0.05F, 0.0F);
    }

    matrix_upload();
    switch (p->held_item) {
        case TOOL_SPADE: kv6_render(&model_spade, p->team); break;
        case TOOL_BLOCK:
            model_block.red   = p->block.r / 255.0F;
            model_block.green = p->block.g / 255.0F;
            model_block.blue  = p->block.b / 255.0F;
            kv6_render(&model_block, p->team);
            break;
        case TOOL_GUN:
            // matrix_translate(matrix_model, 3.0F*0.1F-0.01F+0.025F,0.25F,-0.0625F);
            // matrix_upload();
            if (!(render_fpv && (p->input.buttons & MASKON(BUTTON_SECONDARY)))) {
                switch (p->weapon) {
                    case WEAPON_RIFLE:   kv6_render(&model_semi, p->team);    break;
                    case WEAPON_SMG:     kv6_render(&model_smg, p->team);     break;
                    case WEAPON_SHOTGUN: kv6_render(&model_shotgun, p->team); break;
                }
            }
            break;
        case TOOL_GRENADE: kv6_render(&model_grenade, p->team); break;
    }

    vec4 v = {0.1F, 0, -0.3F, 1};
    matrix_vector(matrix_model, v);
    vec4 v2 = {1.1F, 0, -0.3F, 1};
    matrix_vector(matrix_model, v2);

    p->gun_pos.x = v[0];
    p->gun_pos.y = v[1];
    p->gun_pos.z = v[2];

    p->casing_dir.x = v[0] - v2[0];
    p->casing_dir.y = v[1] - v2[1];
    p->casing_dir.z = v[2] - v2[2];

    matrix_pop(matrix_model);
}

int player_clipbox(float x, float y, float z) {
    int sz;

    if (x < 0 || x >= 512 || y < 0 || y >= 512)
        return 1;
    else if (z < 0)
        return 0;
    sz = (int)z;
    if (sz == 63)
        sz = 62;
    else if (sz >= 64)
        return 1;
    return !map_isair((int)x, 63 - sz, (int)y);
}

void player_reposition(Player * p) {
    p->physics.eye.x = p->pos.x;
    p->physics.eye.y = p->pos.y;
    p->physics.eye.z = p->pos.z;
    float f = p->physics.lastclimb - window_time();
    if (f > -0.25F && !(p->input.keys & MASKON(INPUT_CROUCH))) {
        p->physics.eye.z += (f + 0.25F) / 0.25F;
        if (&players[local_player_id] == p) {
            last_cy = 63.0F - p->physics.eye.z;
        }
    }
}

void player_coordsystem_adjust1(Player * p) {
    float tmp;

    tmp = p->pos.z;
    p->pos.z = 63.0F - p->pos.y;
    p->pos.y = tmp;

    tmp = p->physics.eye.z;
    p->physics.eye.z = 63.0F - p->physics.eye.y;
    p->physics.eye.y = tmp;

    tmp = p->physics.velocity.z;
    p->physics.velocity.z = -p->physics.velocity.y;
    p->physics.velocity.y = tmp;

    tmp = p->orientation.z;
    p->orientation.z = -p->orientation.y;
    p->orientation.y = tmp;
}

void player_coordsystem_adjust2(Player * p) {
    float tmp;

    tmp = p->pos.y;
    p->pos.y = 63.0F - p->pos.z;
    p->pos.z = tmp;

    tmp = p->physics.eye.y;
    p->physics.eye.y = 63.0F - p->physics.eye.z;
    p->physics.eye.z = tmp;

    tmp = p->physics.velocity.y;
    p->physics.velocity.y = -p->physics.velocity.z;
    p->physics.velocity.z = tmp;

    tmp = p->orientation.y;
    p->orientation.y = -p->orientation.z;
    p->orientation.z = tmp;
}

void player_boxclipmove(Player * p, float fsynctics) {
    float offset, m, f, nx, ny, nz, z;
    long climb = 0;

    f = fsynctics * 32.f;
    nx = f * p->physics.velocity.x + p->pos.x;
    ny = f * p->physics.velocity.y + p->pos.y;

    if (p->input.keys & MASKON(INPUT_CROUCH)) {
        offset = 0.45f;
        m      = 0.9f;
    } else {
        offset = 0.9f;
        m      = 1.35f;
    }

    nz = p->pos.z + offset;

    if (p->physics.velocity.x < 0)
        f = -0.45f;
    else
        f = 0.45f;
    z = m;
    while (z >= -1.36f && !player_clipbox(nx + f, p->pos.y - 0.45f, nz + z)
          && !player_clipbox(nx + f, p->pos.y + 0.45f, nz + z))
        z -= 0.9f;
    if (z < -1.36f)
        p->pos.x = nx;
    else if (!(p->input.keys & MASKON(INPUT_CROUCH)) &&
              (p->orientation.z < 0.5f)            &&
             !(p->input.keys & MASKON(INPUT_SPRINT))) {
        z = 0.35f;
        while (z >= -2.36f && !player_clipbox(nx + f, p->pos.y - 0.45f, nz + z)
              && !player_clipbox(nx + f, p->pos.y + 0.45f, nz + z))
            z -= 0.9f;
        if (z < -2.36f) {
            p->pos.x = nx;
            climb = 1;
        } else
            p->physics.velocity.x = 0;
    } else
        p->physics.velocity.x = 0;

    if (p->physics.velocity.y < 0)
        f = -0.45f;
    else
        f = 0.45f;
    z = m;
    while (z >= -1.36f && !player_clipbox(p->pos.x - 0.45f, ny + f, nz + z)
          && !player_clipbox(p->pos.x + 0.45f, ny + f, nz + z))
        z -= 0.9f;
    if (z < -1.36f)
        p->pos.y = ny;
    else if (!(p->input.keys & MASKON(INPUT_CROUCH)) &&
              (p->orientation.z < 0.5f)            &&
             !(p->input.keys & MASKON(INPUT_SPRINT)) &&
             !climb) {
        z = 0.35f;
        while (z >= -2.36f && !player_clipbox(p->pos.x - 0.45f, ny + f, nz + z)
              && !player_clipbox(p->pos.x + 0.45f, ny + f, nz + z))
            z -= 0.9f;
        if (z < -2.36f) {
            p->pos.y = ny;
            climb = 1;
        } else
            p->physics.velocity.y = 0;
    } else if (!climb)
        p->physics.velocity.y = 0;

    if (climb) {
        p->physics.velocity.x *= 0.5f;
        p->physics.velocity.y *= 0.5f;
        p->physics.lastclimb = window_time();
        nz--;
        m = -1.35f;
    } else {
        if (p->physics.velocity.z < 0)
            m = -m;
        nz += p->physics.velocity.z * fsynctics * 32.f;
    }

    p->physics.airborne = 1;

    if (player_clipbox(p->pos.x - 0.45f, p->pos.y - 0.45f, nz + m)
     || player_clipbox(p->pos.x - 0.45f, p->pos.y + 0.45f, nz + m)
     || player_clipbox(p->pos.x + 0.45f, p->pos.y - 0.45f, nz + m)
     || player_clipbox(p->pos.x + 0.45f, p->pos.y + 0.45f, nz + m)) {
        if (p->physics.velocity.z >= 0) {
            p->physics.wade = p->pos.z > 61;
            p->physics.airborne = 0;
        }
        p->physics.velocity.z = 0;
    } else
        p->pos.z = nz - offset;

    player_reposition(p);
}

int player_move(Player * p, float fsynctics, int id) {
    if (!p->alive) {
        p->physics.velocity.y -= fsynctics;
        AABB dead_bb = {.min = {0, 0, 0}, .max = {0, 0, 0}};
        aabb_set_size(&dead_bb, 0.7F, 0.15F, 0.7F);
        aabb_set_center(&dead_bb, p->pos.x + p->physics.velocity.x * fsynctics * 32.0F,
                        p->pos.y + p->physics.velocity.y * fsynctics * 32.0F,
                        p->pos.z + p->physics.velocity.z * fsynctics * 32.0F);
        if (!aabb_intersection_terrain(&dead_bb, 0)) {
            p->pos.x += p->physics.velocity.x * fsynctics * 32.0F;
            p->pos.y += p->physics.velocity.y * fsynctics * 32.0F;
            p->pos.z += p->physics.velocity.z * fsynctics * 32.0F;
        } else {
            p->physics.velocity.x *= 0.36F;
            p->physics.velocity.y *= -0.36F;
            p->physics.velocity.z *= 0.36F;
        }
        return 0;
    }

    int local = (id == local_player_id && camera_mode == CAMERAMODE_FPS);

    player_coordsystem_adjust1(p);
    float f, f2;

    // move player and perform simple physics (gravity, momentum, friction)
    if (p->physics.jump) {
        sound_create(local ? SOUND_LOCAL : SOUND_WORLD, p->physics.wade ? &sound_jump_water : &sound_jump, p->pos.x,
                     63.0F - p->pos.z, p->pos.y);
        p->physics.jump = 0;
        p->physics.velocity.z = -0.36f;
    }

    f = fsynctics; // player acceleration scalar
    if (p->physics.airborne)
        f *= 0.1f;
    else if (p->input.keys & MASKON(INPUT_CROUCH))
        f *= 0.3f;
    else if (((p->input.buttons & MASKON(BUTTON_SECONDARY)) && p->held_item == TOOL_GUN) || (p->input.keys & MASKON(INPUT_SNEAK)))
        f *= 0.5f;
    else if (p->input.keys & MASKON(INPUT_SPRINT))
        f *= 1.3f;

    if (((p->input.keys & MASKON(INPUT_UP))   || (p->input.keys & MASKON(INPUT_DOWN))) &&
        ((p->input.keys & MASKON(INPUT_LEFT)) || (p->input.keys & MASKON(INPUT_RIGHT))))
        f *= SQRT; // if strafe + forward/backwards then limit diagonal velocity

    float len = sqrt(pow(p->orientation.x, 2.0F) + pow(p->orientation.y, 2.0F));
    float sx = -p->orientation.y / len;
    float sy = p->orientation.x / len;

    if (p->input.keys & MASKON(INPUT_UP)) {
        p->physics.velocity.x += p->orientation.x * f;
        p->physics.velocity.y += p->orientation.y * f;

    } else if (p->input.keys & MASKON(INPUT_DOWN)) {
        p->physics.velocity.x -= p->orientation.x * f;
        p->physics.velocity.y -= p->orientation.y * f;
    }

    if (p->input.keys & MASKON(INPUT_LEFT)) {
        p->physics.velocity.x -= sx * f;
        p->physics.velocity.y -= sy * f;
    } else if (p->input.keys & MASKON(INPUT_RIGHT)) {
        p->physics.velocity.x += sx * f;
        p->physics.velocity.y += sy * f;
    }

    f = fsynctics + 1;
    p->physics.velocity.z += fsynctics;
    p->physics.velocity.z /= f; // air friction
    if (p->physics.wade)
        f = fsynctics * 6.0F + 1; // water friction
    else if (!p->physics.airborne)
        f = fsynctics * 4.0F + 1; // ground friction
    p->physics.velocity.x /= f;
    p->physics.velocity.y /= f;
    f2 = p->physics.velocity.z;
    player_boxclipmove(p, fsynctics);
    // hit ground... check if hurt

    int ret = 0;

    if (!p->physics.velocity.z && (f2 > FALL_SLOW_DOWN)) {
        // slow down on landing
        p->physics.velocity.x *= 0.5F;
        p->physics.velocity.y *= 0.5F;

        // return fall damage
        if (f2 > FALL_DAMAGE_VELOCITY) {
            f2 -= FALL_DAMAGE_VELOCITY;
            ret = f2 * f2 * FALL_DAMAGE_SCALAR;
            sound_create(local ? SOUND_LOCAL : SOUND_WORLD, &sound_hurt_fall, p->pos.x, 63.0F - p->pos.z, p->pos.y);
        } else {
            sound_create(local ? SOUND_LOCAL : SOUND_WORLD, p->physics.wade ? &sound_land_water : &sound_land, p->pos.x,
                         63.0F - p->pos.z, p->pos.y);
            ret = -1;
        }
    }

    player_coordsystem_adjust2(p);

    if ((p->input.keys & MASKON(INPUT_UP))   ||
        (p->input.keys & MASKON(INPUT_DOWN)) ||
        (p->input.keys & MASKON(INPUT_LEFT)) ||
        (p->input.keys & MASKON(INPUT_RIGHT))) {
        if (window_time() - p->sound.feet_started > ((p->input.keys & MASKON(INPUT_SPRINT)) ? (0.5F / 1.3F) : 0.5F)
           && (!(p->input.keys & MASKON(INPUT_CROUCH)) && !(p->input.keys & MASKON(INPUT_SNEAK)))
           && !p->physics.airborne
           && pow(p->physics.velocity.x, 2.0F) + pow(p->physics.velocity.z, 2.0F) > pow(0.125F, 2.0F)) {
            struct Sound_wav* footstep = (struct Sound_wav*[]) {
                &sound_footstep1, &sound_footstep2, &sound_footstep3, &sound_footstep4,
                &sound_wade1,      &sound_wade2,        &sound_wade3,      &sound_wade4,
            }[(rand() % 4) + (p->physics.wade ? 4 : 0)];

            if (local) {
                sound_create(SOUND_LOCAL, footstep, p->pos.x, p->pos.y, p->pos.z);
            } else {
                sound_create_sticky(footstep, p, id);
            }

            p->sound.feet_started = window_time();
        }
        if (window_time() - p->sound.feet_started_cycle > ((p->input.keys & MASKON(INPUT_SPRINT)) ? (0.5F / 1.3F) : 0.5F)) {
            p->sound.feet_started_cycle = window_time();
            p->sound.feet_cylce = !p->sound.feet_cylce;
        }
    }

    return ret;
}

int player_uncrouch(Player * p) {
    player_coordsystem_adjust1(p);
    float x1 = p->pos.x + 0.45F;
    float x2 = p->pos.x - 0.45F;
    float y1 = p->pos.y + 0.45F;
    float y2 = p->pos.y - 0.45F;
    float z1 = p->pos.z + 2.25F;
    float z2 = p->pos.z - 1.35F;

    // first check if player can lower feet (in midair)
    if (p->physics.airborne
       && !(player_clipbox(x1, y1, z1) || player_clipbox(x1, y2, z1) || player_clipbox(x2, y1, z1)
            || player_clipbox(x2, y2, z1))) {
        player_coordsystem_adjust2(p);
        return 1;
        // then check if they can raise their head
    } else if (!(player_clipbox(x1, y1, z2) || player_clipbox(x1, y2, z2) || player_clipbox(x2, y1, z2)
                || player_clipbox(x2, y2, z2))) {
        p->pos.z -= 0.9F;
        p->physics.eye.z -= 0.9F;
        if (&players[local_player_id] == p) {
            last_cy += 0.9F;
        }
        player_coordsystem_adjust2(p);
        return 1;
    }
    player_coordsystem_adjust2(p);
    return 0;
}
