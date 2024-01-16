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

#include <math.h>
#include <string.h>
#include <float.h>

#include <BetterSpades/common.h>
#include <BetterSpades/cameracontroller.h>
#include <BetterSpades/player.h>
#include <BetterSpades/map.h>
#include <BetterSpades/matrix.h>
#include <BetterSpades/camera.h>
#include <BetterSpades/config.h>

enum camera_mode camera_mode = CAMERAMODE_SPECTATOR;

float frustum[6][4];
float camera_rot_x = 2.04F, camera_rot_y = 1.79F;
float camera_x = 256.0F, camera_y = 60.0F, camera_z = 256.0F;
float camera_vx, camera_vy, camera_vz;
float camera_size = 0.8F;
float camera_height = 0.8F;
float camera_eye_height = 0.0F;
float camera_movement_x = 0.0F, camera_movement_y = 0.0F, camera_movement_z = 0.0F;
float camera_speed = 32.0F;

float camera_fov_scaled() {
    int render_fpv = (camera_mode == CAMERAMODE_FPS)
        || ((camera_mode == CAMERAMODE_BODYVIEW || camera_mode == CAMERAMODE_SPECTATOR)
            && cameracontroller_bodyview_mode);
    int local_id = (camera_mode == CAMERAMODE_FPS) ? local_player_id : cameracontroller_bodyview_player;

    if (render_fpv && players[local_id].held_item == TOOL_GUN && (players[local_id].input.buttons & MASKON(BUTTON_SECONDARY))
       && !(players[local_id].input.keys & MASKON(INPUT_SPRINT)) && players[local_id].alive)
        return CAMERA_DEFAULT_FOV * atan(tan((CAMERA_DEFAULT_FOV / 180.0F * PI) / 2) / 2.0F) * 2.0F;
    return settings.camera_fov;
}

void camera_overflow_adjust() {
    if (camera_rot_y < EPSILON) {
        camera_rot_y = EPSILON;
    }

    if (camera_rot_y > 3.14F) {
        camera_rot_y = 3.14F;
    }

    if (camera_rot_x > DOUBLEPI) {
        camera_rot_x -= DOUBLEPI;
    }

    if (camera_rot_x < 0.0F) {
        camera_rot_x += DOUBLEPI;
    }
}

void camera_apply() {
    switch (camera_mode) {
        case CAMERAMODE_FPS:       cameracontroller_fps_render();       break;
        case CAMERAMODE_BODYVIEW:  cameracontroller_bodyview_render();  break;
        case CAMERAMODE_SPECTATOR: cameracontroller_spectator_render(); break;
        case CAMERAMODE_SELECTION: cameracontroller_selection_render(); break;
        case CAMERAMODE_DEATH:     cameracontroller_death_render();     break;
    }
}

void camera_update(float dt) {
    switch (camera_mode) {
        case CAMERAMODE_FPS:       cameracontroller_fps(dt);       break;
        case CAMERAMODE_BODYVIEW:  cameracontroller_bodyview(dt);  break;
        case CAMERAMODE_SPECTATOR: cameracontroller_spectator(dt); break;
        case CAMERAMODE_SELECTION: cameracontroller_selection(dt); break;
        case CAMERAMODE_DEATH:     cameracontroller_death(dt);     break;
    }
}

void camera_hit_fromplayer(struct Camera_HitType* hit, int player_id, float range) {
    if (player_id != local_player_id) {
        camera_hit(hit, player_id, players[player_id].physics.eye.x,
                   players[player_id].physics.eye.y + player_height(&players[player_id]),
                   players[player_id].physics.eye.z, players[player_id].orientation.x, players[player_id].orientation.y,
                   players[player_id].orientation.z, range);
    } else {
        camera_hit(hit, player_id, players[player_id].physics.eye.x,
                   players[player_id].physics.eye.y + player_height(&players[player_id]),
                   players[player_id].physics.eye.z, sin(camera_rot_x) * sin(camera_rot_y), cos(camera_rot_y),
                   cos(camera_rot_x) * sin(camera_rot_y), range);
    }
}

