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

#include <math.h>

#include <BetterSpades/common.h>
#include <BetterSpades/texture.h>
#include <BetterSpades/map.h>
#include <BetterSpades/file.h>

#include <log.h>
#include <lodepng/lodepng.c>

Texture texture[TEXTURE_TOTAL];

Texture texture_color_selection, texture_minimap, texture_dummy, texture_gradient;

const char * texture_filename(enum Texture ident) {
    switch (ident) {
        case TEXTURE_SPLASH:       return "png/splash.png";

        case TEXTURE_ZOOM_SEMI:    return "png/semi.png";
        case TEXTURE_ZOOM_SMG:     return "png/smg.png";
        case TEXTURE_ZOOM_SHOTGUN: return "png/shotgun.png";

        case TEXTURE_WHITE:        return "png/white.png";
        case TEXTURE_CROSSHAIR1:   return "png/crosshair1.png";
        case TEXTURE_CROSSHAIR2:   return "png/crosshair2.png";
        case TEXTURE_INDICATOR:    return "png/indicator.png";

        case TEXTURE_PLAYER:       return "png/player.png";
        case TEXTURE_MEDICAL:      return "png/medical.png";
        case TEXTURE_INTEL:        return "png/intel.png";
        case TEXTURE_COMMAND:      return "png/command.png";
        case TEXTURE_TRACER:       return "png/tracer.png";

#ifdef USE_TOUCH
        case TEXTURE_UI_KNOB:      return "png/ui/knob.png";
        case TEXTURE_UI_JOYSTICK:  return "png/ui/joystick.png";
#endif

        case TEXTURE_UI_RELOAD:    return "png/ui/reload.png";
        case TEXTURE_UI_BG:        return "png/ui/bg.png";
        case TEXTURE_UI_INPUT:     return "png/ui/input.png";
        case TEXTURE_UI_COLLAPSED: return "png/ui/collapsed.png";
        case TEXTURE_UI_EXPANDED:  return "png/ui/expanded.png";
        case TEXTURE_UI_FLAGS:     return "png/ui/flags.png";
        case TEXTURE_UI_ALERT:     return "png/ui/alert.png";
    }

    return NULL;
}

static char * texture_flags[] = {
    "AD", "AE", "AF", "AG", "AI", "AL", "AM", "AN", "AO", "AR", "AS", "AT", "AU", "AW", "AX", "AZ",
    "BA", "BB", "BD", "BE", "BF", "BG", "BH", "BI", "BJ", "BM", "BN", "BO", "BR", "BS", "BT", "BV",
    "BW", "BY", "BZ", "CA", "CC", "CD", "CF", "CG", "CH", "CI", "CK", "CL", "CM", "CN", "CO", "CR",
    "CS", "CU", "CV", "CX", "CY", "CZ", "DE", "DJ", "DK", "DM", "DO", "DZ", "EC", "EE", "EG", "EH",
    "ER", "ES", "ET", "FI", "FJ", "FK", "FM", "FO", "FR", "GA", "GB", "GD", "GE", "GF", "GH", "GI",
    "GL", "GM", "GN", "GP", "GQ", "GR", "GS", "GT", "GU", "GW", "GY", "HK", "HM", "HN", "HR", "HT",
    "HU", "ID", "IE", "IL", "IN", "IO", "IQ", "IR", "IS", "IT", "JM", "JO", "JP", "KE", "KG", "KH",
    "KI", "KM", "KN", "KP", "KR", "KW", "KY", "KZ", "LA", "LB", "LC", "LI", "LK", "LR", "LS", "LT",
    "LU", "LV", "LY", "MA", "MC", "MD", "ME", "MG", "MH", "MK", "ML", "MM", "MN", "MO", "MP", "MQ",
    "MR", "MS", "MT", "MU", "MV", "MW", "MX", "MY", "MZ", "NA", "NC", "NE", "NF", "NG", "NI", "NL",
    "NO", "NP", "NR", "NU", "NZ", "OM", "PA", "PE", "PF", "PG", "PH", "PK", "PL", "PM", "PN", "PR",
    "PS", "PT", "PW", "PY", "QA", "RE", "RO", "RS", "RU", "RW", "SA", "SB", "SC", "SD", "SE", "SG",
    "SH", "SI", "SJ", "SK", "SL", "SM", "SN", "SO", "SR", "ST", "SV", "SY", "SZ", "TC", "TD", "TF",
    "TG", "TH", "TJ", "TK", "TL", "TM", "TN", "TO", "TR", "TT", "TV", "TW", "TZ", "UA", "UG", "UM",
    "US", "UY", "UZ", "VA", "VC", "VE", "VG", "VI", "VN", "VU", "WF", "WS", "YE", "YT", "ZA", "ZM",
    "ZW"
};

static int texture_flag_cmp(const void * a, const void * b) {
    return strcmp(a, *(const void * const *) b);
}

int texture_flag_index(const char * country) {
    char ** res = bsearch(country, texture_flags, sizeof(texture_flags) / sizeof(texture_flags[0]), sizeof(char*), texture_flag_cmp);
    return res ? (res - texture_flags) : -1;
}

