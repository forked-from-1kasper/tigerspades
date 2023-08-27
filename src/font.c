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
#include <string.h>
#include <math.h>

#include <hashtable.h>

#include <BetterSpades/unicode.h>
#include <BetterSpades/common.h>
#include <BetterSpades/file.h>
#include <BetterSpades/font.h>
#include <BetterSpades/utils.h>

typedef struct {
    char     fontname[128];
    uint16_t high16;
    uint8_t  height;
} __attribute__((packed)) Header;

typedef struct {
    uint16_t low16;
    uint8_t  stride;
} __attribute__((packed)) RawGlyph;

typedef struct {
    uint8_t  page, stride;
    uint16_t x, y;
} Glyph;

typedef struct {
    uint16_t x, y;
} __attribute__((packed)) Pixel16;

#define BUFFSIZE 512
typedef struct {
    uint16_t len;
    Pixel16  vertex[BUFFSIZE * 4];
    Pixel16  texcoords[BUFFSIZE * 4];
} Buffer;

typedef struct {
    uint16_t high16;
    Glyph *  table;
    float    texscale;
    size_t   npages;
    Buffer * buffers;
    GLuint   textures[64];
} Subfont;

typedef struct {
    uint32_t  replacement;
    size_t    length;
    uint8_t   height;
    Subfont * special;
    Subfont * subfonts[];
} Font;

GLuint upload_page(int texsize, const uint8_t * buff) {
    GLuint texid;
    glGenTextures(1, &texid);
    glBindTexture(GL_TEXTURE_2D, texid);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, texsize, texsize, 0, GL_ALPHA, GL_UNSIGNED_BYTE, buff);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);

    return texid;
}

Subfont upload_subfont(const char * filename, int texsize, uint16_t height) {
    FILE * file = fopen(filename, "rb");

    if (!file) {
        log_fatal("ERROR: failed to open %s", filename);
        exit(1);
    }

    Header header; if (fread(&header, sizeof(Header), 1, file) != 1) {
        log_fatal("ERROR: short font header in %s", filename);
        exit(1);
    }
    header.high16 = letohu16(header.high16);

    if (header.height != height) {
        log_fatal("ERROR: invalid font height (given %d, expected %d)", header.height, height);
        exit(1);
    }

    Subfont font; font.high16 = header.high16;
    font.table = calloc(65536, sizeof(Glyph));

    uint8_t * buff = calloc(texsize * texsize, 1);
    size_t x0 = 0, y0 = 0, pagenum = 0;

    for (;;) {
        RawGlyph glyph;

        if (fread(&glyph, sizeof(RawGlyph), 1, file) < 1) {
            font.textures[pagenum] = upload_page(texsize, buff);
            break;
        }

        glyph.low16 = letohu16(glyph.low16);

        size_t size = ((size_t) header.height) * ((size_t) glyph.stride);
        uint8_t * data = malloc(size);

        if (fread(data, size, 1, file) != 1) {
            log_fatal("ERROR: malformed font %s", filename);
            exit(1);
        }

        size_t width = glyph.stride * 8;

        if (x0 + width > texsize) { x0 = 0; y0 += header.height; }
        if (y0 + header.height > texsize) {
            font.textures[pagenum] = upload_page(texsize, buff);
            x0 = y0 = 0; pagenum++;
        }

        font.table[glyph.low16].page   = pagenum;
        font.table[glyph.low16].stride = glyph.stride;

        font.table[glyph.low16].x = x0;
        font.table[glyph.low16].y = y0;

        for (size_t dy = 0; dy < header.height; dy++) {
            for (size_t dx = 0; dx < glyph.stride; dx++) {
                for (size_t bit = 0; bit < 8; bit++) {
                    size_t off = (y0 + dy) * texsize + (x0 + 8 * dx + 7 - bit);
                    buff[off] = data[dy * glyph.stride + dx] & (1 << bit) ? 0xff : 0x00;
                }
            }
        }

        x0 += width;
    }

    font.texscale = 1.0F / ((float) texsize);
    font.npages   = pagenum + 1;
    font.buffers  = calloc(font.npages, sizeof(Buffer));

    log_info("%s (0x%04xXXXX): height = %d, npages = %d", filename, font.high16, height, font.npages);

    free(buff); fclose(file); return font;
}

static enum font_type font_current_type = FONT_FIXEDSYS;

Subfont unifont, uvga;

Font primary   = {.replacement = 0xFFFD, .length = 2, .height = 16, .special = &uvga,    .subfonts = {&uvga, &unifont}};
Font secondary = {.replacement = 0xFFFD, .length = 1, .height = 16, .special = &unifont, .subfonts = {&unifont}};