void camera_hit(struct Camera_HitType * hit, int exclude_player, float x, float y, float z, float ray_x, float ray_y,
                float ray_z, float range) {
    camera_hit_mask(hit, exclude_player, x, y, z, ray_x, ray_y, ray_z, range);
}

void camera_hit_mask(struct Camera_HitType * hit, int exclude_player, float x, float y, float z, float ray_x,
                     float ray_y, float ray_z, float range) {
    Ray dir = (Ray) {
        .origin = {x, y, z},
        .direction = {ray_x, ray_y, ray_z},
    };

    hit->type = CAMERA_HITTYPE_NONE;
    hit->distance = FLT_MAX;

#if HACKS_ENABLED && HACK_WALLHACK
    if (players[local_player_id].held_item != TOOL_GUN) {
#endif
    int * pos = camera_terrain_pickEx(1, x, y, z, ray_x, ray_y, ray_z);
    if (pos != NULL && distance3D(x, y, z, pos[0], pos[1], pos[2]) <= range * range) {
        AABB block = (AABB) {
            .min = {pos[0], pos[1], pos[2]},
            .max = {pos[0] + 1, pos[1] + 1, pos[2] + 1},
        };

        float d;
        if (aabb_intersection_ray(&block, &dir, &d)) {
            hit->type = CAMERA_HITTYPE_BLOCK;
            hit->distance = d;
            hit->x = pos[0];
            hit->y = pos[1];
            hit->z = pos[2];
            hit->xb = pos[3];
            hit->yb = pos[4];
            hit->zb = pos[5];
        }
    }
#if HACKS_ENABLED && HACK_WALLHACK
    }
#endif

    for (int i = 0; i < PLAYERS_MAX; i++) {
        float l = distance2D(x, z, players[i].pos.x, players[i].pos.z);
        if (players[i].connected && players[i].alive && l < range * range
           && (exclude_player < 0 || (exclude_player >= 0 && exclude_player != i))) {
            Hit intersects = {0};
            player_collision(players + i, &dir, &intersects);

            float d; int type = player_intersection_choose(&intersects, &d);
            if (player_intersection_exists(&intersects) && d < hit->distance) {
                hit->type           = CAMERA_HITTYPE_PLAYER;
                hit->distance       = d;
                hit->x              = players[i].pos.x;
                hit->y              = players[i].pos.y;
                hit->z              = players[i].pos.z;
                hit->player_id      = i;
                hit->player_section = type;
            }
        }
    }
}

int * camera_terrain_pick(unsigned char mode) {
    return camera_terrain_pickEx(mode, camera_x, camera_y, camera_z, sin(camera_rot_x) * sin(camera_rot_y),
                                 cos(camera_rot_y), cos(camera_rot_x) * sin(camera_rot_y));
}

