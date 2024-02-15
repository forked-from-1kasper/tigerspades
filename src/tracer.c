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

#include <BetterSpades/common.h>
#include <BetterSpades/matrix.h>
#include <BetterSpades/window.h>
#include <BetterSpades/tracer.h>
#include <BetterSpades/model.h>
#include <BetterSpades/camera.h>
#include <BetterSpades/texture.h>
#include <BetterSpades/config.h>
#include <BetterSpades/sound.h>
#include <BetterSpades/entitysystem.h>

EntitySystem tracers;

void tracer_pvelocity(float * o, Player * p) {
    o[X] = o[X] * 256.0F / 32.0F + p->physics.velocity.x;
    o[Y] = o[Y] * 256.0F / 32.0F + p->physics.velocity.y;
    o[Z] = o[Z] * 256.0F / 32.0F + p->physics.velocity.z;
}

typedef struct {
    int   large;
    float scalef;
    float minimap_x;
    float minimap_y;
} TracerMinimapInfo;

static bool tracer_minimap_single(void * obj, void * user) {
    Tracer * t = (Tracer *) obj;
    TracerMinimapInfo * info = (TracerMinimapInfo *) user;

    if (info->large) {
        float ang = -atan2(t->r.direction[Z], t->r.direction[X]) - HALFPI;
        texture_draw_rotated(texture(TEXTURE_TRACER), info->minimap_x + t->r.origin[X] * info->scalef,
                             info->minimap_y - t->r.origin[Z] * info->scalef, 15 * info->scalef, 15 * info->scalef, ang);
    } else {
        float tracer_x = t->r.origin[X] - info->minimap_x;
        float tracer_y = t->r.origin[Z] - info->minimap_y;

        if (tracer_x > 0.0F && tracer_x < 128.0F && tracer_y > 0.0F && tracer_y < 128.0F) {
            float ang = -atan2(t->r.direction[Z], t->r.direction[X]) - HALFPI;
            texture_draw_rotated(texture(TEXTURE_TRACER), settings.window_width - 143 * info->scalef + tracer_x * info->scalef,
                                 settings.window_height - (15 + tracer_y) * info->scalef, 15 * info->scalef, 15 * info->scalef, ang);
        }
    }

    return false;
}

void tracer_minimap(int large, float scalef, float minimap_x, float minimap_y) {
    entitysys_iterate(&tracers,
                      &(TracerMinimapInfo) {
                          .large     = large,
                          .scalef    = scalef,
                          .minimap_x = minimap_x,
                          .minimap_y = minimap_y,
                      },
                      tracer_minimap_single);
}

void tracer_add(int type, float x, float y, float z, float dx, float dy, float dz) {
    Tracer t = (Tracer) {
        .type = type,
        .x = t.r.origin[X] = x + dx / 4.0F,
        .y = t.r.origin[Y] = y + dy / 4.0F,
        .z = t.r.origin[Z] = z + dz / 4.0F,
        .r.direction[X] = dx,
        .r.direction[Y] = dy,
        .r.direction[Z] = dz,
        .created = window_time(),
    };

    float len = hypot3f(dx, dy, dz);
    camera_hit(&t.hit, -1, t.x, t.y, t.z, dx / len, dy / len, dz / len, 128.0F);

    entitysys_add(&tracers, &t);
}

static bool tracer_render_single(void * obj, void * user) {
    Tracer * t = (Tracer *) obj;

    static enum kv6 model_tracer[] = {
        [WEAPON_RIFLE]   = MODEL_SEMI_TRACER,
        [WEAPON_SMG]     = MODEL_SMG_TRACER,
        [WEAPON_SHOTGUN] = MODEL_SHOTGUN_TRACER
    };

    matrix_push(matrix_model);
    matrix_translate(matrix_model, t->r.origin[X], t->r.origin[Y], t->r.origin[Z]);
    matrix_pointAt(matrix_model, t->r.direction[X], t->r.direction[Y], t->r.direction[Z]);
    matrix_rotate(matrix_model, 90.0F, 0.0F, 1.0F, 0.0F);
    matrix_upload();
    kv6_render(&model[model_tracer[t->type]], TEAM_SPECTATOR);
    matrix_pop(matrix_model);

    return false;
}

void tracer_render() {
    entitysys_iterate(&tracers, NULL, tracer_render_single);
}

static bool tracer_update_single(void * obj, void * user) {
    Tracer * t = (Tracer *) obj;
    float dt = *(float *) user;

    float len = norm3f(t->x, t->y, t->z, t->r.origin[X], t->r.origin[Y], t->r.origin[Z]);

    // 128.0[m] / 256.0[m/s] = 0.5[s]
    if ((t->hit.type != CAMERA_HITTYPE_NONE && len > sqrf(t->hit.distance)) || window_time() - t->created > 0.5F) {
        if (t->hit.type != CAMERA_HITTYPE_NONE)
            sound_create(SOUND_WORLD, &sound_impact, t->r.origin[X], t->r.origin[Y], t->r.origin[Z]);

        return true;
    } else {
        t->r.origin[X] += t->r.direction[X] * 32.0F * dt;
        t->r.origin[Y] += t->r.direction[Y] * 32.0F * dt;
        t->r.origin[Z] += t->r.direction[Z] * 32.0F * dt;
    }

    return false;
}

void tracer_update(float dt) {
    entitysys_iterate(&tracers, &dt, tracer_update_single);
}

void tracer_init() {
    entitysys_create(&tracers, sizeof(Tracer), PLAYERS_MAX);
}