Font * choose_font(enum font_type type) {
    switch (type) {
        case FONT_FIXEDSYS: return &primary;
        case FONT_SMALLFNT: return &secondary;
        default:            return NULL;
    }
}

void font_init() {
    int max_size = 0; glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_size);
    unifont = upload_subfont("fonts/unifont.bitmap", max_size, 16);
    uvga    = upload_subfont("fonts/uvga.bitmap", max_size, 16);
}

void font_select(enum font_type type) {
    font_current_type = type;
}

Subfont * get_glyph(Font * font, uint32_t codepoint, Glyph * outglyph) {
    uint16_t high16 = codepoint >> 16;

    for (size_t k = 0; k < font->length; k++) {
        Subfont * subfont = font->subfonts[k];

        if (subfont->high16 == high16) {
            Glyph glyph = subfont->table[codepoint & 0xFFFF];
            if (glyph.stride != 0) { *outglyph = glyph; return subfont; }
        }
    }

    *outglyph = font->special->table[font->replacement]; return font->special;
}

void clear_buffers(Font * font) {
    for (size_t k = 0; k < font->length; k++) {
        Subfont * subfont = font->subfonts[k];

        for (size_t i = 0; i < subfont->npages; i++)
            subfont->buffers[i].len = 0;
    }
}

float font_length(int scale, char * text, Codepage codepage) {
    Font * font = choose_font(font_current_type);

    float x = 0, length = 0;

    while (*text) {
        if (*text == '\n') {
            length = fmax(length, x);
            x = 0; text++;
        } else {
            uint32_t codepoint; text += decode(text, &codepoint, codepage);
            Glyph glyph; get_glyph(font, codepoint, &glyph); x += scale * glyph.stride * 8;
        }
    }

    return fmax(length, x);
}

void font_render(float x, float y, int scale, uint8_t * text, Codepage codepage) {
    Font * font = choose_font(font_current_type);
    clear_buffers(font);

    float x0 = x, y0 = y, h = font->height * scale;

    while (*text) {
        if (*text == '\n') {
            x0  = x;
            y0 += h;
            text++;
        } else {
            uint32_t codepoint; text += decode(text, &codepoint, codepage);

            Glyph glyph; Subfont * subfont = get_glyph(font, codepoint, &glyph);
            Buffer * buffer = &subfont->buffers[glyph.page];

            float width = glyph.stride * 8;

            buffer->texcoords[buffer->len * 4 + 0] = (Pixel16) {glyph.x, glyph.y + font->height};
            buffer->texcoords[buffer->len * 4 + 1] = (Pixel16) {glyph.x + width, glyph.y + font->height};
            buffer->texcoords[buffer->len * 4 + 2] = (Pixel16) {glyph.x + width, glyph.y};
            buffer->texcoords[buffer->len * 4 + 3] = (Pixel16) {glyph.x, glyph.y};

            buffer->vertex[buffer->len * 4 + 0] = (Pixel16) {x0, y0 - h};
            buffer->vertex[buffer->len * 4 + 1] = (Pixel16) {x0 + scale * width, y0 - h};
            buffer->vertex[buffer->len * 4 + 2] = (Pixel16) {x0 + scale * width, y0};
            buffer->vertex[buffer->len * 4 + 3] = (Pixel16) {x0, y0};

            buffer->len++; x0 += scale * width;
        }
    }

    glEnable(GL_TEXTURE_2D);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glMatrixMode(GL_TEXTURE);

    for (size_t k = 0; k < font->length; k++) {
        Subfont * subfont = font->subfonts[k];

        glLoadIdentity(); glScalef(subfont->texscale, subfont->texscale, 1.0F);

        for (size_t i = 0; i < subfont->npages; i++) {
            Buffer * buffer = &subfont->buffers[i];
            if (buffer->len == 0) continue;

            glBindTexture(GL_TEXTURE_2D, subfont->textures[i]);
            glVertexPointer(2, GL_SHORT, 0, buffer->vertex);
            glTexCoordPointer(2, GL_SHORT, 0, buffer->texcoords);
            glDrawArrays(GL_QUADS, 0, buffer->len * 4);
        }
    }

    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_BLEND);

    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);
    glDisable(GL_TEXTURE_2D);

    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();

    glMatrixMode(GL_MODELVIEW);
}

void font_centered(float x, float y, int h, uint8_t * text, Codepage codepage) {
    font_render(x - font_length(h, text, codepage) / 2.0F, y, h, text, codepage);
}
