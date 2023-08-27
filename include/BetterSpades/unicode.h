#include <BetterSpades/common.h>

#ifndef UNICODE_H
#define UNICODE_H

uint8_t encode(uint8_t *, uint32_t, Codepage);
uint8_t decode(uint8_t *, uint32_t *, Codepage);

#endif