// kindly borrowed from
// https://stackoverflow.com/questions/16505905/walk-a-line-between-two-points-in-a-3d-voxel-space-visiting-all-cells
// adapted, original code by Wivlaro
int * camera_terrain_pickEx(unsigned char mode, float gx0, float gy0, float gz0, float ray_x, float ray_y, float ray_z) {
    float gx1 = gx0 + ray_x * 128.0F;
    float gy1 = gy0 + ray_y * 128.0F;
    float gz1 = gz0 + ray_z * 128.0F;

    int gx0idx = floor(gx0);
    int gy0idx = floor(gy0);
    int gz0idx = floor(gz0);

    int gx1idx = floor(gx1);
    int gy1idx = floor(gy1);
    int gz1idx = floor(gz1);

    int sx = gx1idx > gx0idx ? 1 : gx1idx < gx0idx ? -1 : 0;
    int sy = gy1idx > gy0idx ? 1 : gy1idx < gy0idx ? -1 : 0;
    int sz = gz1idx > gz0idx ? 1 : gz1idx < gz0idx ? -1 : 0;

    int gx = gx0idx;
    int gy = gy0idx;
    int gz = gz0idx;

    int gxp = gx0idx + (gx1idx > gx0idx ? 1 : 0);
    int gyp = gy0idx + (gy1idx > gy0idx ? 1 : 0);
    int gzp = gz0idx + (gz1idx > gz0idx ? 1 : 0);

    float vx = gx1 == gx0 ? 1 : gx1 - gx0;
    float vy = gy1 == gy0 ? 1 : gy1 - gy0;
    float vz = gz1 == gz0 ? 1 : gz1 - gz0;

    float vxvy = vx * vy;
    float vxvz = vx * vz;
    float vyvz = vy * vz;

    float errx = (gxp - gx0) * vyvz;
    float erry = (gyp - gy0) * vxvz;
    float errz = (gzp - gz0) * vxvy;

    float derrx = sx * vyvz;
    float derry = sy * vxvz;
    float derrz = sz * vxvy;

    int gx_pre = gx, gy_pre = gy, gz_pre = gz;

    static int ret[6];
    ret[0] = ret[1] = ret[2] = 0;
    ret[3] = ret[4] = ret[5] = 0;

    for (;;) {
        if (gx >= map_size_x || gx < 0 || gy < 0 || gz >= map_size_z || gz < 0) {
            return NULL;
        }
        switch (mode) {
            case 0:
                if (!map_isair(gx, gy, gz) && map_isair(gx_pre, gy_pre, gz_pre)) {
                    ret[0] = gx_pre;
                    ret[1] = gy_pre;
                    ret[2] = gz_pre;
                    return ret;
                }
                break;
            case 1:
                if (!map_isair(gx, gy, gz)) {
                    ret[0] = gx;
                    ret[1] = gy;
                    ret[2] = gz;
                    ret[3] = gx_pre;
                    ret[4] = gy_pre;
                    ret[5] = gz_pre;
                    return ret;
                }
                break;
        }
        gx_pre = gx;
        gy_pre = gy;
        gz_pre = gz;

        if (gx == gx1idx && gy == gy1idx && gz == gz1idx)
            break;

        int xr = abs(errx);
        int yr = abs(erry);
        int zr = abs(errz);

        if (sx != 0 && (sy == 0 || xr < yr) && (sz == 0 || xr < zr)) {
            gx += sx;
            errx += derrx;
        } else if (sy != 0 && (sz == 0 || yr < zr)) {
            gy += sy;
            erry += derry;
        } else if (sz != 0) {
            gz += sz;
            errz += derrz;
        }
    }

    return NULL;
}

