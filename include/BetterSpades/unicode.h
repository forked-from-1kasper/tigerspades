#include <BetterSpades/common.h>

#ifndef UNICODE_H
#define UNICODE_H

#define OCT1(c) ((((uint8_t) c) & 0x80) == 0x00)
#define OCT2(c) ((((uint8_t) c) & 0xE0) == 0xC0)
#define OCT3(c) ((((uint8_t) c) & 0xF0) == 0xE0)
#define OCT4(c) ((((uint8_t) c) & 0xF8) == 0xF0)
#define CONT(c) ((((uint8_t) c) & 0xC0) == 0x80)

uint8_t encode(uint8_t *, uint32_t, Codepage);
uint8_t decode(uint8_t *, uint32_t *, Codepage);
void reencode(uint8_t *, const uint8_t *, Codepage inpage, Codepage outpage);

size_t encodeMagic(uint8_t * dest, const uint8_t * src, size_t insize, size_t outsize);
void decodeMagic(uint8_t * dest, const uint8_t * src, size_t size);

#endif