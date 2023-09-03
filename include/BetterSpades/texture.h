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

typedef struct {
    int width, height;
    int texture_id;
    unsigned char * pixels;
} Texture;

extern Texture texture_splash;
extern Texture texture_minimap;
extern Texture texture_gradient;
extern Texture texture_dummy;

extern Texture texture_health;
extern Texture texture_block;
extern Texture texture_grenade;
extern Texture texture_ammo_semi;
extern Texture texture_ammo_smg;
extern Texture texture_ammo_shotgun;

extern Texture texture_color_selection;

extern Texture texture_zoom_semi;
extern Texture texture_zoom_smg;
extern Texture texture_zoom_shotgun;

extern Texture texture_white;
extern Texture texture_target;
extern Texture texture_indicator;

extern Texture texture_player;
extern Texture texture_medical;
extern Texture texture_intel;
extern Texture texture_command;
extern Texture texture_tracer;

extern Texture texture_ui_wait;
extern Texture texture_ui_join;
extern Texture texture_ui_reload;
extern Texture texture_ui_bg;
extern Texture texture_ui_input;
extern Texture texture_ui_box_empty;
extern Texture texture_ui_box_check;
extern Texture texture_ui_expanded;
extern Texture texture_ui_collapsed;
extern Texture texture_ui_flags;
extern Texture texture_ui_alert;
extern Texture texture_ui_joystick;
extern Texture texture_ui_knob;

#define TEXTURE_FILTER_NEAREST GL_NEAREST
#define TEXTURE_FILTER_LINEAR  GL_LINEAR

int texture_flag_index(const char * country);
void texture_flag_offset(int index, float * u, float * v);
void texture_filter(Texture * t, int filter);
void texture_init(void);
int texture_create(Texture * t, char * filename, GLuint filter);
int texture_create_buffer(Texture * t, int width, int height, unsigned char * buff, int new);
void texture_delete(Texture * t);
void texture_draw(Texture * t, float x, float y, float w, float h);
void texture_draw_sector(Texture * t, float x, float y, float w, float h, float u, float v, float us, float vs);
void texture_draw_empty(float x, float y, float w, float h);
void texture_draw_empty_rotated(float x, float y, float w, float h, float angle);
void texture_draw_rotated(Texture * t, float x, float y, float w, float h, float angle);
void texture_resize_pow2(Texture * t, int min_size);
TrueColor texture_block_color(int x, int y);
void texture_gradient_fog(unsigned int *);

#endif
