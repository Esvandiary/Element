#include "util/crc32.h"

// Implementation from https://github.com/stbrumme/crc32

// zlib License

// Copyright (c) 2011-2016 Stephan Brumme

// This software is provided 'as-is', without any express or implied warranty. In no event will the authors be held liable for any damages arising from the use of this software.
// Permission is granted to anyone to use this software for any purpose, including commercial applications, and to alter it and redistribute it freely, subject to the following restrictions:
// 1. The origin of this software must not be misrepresented; you must not claim that you wrote the original software.
//    If you use this software in a product, an acknowledgment in the product documentation would be appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.

/// look-up table for half-byte, same as crc32Lookup[0][16*i]
static const uint32_t Crc32Lookup16[16] =
{
    0x00000000,0x1DB71064,0x3B6E20C8,0x26D930AC,0x76DC4190,0x6B6B51F4,0x4DB26158,0x5005713C,
    0xEDB88320,0xF00F9344,0xD6D6A3E8,0xCB61B38C,0x9B64C2B0,0x86D3D2D4,0xA00AE278,0xBDBDF21C
};

/// compute CRC32 (half-byte algoritm)
static uint32_t crc32_halfbyte(const void* data, size_t length, uint32_t previousCrc32)
{
    uint32_t crc = ~previousCrc32; // same as previousCrc32 ^ 0xFFFFFFFF
    const uint8_t* current = (const uint8_t*) data;

    while (length-- != 0)
    {
        crc = Crc32Lookup16[(crc ^  *current      ) & 0x0F] ^ (crc >> 4);
        crc = Crc32Lookup16[(crc ^ (*current >> 4)) & 0x0F] ^ (crc >> 4);
        current++;
    }

    return ~crc; // same as crc ^ 0xFFFFFFFF
}

uint32_t crc32(const void* data, size_t length)
{
    return crc32_halfbyte(data, length, 0);
}