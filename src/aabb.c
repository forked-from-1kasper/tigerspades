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

#include <float.h>
#include <stdlib.h>
#include <math.h>

#include <BetterSpades/common.h>
#include <BetterSpades/aabb.h>
#include <BetterSpades/map.h>

void aabb_render(AABB * a) { }

// see: https://tavianator.com/2011/ray_box.html
bool aabb_intersection_ray(AABB * a, Ray * r, float * distance) {
    double inv_x = 1.0 / r->direction[X];
    double tx1 = (a->min[X] - r->origin[X]) * inv_x;
    double tx2 = (a->max[X] - r->origin[X]) * inv_x;

    double tmin = fmin(tx1, tx2);
    double tmax = fmax(tx1, tx2);

    double inv_y = 1.0 / r->direction[Y];
    double ty1 = (a->min[Y] - r->origin[Y]) * inv_y;
    double ty2 = (a->max[Y] - r->origin[Y]) * inv_y;

    tmin = fmax(tmin, fmin(fmin(ty1, ty2), tmax));
    tmax = fmin(tmax, fmax(fmax(ty1, ty2), tmin));

    double inv_z = 1.0 / r->direction[Z];
    double tz1 = (a->min[Z] - r->origin[Z]) * inv_z;
    double tz2 = (a->max[Z] - r->origin[Z]) * inv_z;

    tmin = fmax(tmin, fmin(fmin(tz1, tz2), tmax));
    tmax = fmin(tmax, fmax(fmax(tz1, tz2), tmin));

    if (distance) *distance = fmax(tmin, 0.0) * hypot3f(r->direction[X], r->direction[Y], r->direction[Z]);

    return tmax > fmax(tmin, 0.0);
}

void aabb_set_center(AABB * a, float x, float y, float z) {
    float size_x = a->max[X] - a->min[X];
    float size_y = a->max[Y] - a->min[Y];
    float size_z = a->max[Z] - a->min[Z];

    a->min[X] = x - size_x / 2;
    a->min[Y] = y - size_y / 2;
    a->min[Z] = z - size_z / 2;
    a->max[X] = x + size_x / 2;
    a->max[Y] = y + size_y / 2;
    a->max[Z] = z + size_z / 2;
}

void aabb_set_size(AABB * a, float x, float y, float z) {
    a->max[X] = a->min[X] + x;
    a->max[Y] = a->min[Y] + y;
    a->max[Z] = a->min[Z] + z;
}

bool aabb_intersection(AABB * a, AABB * b) {
    return (a->min[X] <= b->max[X] && b->min[X] <= a->max[X])
        && (a->min[Y] <= b->max[Y] && b->min[Y] <= a->max[Y])
        && (a->min[Z] <= b->max[Z] && b->min[Z] <= a->max[Z]);
}

bool aabb_intersection_terrain(AABB * a, int miny) {
    AABB terrain_cube;

    int min_x = min(max(floor(a->min[X]) - 1, 0),    map_size_x);
    int min_y = min(max(floor(a->min[Y]) - 1, miny), map_size_y);
    int min_z = min(max(floor(a->min[Z]) - 1, 0),    map_size_z);

    int max_x = min(max(ceil(a->max[X]) + 1, 0), map_size_x);
    int max_y = min(max(ceil(a->max[Y]) + 1, 0), map_size_y);
    int max_z = min(max(ceil(a->max[Z]) + 1, 0), map_size_z);

    for (int x = min_x; x < max_x; x++) {
        for (int z = min_z; z < max_z; z++) {
            for (int y = min_y; y < max_y; y++) {
                if (!map_isair(x, y, z)) {
                    terrain_cube.min[X] = x;
                    terrain_cube.min[Y] = y;
                    terrain_cube.min[Z] = z;
                    terrain_cube.max[X] = x + 1;
                    terrain_cube.max[Y] = y + 1;
                    terrain_cube.max[Z] = z + 1;

                    if (aabb_intersection(a, &terrain_cube))
                        return true;
                }
            }
        }
    }

    return false;
}
