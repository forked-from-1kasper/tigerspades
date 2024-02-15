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

#include <stdint.h>
#include <math.h>

#ifndef COMMON_H
#define COMMON_H

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
#elif defined(_ARCH_PPC64) || defined(__ppc64__) || defined(__PPC64__) || defined(__powerpc64__) || defined(__ppc_64)
    #define ARCH "PPC64"
#elif defined(_ARCH_PPC) || defined(__ppc) || defined(__ppc__) || defined(__PPC__) || defined(__powerpc) || defined(__powerpc__) || defined(__POWERPC__)
    #define ARCH "PowerPC"
#elif defined(__riscv)
    #define ARCH "RISC-V"
#elif defined(__sparc__)
    #define ARCH "SPARC"
#else
    #define ARCH ""
#endif

#define BETTERSPADES_VERSION_SUMMARY BETTERSPADES_VERSION " " ARCH " " GIT_COMMIT_HASH

#ifdef USE_RPC
    #include <discord_rpc.h>
#endif

#ifndef min
    #define min(a, b) ((a) < (b) ? (a) : (b))
#endif

#ifndef max
    #define max(a, b) ((a) > (b) ? (a) : (b))
#endif

#define clamp(m, M, x) (min(M, max(m, x)))

#define absf(a) (((a) > 0) ? (a) : -(a))

static inline float sqrf(float x)    { return x * x; }
static inline float cubef(float x)   { return x * x * x; }
static inline float fourthf(float x) { return x * x * x * x; }

static inline float norm2f(float x1, float y1, float x2, float y2)
{ return (x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1); }

static inline float norm3f(float x1, float y1, float z1, float x2, float y2, float z2)
{ return (x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1) + (z2 - z1) * (z2 - z1); }

static inline int norm3i(int x1, int y1, int z1, int x2, int y2, int z2)
{ return (x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1) + (z2 - z1) * (z2 - z1); }

// Vectors should be normalized.
static inline float angle3f(float x1, float y1, float z1, float x2, float y2, float z2)
{ return acosf(x1 * x2 + y1 * y2 + z1 * z2); }

static inline float hypot2f(float x, float y) { return sqrtf(x * x + y * y); }
static inline float hypot3f(float x, float y, float z) { return sqrtf(x * x + y * y + z * z); }

#define BYTE0(col) ((col) & 0xFF)
#define BYTE1(col) (((col) >> 8) & 0xFF)
#define BYTE2(col) (((col) >> 16) & 0xFF)
#define BYTE3(col) (((col) >> 24) & 0xFF)

#define PI       3.1415F
#define DOUBLEPI (PI * 2.0F)
#define HALFPI   (PI * 0.5F)
#define EPSILON  0.005F

#define MOUSE_SENSITIVITY 0.002F

#define CHAT_NO_INPUT   0
#define CHAT_ALL_INPUT  1
#define CHAT_TEAM_INPUT 2

typedef enum {
    VER075, VER076, UNKNOWN
} Version;

typedef struct {
    float x, y, z;
} Position;

typedef struct {
    float x, y, z;
} Orientation;

typedef struct {
    float x, y, z;
} Velocity;

typedef struct {
    uint8_t r, g, b, a;
} TrueColor;

typedef enum {
    UTF8, CP437, CP1252
} Codepage;

extern const TrueColor White, Black, Red, Green, Blue, Yellow, Cyan, Magenta, Sky;

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
void chat_add(int channel, TrueColor, const char *, Codepage);
void chat_showpopup(const char *, float duration, TrueColor, Codepage);
const char * reason_disconnect(int code);

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

static inline uint8_t decode8le(uint8_t * const buff)
{ return buff[0]; }

static inline uint16_t decode16le(uint8_t * const buff)
{ return buff[0] | (buff[1] << 8); }

static inline uint32_t decode32le(uint8_t * const buff)
{ return buff[0] | (buff[1] << 8) | (buff[2] << 16) | (buff[3] << 24); }

static inline void encode8le(uint8_t * const buff, uint8_t value)
{ buff[0] = value; }

static inline void encode16le(uint8_t * const buff, uint16_t value)
{ buff[0] = value & 0xFF; buff[1] = (value >> 8) & 0xFF; }

static inline void encode32le(uint8_t * const buff, uint32_t value)
{ buff[0] = (value >> 0)  & 0xFF; buff[1] = (value >> 8)  & 0xFF;
  buff[2] = (value >> 16) & 0xFF; buff[3] = (value >> 24) & 0xFF; }

#define DEFGETTER(T, U, ident, decoder) static inline T ident(uint8_t * const buff, size_t * index) \
                                        { union _Return { T val; U data; } ret; \
                                          ret.data = decoder(buff + *index); \
                                          *index += sizeof(U); return ret.val; }

DEFGETTER(uint8_t,  uint8_t,  getu8le,   decode8le)
DEFGETTER(uint16_t, uint16_t, getu16le,  decode16le)
DEFGETTER(uint32_t, uint32_t, getu32le,  decode32le)
DEFGETTER(int8_t,   uint8_t,  gets8le,   decode8le)
DEFGETTER(int16_t,  uint16_t, gets16le,  decode16le)
DEFGETTER(int32_t,  uint32_t, gets32le,  decode32le)
DEFGETTER(float,    uint32_t, getf32le,  decode32le)
DEFGETTER(char,     uint8_t,  getc8le,   decode8le)

static inline TrueColor getBGRA(uint8_t * const buff, size_t * index) {
    uint8_t b = getu8le(buff, index);
    uint8_t g = getu8le(buff, index);
    uint8_t r = getu8le(buff, index);
    uint8_t a = getu8le(buff, index);

    return (TrueColor) {r, g, b, a};
}

void writeRGBA(uint32_t *, TrueColor);
void writeBGR(uint32_t *, TrueColor);

TrueColor readBGR(uint32_t *);
TrueColor readBGRA(uint32_t *);

void strnzcpy(char * dest, const char * src, size_t);

// QUESTION: should we allow players to change this?
#define RENDER_DISTANCE 128.0F

                        // NOTE: These options are intended for testing purposes only.
                        // NOTE: Don’t cry if you got banned for using this on a public server.
                        // ┌───────────────┬──────────────┬─────────────────────────────────────────────────────────────────────┐
                        // │ Easy to spot? │ Easy to fix? │ Reason                                                              │
                        // ├───────────────┼──────────────┼─────────────────────────────────────────────────────────────────────┤
#define HACK_NORELOAD 0 // │ Yes           │ Kinda        │ Hit packets are not checked for shooting without PacketWeaponInput. │
#define HACK_NORECOIL 0 // │ For spectator │ No           │ Recoil is client-side.                                              │
#define HACK_NOSPREAD 0 // │ For spectator │ No           │ Spread is client-side.                                              │
#define HACK_WALLHACK 0 // │ Yes           │ Yes          │ Hit packets are not checked for shooting through walls.             │
#define HACK_NOFOG    0 // │ No            │ Impossible?  │ Fog is (totally) client-side.                                       │
#define HACK_MAPHACK  0 // │ No            │ Strongly no  │ Player’s position data is always sent to everyone in full.          │
#define HACK_ESP      0 // │ Kinda         │ Strongly no  │ Same as previous.                                                   │
                        // └───────────────┴──────────────┴─────────────────────────────────────────────────────────────────────┘
#define HACKS_ENABLED (HACK_NORELOAD || HACK_NORECOIL || HACK_NOSPREAD || HACK_WALLHACK || HACK_MAPHACK || HACK_NOFOG || HACK_ESP)

#endif
