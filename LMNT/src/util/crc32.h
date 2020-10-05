#ifndef LMNT_UTIL_CRC32_H
#define LMNT_UTIL_CRC32_H

#include <stdint.h>
#include <stdlib.h>

uint32_t crc32(const void* data, size_t length);

#endif