void texture_flag_offset(int index, float * u, float * v) {
    if (index >= 0) {
        *u = (index % 16) * (16.0F / 256.0F);
        *v = (index / 16) * (16.0F / 256.0F);
    } else {
        *u = 0.0625F;
        *v = 0.9375F;
    }
}

void texture_filter(Texture * t, int filter) {
    glBindTexture(GL_TEXTURE_2D, t->texture_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void texture_create(Texture * t, const char * filename, GLuint filter) {
    int sz = file_size(filename);
    void * data = file_load(filename);
    int error = lodepng_decode32(&t->pixels, &t->width, &t->height, data, sz);
    free(data);

    if (error) {
        log_warn("Could not load texture (%u): %s", error, lodepng_error_text(error));
        return;
    }

    log_info("loading texture: %s", filename);

    texture_resize_pow2(t, 0);

    glGenTextures(1, &t->texture_id);
    glBindTexture(GL_TEXTURE_2D, t->texture_id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, t->width, t->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, t->pixels);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void texture_create_buffer(Texture * t, unsigned int width, unsigned int height, unsigned char * buff, int new) {
    if (new) glGenTextures(1, &t->texture_id);
    t->width  = width;
    t->height = height;
    t->pixels = buff;
    texture_resize_pow2(t, max(width, height));

    glBindTexture(GL_TEXTURE_2D, t->texture_id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, t->width, t->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, t->pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void texture_delete(Texture * t) {
    if (t->pixels) free(t->pixels);
    glDeleteTextures(1, &t->texture_id);
}

void texture_draw_sector(Texture * t, float x, float y, float w, float h, float u, float v, float us, float vs) {
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBindTexture(GL_TEXTURE_2D, t->texture_id);

    float vertices[12] = {x, y, x, y - h, x + w, y - h, x, y, x + w, y - h, x + w, y};
    float texcoords[12] = {u, v, u, v + vs, u + us, v + vs, u, v, u + us, v + vs, u + us, v};
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glEnableClientState(GL_VERTEX_ARRAY);
    glTexCoordPointer(2, GL_FLOAT, 0, texcoords);
    glVertexPointer(2, GL_FLOAT, 0, vertices);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);

    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);
}

void texture_draw(Texture * t, float x, float y, float w, float h) {
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBindTexture(GL_TEXTURE_2D, t->texture_id);
    texture_draw_empty(x, y, w, h);
    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);
}

void texture_draw_rotated(Texture * t, float x, float y, float w, float h, float angle) {
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBindTexture(GL_TEXTURE_2D, t->texture_id);
    texture_draw_empty_rotated(x, y, w, h, angle);
    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);
}

void texture_draw_empty(float x, float y, float w, float h) {
    float vertices[12] = {x, y, x, y - h, x + w, y - h, x, y, x + w, y - h, x + w, y};
    float texcoords[12] = {0.0F, 0.0F, 0.0F, 1.0F, 1.0F, 1.0F, 0.0F, 0.0F, 1.0F, 1.0F, 1.0F, 0.0F};
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glEnableClientState(GL_VERTEX_ARRAY);
    glTexCoordPointer(2, GL_FLOAT, 0, texcoords);
    glVertexPointer(2, GL_FLOAT, 0, vertices);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);
}

#define texture_emit_rotated(tx, ty, x, y, a) cos(a) * (x)-sin(a) * (y) + (tx), sin(a) * (x) + cos(a) * (y) + (ty)

void texture_draw_empty_rotated(float x, float y, float w, float h, float angle) {
    float vertices[12]
        = {texture_emit_rotated(x, y, -w / 2, h / 2, angle), texture_emit_rotated(x, y, -w / 2, -h / 2, angle),
           texture_emit_rotated(x, y, w / 2, -h / 2, angle), texture_emit_rotated(x, y, -w / 2, h / 2, angle),
           texture_emit_rotated(x, y, w / 2, -h / 2, angle), texture_emit_rotated(x, y, w / 2, h / 2, angle)};
    float texcoords[12] = {0.0F, 0.0F, 0.0F, 1.0F, 1.0F, 1.0F, 0.0F, 0.0F, 1.0F, 1.0F, 1.0F, 0.0F};
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glEnableClientState(GL_VERTEX_ARRAY);
    glTexCoordPointer(2, GL_FLOAT, 0, texcoords);
    glVertexPointer(2, GL_FLOAT, 0, vertices);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);
}

static inline bool has_npot_textures() {
    return strstr((const char *) glGetString(GL_EXTENSIONS), "ARB_texture_non_power_of_two") != NULL;
}

