//
// Created by Attila on 11/16/2025.
//
#ifndef DEFLATE_LZ77_H
#define DEFLATE_LZ77_H

#include <stdint.h>
#include <stdlib.h>

/**
 * @brief The amount to expand the buffer's capacity when full.
 */
#define EXPAND_BY 50

/**
 * @brief Enumeration to distinguish between a literal byte and a match pair.
 */
typedef enum {
    LITERAL,
    MATCH
} LZ77_encoded_type;

/**
 * @brief The fundamental LZ77 token structure.
 * * Uses a struct for the match data to correctly store both distance and length.
 * Uses a union to ensure the token only takes the size of the largest data type (the match struct).
 */
typedef struct {
    LZ77_encoded_type type;

    union {
        uint8_t literal;
        struct {
            uint16_t distance;
            uint16_t length;
        } match;
    } data;
} LZ77_compressed;

/**
 * @brief Structure to manage a dynamic array (growing buffer) of LZ77 tokens.
 * * Stores the tokens directly in a contiguous array (LZ77_compressed*),
 * avoiding per-token allocations for better performance and cache utilization.
 */
typedef struct {
    LZ77_compressed* tokens; ///< Pointer to the start of the dynamic array of tokens.
    size_t size;             ///< The current number of tokens stored (using size_t for large files).
    size_t capacity;         ///< The total number of tokens the buffer can hold.
} LZ77_buffer;

extern LZ77_compressed createLiteralLZ77(uint8_t byte);

extern LZ77_compressed createMatchLZ77(uint16_t distance, uint16_t length);

extern LZ77_buffer* initLZ77Buffer(void);

extern void expandBuffer(LZ77_buffer* buffer);

extern void appendToken(LZ77_buffer* buffer, LZ77_compressed token);

extern void freeLZ77Buffer(LZ77_buffer* buffer);

#endif //DEFLATE_LZ77_H