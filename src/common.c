#include <BetterSpades/common.h>

#define BSWAP32(T, ident)                                   \
    T ident(T in) {                                         \
        T out;                                              \
        char * inptr = (char*) &in, *outptr = (char*) &out; \
        outptr[0] = inptr[3];                               \
        outptr[1] = inptr[2];                               \
        outptr[2] = inptr[1];                               \
        outptr[3] = inptr[0];                               \
        return out;                                         \
    }

BSWAP32(uint32_t, letohu32)
BSWAP32(int,      letohs32)
BSWAP32(float,    letohf)

void writeRGBA(uint32_t * dest, RGBA color) {
    *((uint8_t*) dest + 0) = color.r;
    *((uint8_t*) dest + 1) = color.g;
    *((uint8_t*) dest + 2) = color.b;
    *((uint8_t*) dest + 3) = color.a;
}

void writeBGR(uint32_t * dest, RGBA color) {
    *((uint8_t*) dest + 0) = color.b;
    *((uint8_t*) dest + 1) = color.g;
    *((uint8_t*) dest + 2) = color.r;
    *((uint8_t*) dest + 3) = 255;
}

RGBA readBGR(uint32_t * src) {
    RGBA retval;

    retval.b = *((uint8_t*) src + 0);
    retval.g = *((uint8_t*) src + 1);
    retval.r = *((uint8_t*) src + 2);
    retval.a = 255;

    return retval;
}
