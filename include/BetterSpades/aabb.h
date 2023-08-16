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

#ifndef AABB_H
#define AABB_H

#include <stdbool.h>

#include <cglm/vec3.h>

#define X 0
#define Y 1
#define Z 2

typedef struct { vec3 min, max; } AABB;
typedef struct { vec3 origin, direction; } Ray;

bool aabb_intersection(AABB * a, AABB * b);
bool aabb_intersection_ray(AABB * a, Ray * r, float * distance);
bool aabb_intersection_terrain(AABB * a, int miny);
void aabb_set_size(AABB * a, float x, float y, float z);
void aabb_set_center(AABB * a, float x, float y, float z);
void aabb_render(AABB * a);

#endif
