//
// Created by Rendszergazda on 11/24/2025.
//

#ifndef DEFLATE_CRC_CHECKSUM_H
#define DEFLATE_CRC_CHECKSUM_H

// The standard polynomial used for Gzip/Zlib (IEEE 802.3)
#define CRC32_POLYNOMIAL 0xEDB88320UL

// The initial value for the CRC32 calculation in Gzip/Zlib is 0xFFFFFFFF
#define CRC32_INITIAL_VALUE 0xFFFFFFFFUL
#include <stdint.h>

/**
 * @brief Updates a running CRC32 checksum based on a block of data.
 * The CRC32 algorithm used is the standard IEEE 802.3 (used in Gzip and Zlib).
 *
 * @param current_crc The current running CRC value (should be 0xFFFFFFFF for the start).
 * @param data Pointer to the buffer containing the data chunk.
 * @param length The number of bytes in the data chunk.
 * @return uint32_t The updated CRC value.
 */
extern uint32_t calculate_crc32(uint32_t current_crc, const uint8_t* data, size_t length);

#endif //DEFLATE_CRC_CHECKSUM_H