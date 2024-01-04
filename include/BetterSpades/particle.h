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

#ifndef PARTICLE_H
#define PARTICLE_H

#include <BetterSpades/player.h>

struct Particle {
    float x, y, z;
    float vx, vy, vz;
    float ox, oy, oz;
    unsigned char type;
    float size;
    float fade;
    TrueColor color;
};

void particle_init(void);
void particle_update(float dt);
void particle_render(void);
void particle_create_casing(Player *);
void particle_create(TrueColor color, float x, float y, float z, float velocity, float velocity_y, int amount,
                     float min_size, float max_size);

#define TRACE_MAX_LENGTH 128
#define MAX_TRACES       64

typedef struct { float x, y, z, value; } Vertex;

typedef struct {
    int index, begin, end;
    Vertex data[TRACE_MAX_LENGTH];
} Trace;

extern Trace traces[MAX_TRACES];

void bullet_traces_render_all(void);
void bullet_traces_reset(void);

#define NEXT(x) ((x) = ((x) + 1) % TRACE_MAX_LENGTH)

#endif
