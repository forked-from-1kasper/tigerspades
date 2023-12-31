#include <BetterSpades/common.h>

const TrueColor White   = {0xFF, 0xFF, 0xFF, 0xFF};
const TrueColor Black   = {0x00, 0x00, 0x00, 0xFF};
const TrueColor Red     = {0xFF, 0x00, 0x00, 0xFF};
const TrueColor Green   = {0x00, 0xFF, 0x00, 0xFF};
const TrueColor Blue    = {0x00, 0x00, 0xFF, 0xFF};
const TrueColor Yellow  = {0xFF, 0xFF, 0x00, 0xFF};
const TrueColor Cyan    = {0x00, 0xFF, 0xFF, 0xFF};
const TrueColor Magenta = {0xFF, 0x00, 0xFF, 0xFF};

#define BSWAP16(T, ident)                                         \
    T ident (T inval) {                                           \
        T outval;                                                 \
        char * inptr = (char*) &inval, *outptr = (char*) &outval; \
        outptr[0] = inptr[1];                                     \
        outptr[1] = inptr[0];                                     \
        return outval;                                            \
    }

#define BSWAP32(T, ident)                                         \
    T ident (T inval) {                                           \
        T outval;                                                 \
        char * inptr = (char*) &inval, *outptr = (char*) &outval; \
        outptr[0] = inptr[3];                                     \
        outptr[1] = inptr[2];                                     \
        outptr[2] = inptr[1];                                     \
        outptr[3] = inptr[0];                                     \
        return outval;                                            \
    }

#ifdef __BIG_ENDIAN__
    BSWAP16(uint16_t, letohu16)
    BSWAP16(int16_t,  letohs16)

    BSWAP32(uint32_t, letohu32)
    BSWAP32(int,      letohs32)
    BSWAP32(float,    letohf)
#endif

void writeRGBA(uint32_t * dest, TrueColor color) {
    *((uint8_t*) dest + 0) = color.r;
    *((uint8_t*) dest + 1) = color.g;
    *((uint8_t*) dest + 2) = color.b;
    *((uint8_t*) dest + 3) = color.a;
}

void writeBGR(uint32_t * dest, TrueColor color) {
    *((uint8_t*) dest + 0) = color.b;
    *((uint8_t*) dest + 1) = color.g;
    *((uint8_t*) dest + 2) = color.r;
    *((uint8_t*) dest + 3) = 255;
}

TrueColor readBGR(uint32_t * src) {
    TrueColor retval;

    retval.b = *((uint8_t*) src + 0);
    retval.g = *((uint8_t*) src + 1);
    retval.r = *((uint8_t*) src + 2);
    retval.a = 255;

    return retval;
}

TrueColor readBGRA(uint32_t * src) {
    TrueColor retval;

    retval.b = *((uint8_t*) src + 0);
    retval.g = *((uint8_t*) src + 1);
    retval.r = *((uint8_t*) src + 2);
    retval.a = *((uint8_t*) src + 3);

    return retval;
}

void strnzcpy(char * dest, const char * src, size_t size) {
    strncpy(dest, src, size - 1); dest[size - 1] = 0;
}