void texture_resize_pow2(Texture * t, int min_size) {
    if (!t->pixels) return;

    int max_size = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_size);
    max_size = max(max_size, min_size);

    int w = 1, h = 1;
    if (has_npot_textures()) {
        if (t->width <= max_size && t->height <= max_size)
            return;
        w = t->width;
        h = t->height;
    } else {
        while (w < t->width)
            w += w;
        while (h < t->height)
            h += h;
    }

    w = min(w, max_size);
    h = min(h, max_size);

    if (t->width == w && t->height == h)
        return;

    log_info("texture original: %i:%i now: %i:%i limit: %i", t->width, t->height, w, h, max_size);

    if (!t->pixels)
        return;

    unsigned int * pixels_new = malloc(w * h * sizeof(unsigned int));
    CHECK_ALLOCATION_ERROR(pixels_new)
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            float px = (float) x / (float) w * (float) t->width;
            float py = (float) y / (float) h * (float) t->height;
            float u = px - (int) px;
            float v = py - (int) py;

            unsigned int aa = ((unsigned int *) t->pixels)[(int) px + (int) py * t->width];
            unsigned int ba = ((unsigned int *) t->pixels)[min((int) px + (int) py * t->width + 1, t->width * t->height)];
            unsigned int ab = ((unsigned int *) t->pixels)[min((int) px + (int) py * t->width + t->width, t->width * t->height)];
            unsigned int bb = ((unsigned int *) t->pixels)[min((int) px + (int) py * t->width + 1 + t->width, t->width * t->height)];

            pixels_new[x + y * w] = 0;
            pixels_new[x + y * w] |= (int) ((1.0F - v) * u * BYTE0(ba) + (1.0F - v) * (1.0F - u) * BYTE0(aa)
                                            + v * u * BYTE0(bb) + v * (1.0F - u) * BYTE0(ab));
            pixels_new[x + y * w] |= (int) ((1.0F - v) * u * BYTE1(ba) + (1.0F - v) * (1.0F - u) * BYTE1(aa)
                                            + v * u * BYTE1(bb) + v * (1.0F - u) * BYTE1(ab))
                << 8;
            pixels_new[x + y * w] |= (int) ((1.0F - v) * u * BYTE2(ba) + (1.0F - v) * (1.0F - u) * BYTE2(aa)
                                            + v * u * BYTE2(bb) + v * (1.0F - u) * BYTE2(ab))
                << 16;
            pixels_new[x + y * w] |= (int) ((1.0F - v) * u * BYTE3(ba) + (1.0F - v) * (1.0F - u) * BYTE3(aa)
                                            + v * u * BYTE3(bb) + v * (1.0F - u) * BYTE3(ab))
                << 24;
        }
    }

    t->width = w;
    t->height = h;
    free(t->pixels);
    t->pixels = (unsigned char *) pixels_new;
}

TrueColor texture_block_color(int x, int y) {
    int base[3][8] = {{15, 31, 31, 31, 0, 0, 0, 31}, {15, 0, 15, 31, 31, 31, 0, 0}, {15, 0, 0, 0, 0, 31, 31, 31}};

    uint32_t r = base[0][y] + x * (base[0][y] * 2 + 2) * (base[0][y] > 0);
    uint32_t g = base[1][y] + x * (base[1][y] * 2 + 2) * (base[1][y] > 0);
    uint32_t b = base[2][y] + x * (base[2][y] * 2 + 2) * (base[2][y] > 0);

    if (x >= 4) {
        r += ((x - 3) * 64 - 33) * (!base[0][y]);
        g += ((x - 3) * 64 - 33) * (!base[1][y]);
        b += ((x - 3) * 64 - 33) * (!base[2][y]);
    }

    return (TrueColor) {min(r, 255), min(g, 255), min(b, 255), 255};
}

void texture_gradient_fog(unsigned int * gradient) {
    int size = 512;
    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            int d = min(hypot2f(x - size / 2, y - size / 2) / (float) size * 2.0F * 255.0F, 255);
            writeRGBA(gradient + x + y * size, (TrueColor) {d, d, d, 255});
        }
    }
}

void texture_init() {
    for (enum Texture i = TEXTURE_FIRST; i <= TEXTURE_LAST; i++)
        texture_create(&texture[i], texture_filename(i), TEXTURE_FILTER_NEAREST);

    unsigned int pixels[64 * 64];
    memset(pixels, 0, sizeof(pixels));

    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            TrueColor color = texture_block_color(x, y);

            for (int ys = 0; ys < 6; ys++)
                for (int xs = 0; xs < 6; xs++)
                    writeRGBA(pixels + (x * 8 + xs) + (y * 8 + ys) * 64, color);
        }
    }

    texture_create_buffer(&texture_color_selection, 64, 64, (unsigned char *) pixels, 1);

    texture_create_buffer(&texture_minimap, map_size_x, map_size_z, NULL, 1);

    unsigned int * gradient = malloc(512 * 512 * sizeof(unsigned int));
    CHECK_ALLOCATION_ERROR(gradient)
    texture_gradient_fog(gradient);
    texture_create_buffer(&texture_gradient, 512, 512, (unsigned char *) gradient, 1);
    texture_filter(&texture_gradient, TEXTURE_FILTER_LINEAR);

    texture_create_buffer(&texture_dummy, 1, 1, (unsigned char[]) {0, 0, 0, 0}, 1);
}
