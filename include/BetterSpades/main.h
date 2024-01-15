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

void reshape(struct window_instance * window, int width, int height);
void text_input(struct window_instance * window, const uint8_t *);
void keys(struct window_instance * window, int key, int action, int mods);
void mouse_click(struct window_instance * window, int button, int action, int mods);
void mouse(struct window_instance * window, double x, double y);
void mouse_scroll(struct window_instance * window, double xoffset, double yoffset);
void mouse_focus(struct window_instance *, bool);
void mouse_hover(struct window_instance *, bool);
void on_error(int i, const char * s);

#endif
