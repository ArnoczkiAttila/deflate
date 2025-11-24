//
// Created by Rendszergazda on 11/24/2025.
//

#include "CRC_CHECKSUM.h"
#include <stdio.h>

// Static array to store the 256 pre-calculated CRC values
static uint32_t crc_table[256];

/**
 * @brief Generates the 256-entry lookup table for fast CRC32 calculation.
 * This should only be called once at program start or upon first use.
 */
static void build_crc_table() {
    uint32_t c;
    int n, k;

    for (n = 0; n < 256; n++) {
        c = (uint32_t)n;
        for (k = 0; k < 8; k++) {
            // Check the LSB of the current value 'c'
            if (c & 1) {
                // If LSB is 1, XOR with the polynomial
                c = CRC32_POLYNOMIAL ^ (c >> 1);
            } else {
                // If LSB is 0, simply shift
                c = c >> 1;
            }
        }
        crc_table[n] = c;
    }
}

// Flag to ensure the table is only built once
static int table_is_initialized = 0;

/**
 * @brief Updates a running CRC32 checksum based on a block of data.
 * * @param current_crc The current running CRC value (initial value 0xFFFFFFFF).
 * @param data Pointer to the buffer containing the data chunk.
 * @param length The number of bytes in the data chunk.
 * @return uint32_t The updated CRC value.
 */
extern uint32_t calculate_crc32(uint32_t current_crc, const uint8_t* data, size_t length) {
    if (!table_is_initialized) {
        build_crc_table();
        table_is_initialized = 1;
    }

    uint32_t c = current_crc;
    const uint8_t *buf = data;
    size_t i;

    // Core lookup table logic (optimized for speed)
    for (i = 0; i < length; i++) {
        // 1. XOR the current byte of input data with the LSB byte of the CRC accumulator (c)
        // 2. Look up the result in the table
        // 3. XOR the result with the MSB portion of the accumulator (c shifted right by 8 bits)
        c = crc_table[(c ^ buf[i]) & 0xFF] ^ (c >> 8);
    }

    return c;
}
