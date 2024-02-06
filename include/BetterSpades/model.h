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

#ifndef MODEL_H
#define MODEL_H

#include <stdint.h>
#include <stdbool.h>

#include <BetterSpades/common.h>
#include <BetterSpades/aabb.h>
#include <BetterSpades/glx.h>
#include <BetterSpades/tesselator.h>

#define KV6_VIS_NEG_X (1 << 0)
#define KV6_VIS_POS_X (1 << 1)
#define KV6_VIS_NEG_Z (1 << 2)
#define KV6_VIS_POS_Z (1 << 3)
#define KV6_VIS_POS_Y (1 << 4)
#define KV6_VIS_NEG_Y (1 << 5)

typedef struct {
    uint16_t x, y, z;
    uint8_t visfaces;
    TrueColor color;
} Voxel;

typedef struct {
    uint16_t xsiz, ysiz, zsiz;
    float xpiv, ypiv, zpiv;
    bool has_display_list, colorize;
    GLXDisplayList display_list[2];
    Voxel * voxels;
    int voxel_count;
    float scale;
    float red, green, blue;
} kv6;

extern kv6 model_playerdead;
extern kv6 model_playerhead;
extern kv6 model_playertorso;
extern kv6 model_playertorsoc;
extern kv6 model_playerarms;
extern kv6 model_playerleg;
extern kv6 model_playerlegc;
extern kv6 model_intel;
extern kv6 model_tent;

extern kv6 model_semi;
extern kv6 model_smg;
extern kv6 model_shotgun;
extern kv6 model_spade;
extern kv6 model_block;
extern kv6 model_grenade;

extern kv6 model_semi_tracer;
extern kv6 model_smg_tracer;
extern kv6 model_shotgun_tracer;

extern kv6 model_semi_casing;
extern kv6 model_smg_casing;
extern kv6 model_shotgun_casing;

void kv6_calclight(int x, int y, int z);
void kv6_rebuild_complete(void);
void kv6_rebuild(kv6 *);
void kv6_render(kv6 *, unsigned char team);
void kv6_load(kv6 *, uint8_t * bytes, float scale);
void kv6_init(void);

extern float kv6_normals[256][3];

#endif
