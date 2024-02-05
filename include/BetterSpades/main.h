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

#ifndef MAIN_H
#define MAIN_H

#include <BetterSpades/window.h>
#include <stdbool.h>

void reshape(WindowInstance *, int width, int height);
void text_input(WindowInstance *, const uint8_t *);
void keys(WindowInstance *, int key, int action, int mods);
void mouse_click(WindowInstance *, int button, int action, int mods);
void mouse(WindowInstance *, double x, double y);
void mouse_scroll(WindowInstance *, double xoffset, double yoffset);
void mouse_focus(WindowInstance *, bool);
void mouse_hover(WindowInstance *, bool);
void on_error(int i, const char * s);

#endif
