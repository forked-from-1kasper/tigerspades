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

#include <BetterSpades/common.h>

#ifndef TEXTURE_H
#define TEXTURE_H

typedef struct _Texture Texture;

enum Texture {
    TEXTURE_SPLASH,

    TEXTURE_ZOOM_SEMI,
    TEXTURE_ZOOM_SMG,
    TEXTURE_ZOOM_SHOTGUN,

    TEXTURE_WHITE,
    TEXTURE_CROSSHAIR1,
    TEXTURE_CROSSHAIR2,
    TEXTURE_INDICATOR,

    TEXTURE_PLAYER,
    TEXTURE_MEDICAL,
    TEXTURE_INTEL,
    TEXTURE_COMMAND,
    TEXTURE_TRACER,

#ifdef USE_TOUCH
    TEXTURE_UI_KNOB,
    TEXTURE_UI_JOYSTICK,
#endif

    TEXTURE_UI_RELOAD,
    TEXTURE_UI_BG,
    TEXTURE_UI_INPUT,
    TEXTURE_UI_COLLAPSED,
    TEXTURE_UI_EXPANDED,
    TEXTURE_UI_FLAGS,
    TEXTURE_UI_ALERT,

    TEXTURE_FIRST = TEXTURE_SPLASH,
    TEXTURE_LAST  = TEXTURE_UI_ALERT
};

extern Texture * const texture_color_selection;
extern Texture * const texture_minimap;
extern Texture * const texture_dummy;
extern Texture * const texture_gradient;
Texture * texture(enum Texture);

typedef enum {
    TEXTURE_FILTER_NEAREST,
    TEXTURE_FILTER_LINEAR
} Filtering;

const char * texture_filename(enum Texture);

int texture_flag_index(const char * country);
void texture_flag_offset(int index, float * u, float * v);
void texture_filter(Texture *, Filtering filter);
void texture_init(void);
void texture_load(enum Texture, Filtering filter);
void texture_create_buffer(Texture *, const char *, unsigned int width, unsigned int height, unsigned char * buff, int new);
void texture_delete(Texture *);
void texture_draw(Texture *, float x, float y, float w, float h);
void texture_draw_sector(Texture *, float x, float y, float w, float h, float u, float v, float us, float vs);
void texture_draw_empty(float x, float y, float w, float h);
void texture_draw_empty_rotated(float x, float y, float w, float h, float angle);
void texture_draw_rotated(Texture *, float x, float y, float w, float h, float angle);
void texture_resize_pow2(Texture *, const char *, int min_size);
TrueColor texture_block_color(int x, int y);
void texture_gradient_fog(unsigned int *);
void texture_bind(Texture *);
float texture_width(Texture *);
float texture_height(Texture *);
Texture * texture_alloc();
void texture_subimage(Texture *, int xoffset, int yoffset, size_t width, size_t height, const void *);

#endif
