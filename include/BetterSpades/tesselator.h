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

#ifndef TESSELATOR_H
#define TESSELATOR_H

#include <stdint.h>

#include <BetterSpades/glx.h>

#ifdef OPENGL_ES
#define TESSELATE_TRIANGLES
#else
#define TESSELATE_QUADS
#endif

typedef enum {
    VERTEX_INT,
    VERTEX_FLOAT,
} TesselatorVertexType;

typedef struct {
    void * vertices;
    int8_t * normals;
    uint32_t * colors;
    uint32_t quad_count;
    uint32_t quad_space;
    int has_normal;
    TrueColor color;
    int8_t normal[3];
    TesselatorVertexType vertex_type;
} Tesselator;

typedef enum {
    CUBE_FACE_X_N,
    CUBE_FACE_X_P,
    CUBE_FACE_Y_N,
    CUBE_FACE_Y_P,
    CUBE_FACE_Z_N,
    CUBE_FACE_Z_P,
} TesselatorCubeFace;

void tesselator_create(Tesselator *, TesselatorVertexType type, int has_normal);
void tesselator_clear(Tesselator *);
void tesselator_free(Tesselator *);
void tesselator_draw(Tesselator *, int with_color);
void tesselator_glx(Tesselator *, GLXDisplayList *);
void tesselator_set_color(Tesselator *, TrueColor color);
void tesselator_set_normal(Tesselator *, int8_t x, int8_t y, int8_t z);
void tesselator_addi(Tesselator *, int16_t * coords, TrueColor * colors, int8_t * normals);
void tesselator_addf(Tesselator *, float * coords, TrueColor * colors, int8_t * normals);
void tesselator_addi_simple(Tesselator *, int16_t * coords);
void tesselator_addf_simple(Tesselator *, float * coords);
void tesselator_addi_cube_face(Tesselator *, TesselatorCubeFace face, int16_t x, int16_t y, int16_t z);
void tesselator_addi_cube_face_adv(Tesselator *, TesselatorCubeFace face, int16_t x, int16_t y,
                                   int16_t z, int16_t sx, int16_t sy, int16_t sz);
void tesselator_addf_cube_face(Tesselator *, TesselatorCubeFace face, float x, float y, float z,
                               float sz);

#endif