void camera_ExtractFrustum() {
    float clip[16];
    float t;

    mat4 mvp;
    matrix_load(mvp, matrix_model);
    matrix_multiply(mvp, matrix_view);
    matrix_multiply(mvp, matrix_projection);
    memcpy(clip, (float*) mvp, 16 * sizeof(float));

    /* Extract the numbers for the RIGHT plane */
    frustum[0][0] = clip[3]  - clip[0];
    frustum[0][1] = clip[7]  - clip[4];
    frustum[0][2] = clip[11] - clip[8];
    frustum[0][3] = clip[15] - clip[12];

    /* Normalize the result */
    t = sqrt(frustum[0][0] * frustum[0][0] + frustum[0][1] * frustum[0][1] + frustum[0][2] * frustum[0][2]);
    frustum[0][0] /= t;
    frustum[0][1] /= t;
    frustum[0][2] /= t;
    frustum[0][3] /= t;

    /* Extract the numbers for the LEFT plane */
    frustum[1][0] = clip[3]  + clip[0];
    frustum[1][1] = clip[7]  + clip[4];
    frustum[1][2] = clip[11] + clip[8];
    frustum[1][3] = clip[15] + clip[12];

    /* Normalize the result */
    t = sqrt(frustum[1][0] * frustum[1][0] + frustum[1][1] * frustum[1][1] + frustum[1][2] * frustum[1][2]);
    frustum[1][0] /= t;
    frustum[1][1] /= t;
    frustum[1][2] /= t;
    frustum[1][3] /= t;

    /* Extract the BOTTOM plane */
    frustum[2][0] = clip[3]  + clip[1];
    frustum[2][1] = clip[7]  + clip[5];
    frustum[2][2] = clip[11] + clip[9];
    frustum[2][3] = clip[15] + clip[13];

    /* Normalize the result */
    t = sqrt(frustum[2][0] * frustum[2][0] + frustum[2][1] * frustum[2][1] + frustum[2][2] * frustum[2][2]);
    frustum[2][0] /= t;
    frustum[2][1] /= t;
    frustum[2][2] /= t;
    frustum[2][3] /= t;

    /* Extract the TOP plane */
    frustum[3][0] = clip[3]  - clip[1];
    frustum[3][1] = clip[7]  - clip[5];
    frustum[3][2] = clip[11] - clip[9];
    frustum[3][3] = clip[15] - clip[13];

    /* Normalize the result */
    t = sqrt(frustum[3][0] * frustum[3][0] + frustum[3][1] * frustum[3][1] + frustum[3][2] * frustum[3][2]);
    frustum[3][0] /= t;
    frustum[3][1] /= t;
    frustum[3][2] /= t;
    frustum[3][3] /= t;

    /* Extract the FAR plane */
    frustum[4][0] = clip[3]  - clip[2];
    frustum[4][1] = clip[7]  - clip[6];
    frustum[4][2] = clip[11] - clip[10];
    frustum[4][3] = clip[15] - clip[14];

    /* Normalize the result */
    t = sqrt(frustum[4][0] * frustum[4][0] + frustum[4][1] * frustum[4][1] + frustum[4][2] * frustum[4][2]);
    frustum[4][0] /= t;
    frustum[4][1] /= t;
    frustum[4][2] /= t;
    frustum[4][3] /= t;

    /* Extract the NEAR plane */
    frustum[5][0] = clip[3]  + clip[2];
    frustum[5][1] = clip[7]  + clip[6];
    frustum[5][2] = clip[11] + clip[10];
    frustum[5][3] = clip[15] + clip[14];

    /* Normalize the result */
    t = sqrt(frustum[5][0] * frustum[5][0] + frustum[5][1] * frustum[5][1] + frustum[5][2] * frustum[5][2]);
    frustum[5][0] /= t;
    frustum[5][1] /= t;
    frustum[5][2] /= t;
    frustum[5][3] /= t;
}

unsigned char camera_PointInFrustum(float x, float y, float z) {
    int p;

    for (p = 0; p < 6; p++)
        if (frustum[p][0] * x + frustum[p][1] * y + frustum[p][2] * z + frustum[p][3] <= 0)
            return 0;

    return 1;
}

int camera_CubeInFrustum(float x, float y, float z, float size, float size_y) {
    int p;
    int c;
    int c2 = 0;

    for (p = 0; p < 6; p++) {
        c = 0;
        if (frustum[p][0] * (x - size) + frustum[p][1] * (y - size) + frustum[p][2] * (z - size) + frustum[p][3] > 0)
            c++;
        if (frustum[p][0] * (x + size) + frustum[p][1] * (y - size) + frustum[p][2] * (z - size) + frustum[p][3] > 0)
            c++;
        if (frustum[p][0] * (x - size) + frustum[p][1] * (y + size_y) + frustum[p][2] * (z - size) + frustum[p][3] > 0)
            c++;
        if (frustum[p][0] * (x + size) + frustum[p][1] * (y + size_y) + frustum[p][2] * (z - size) + frustum[p][3] > 0)
            c++;
        if (frustum[p][0] * (x - size) + frustum[p][1] * (y) + frustum[p][2] * (z + size) + frustum[p][3] > 0)
            c++;
        if (frustum[p][0] * (x + size) + frustum[p][1] * (y) + frustum[p][2] * (z + size) + frustum[p][3] > 0)
            c++;
        if (frustum[p][0] * (x - size) + frustum[p][1] * (y + size_y) + frustum[p][2] * (z + size) + frustum[p][3] > 0)
            c++;
        if (frustum[p][0] * (x + size) + frustum[p][1] * (y + size_y) + frustum[p][2] * (z + size) + frustum[p][3] > 0)
            c++;
        if (c == 0)
            return 0;
        if (c == 8)
            c2++;
    }

    return (c2 == 6) ? 2 : 1;
}
