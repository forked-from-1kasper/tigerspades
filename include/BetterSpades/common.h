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

#ifndef COMMON_H
#define COMMON_H

#if defined(OS_WINDOWS)
    #define OS "Windows"
#elif defined(OS_LINUX)
    #define OS "Linux"
#elif defined(OS_APPLE)
    #define OS "Mac"
#elif defined(OS_HAIKU)
    #define OS "Haiku"
#elif defined(USE_TOUCH)
    #define OS "Android"
#else
    #define OS "Unknown"
#endif

#if defined(__alpha__)
    #define ARCH "Alpha"
#elif defined(__x86_64__)
    #define ARCH "x86-64"
#elif defined(__arm__)
    #define ARCH "ARM"
#elif defined(__aarch64__)
    #define ARCH "ARM64"
#elif defined(__i386__)
    #define ARCH "i386"
#elif defined(__ia64__)
    #define ARCH "IA-64"
#elif defined(__m68k__)
    #define ARCH "m68k"
#elif defined(__mips__)
    #define ARCH "MIPS"
#elif defined(__powerpc64__) || defined(__ppc_64)
    #define ARCH "PPC64"
#elif defined(__powerpc__)
    #define ARCH "PowerPC"
#elif defined(__riscv)
    #define ARCH "RISC-V"
#elif defined(__sparc__)
    #define ARCH "SPARC"
#else
    #define ARCH ""
#endif

#define BETTERSPADES_VERSION "v" BETTERSPADES_MAJOR "." BETTERSPADES_MINOR "." BETTERSPADES_PATCH
#define BETTERSPADES_VERSION_SUMMARY BETTERSPADES_VERSION " " ARCH " " GIT_COMMIT_HASH

#ifndef OPENGL_ES
    #define GLEW_STATIC
    #include <GL/glew.h>
#else
    #ifdef USE_SDL
        #include <SDL2/SDL_opengles.h>
    #endif

    #define glColor3f(r, g, b) glColor4f(r, g, b, 1.0F)
    #define glColor3ub(r, g, b) glColor4ub(r, g, b, 255)
    #define glDepthRange(a, b) glDepthRangef(a, b)
    #define glClearDepth(a) glClearDepthf(a)
#endif

#ifdef USE_RPC
    #include <discord_rpc.h>
#endif

#ifdef _WIN32
    #define OS_WINDOWS
#endif

#ifdef __linux__
    #define OS_LINUX
    #include <sys/sysinfo.h>
#endif

#ifdef __APPLE__
    #define OS_APPLE
#endif

#ifdef __HAIKU__
    #define OS_HAIKU
#endif

#ifndef min
    #define min(a, b) ((a) < (b) ? (a) : (b))
#endif

#ifndef max
    #define max(a, b) ((a) > (b) ? (a) : (b))
#endif

#define absf(a) (((a) > 0) ? (a) : -(a))

#define distance2D(x1, y1, x2, y2) (((x2) - (x1)) * ((x2) - (x1)) + ((y2) - (y1)) * ((y2) - (y1)))
#define distance3D(x1, y1, z1, x2, y2, z2) (((x2) - (x1)) * ((x2) - (x1)) + ((y2) - (y1)) * ((y2) - (y1)) + ((z2) - (z1)) * ((z2) - (z1)))
#define angle3D(x1, y1, z1, x2, y2, z2) acos((x1) * (x2) + (y1) * (y2) + (z1) * (z2)) // vectors should be normalized
#define len2D(x, y) sqrt(pow(x, 2) + pow(y, 2))
#define len3D(x, y, z) sqrt(pow(x, 2) + pow(y, 2) + pow(z, 2))

#define BYTE0(col) ((col) & 0xFF)
#define BYTE1(col) (((col) >> 8) & 0xFF)
#define BYTE2(col) (((col) >> 16) & 0xFF)
#define BYTE3(col) (((col) >> 24) & 0xFF)

#define PI 3.1415F
#define DOUBLEPI (PI * 2.0F)
#define HALFPI (PI * 0.5F)
#define EPSILON 0.005F

#define MOUSE_SENSITIVITY 0.002F

#define CHAT_NO_INPUT   0
#define CHAT_ALL_INPUT  1
#define CHAT_TEAM_INPUT 2

typedef struct {
    uint8_t r, g, b, a;
} TrueColor;

typedef enum {
    UTF8, CP437, CP1252
} Codepage;

extern const TrueColor White, Black, Red, Green, Blue, Yellow, Cyan, Magenta;

extern int chat_input_mode;
extern float last_cy;

extern int fps;

extern char chat[2][10][256];
extern TrueColor chat_color[2][10];
extern float chat_timer[2][10];
extern char chat_popup[256];
extern float chat_popup_timer;
extern float chat_popup_duration;
extern TrueColor chat_popup_color;
void chat_add(int channel, TrueColor, const uint8_t *, Codepage);
void chat_showpopup(const uint8_t *, float duration, TrueColor, Codepage);
const char * reason_disconnect(int code);

#define SCREEN_NONE        0
#define SCREEN_TEAM_SELECT 1
#define SCREEN_GUN_SELECT  2

extern int ms_seed;
int ms_rand(void);

#include <stdlib.h>
#include <log.h>

#define CHECK_ALLOCATION_ERROR(ret)                                                        \
    if (!ret) {                                                                            \
        log_fatal("Critical error: memory allocation failed (%s:%d)", __func__, __LINE__); \
        exit(1);                                                                           \
    }

#ifdef __BIG_ENDIAN__
    uint16_t letohu16(uint16_t);
    int16_t  letohs16(int16_t);

    float    letohf(float);
    uint32_t letohu32(uint32_t);
    int      letohs32(int);

#else
    #define letohu16(x) (x)
    #define letohs16(x) (x)

    #define letohf(x)   (x)
    #define letohu32(x) (x)
    #define letohs32(x) (x)
#endif

#define htoleu16(x) letohu16(x)
#define htoles16(x) letohs16(x)

#define htolef(x)   letohf(x)
#define htoleu32(x) letohu32(x)
#define htoles32(x) letohs32(x)

void writeRGBA(uint32_t *, TrueColor);
void writeBGR(uint32_t *, TrueColor);

TrueColor readBGR(uint32_t *);
TrueColor readBGRA(uint32_t *);

#endif
