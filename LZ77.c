#include <stdint.h>
#include "LZ77.h"

//
// Created by Rendszergazda on 11/16/2025.
//
/**
* @brief Create Literal Struct for LZ77 compression
*
* The createLiteralLZ77 function allocates memory for one single LZ77_compresed struct
* filled with enum LITERAL and the actual byte.
*
* @param byte The actual byte to be written into the file
* @return LZ77_compressed
*/
extern LZ77_compressed createLiteralLZ77(const uint8_t byte) {
    const LZ77_compressed LZ77_compressed = {
        .type = LITERAL,
        .data.literal = byte
    };

    return LZ77_compressed;
}

/**
* @brief Create MATCH Struct for LZ77 compression
*
* The createMatchLZ77 function allocates memory for one single LZ77_compresed struct
* filled with enum MATCH and the actual distance / length pair.
*
* @param distance The distance from the last occurrence
* @param length The length which specifies the length from last occurrence
* @return LZ77_compressed
*/
extern LZ77_compressed createMatchLZ77(const uint16_t distance, const uint16_t length) {
    const LZ77_compressed LZ77_compressed = {
        .type = MATCH,
        .data.match = {
            .distance = distance,
            .length = length
        }
    };

    return LZ77_compressed;
}


// --- Buffer Management Functions ---

/**
 * @brief Initalizes the LZ77_buffer struct.
 *
 * Allocates initial memory for the token array.
 *
 * @return LZ77_buffer* The location in memory. MUST BE FREED afterward!
 */
extern LZ77_buffer* initLZ77Buffer(void) {
    LZ77_buffer* buffer = (LZ77_buffer*) malloc(sizeof(LZ77_buffer));
    if (buffer == NULL) {
        perror("Error allocating LZ77_buffer structure");
        exit(EXIT_FAILURE);
    }

    buffer->size = 0;
    buffer->capacity = EXPAND_BY;

    buffer->tokens = (LZ77_compressed*) malloc(sizeof(LZ77_compressed) * buffer->capacity);
    if (buffer->tokens == NULL) {
        perror("Error allocating initial token array");
        free(buffer);
        exit(EXIT_FAILURE);
    }

    return buffer;
}

/**
 * @brief Expands the buffer capacity by EXPAND_BY tokens.
 *
 * @param buffer The buffer to expand.
 */
extern void expandBuffer(LZ77_buffer* buffer) {
    const size_t new_capacity = buffer->capacity + EXPAND_BY;

    // Use realloc to resize the contiguous array of structs
    LZ77_compressed* new_tokens = (LZ77_compressed*) realloc(buffer->tokens, new_capacity * sizeof(LZ77_compressed));

    if (new_tokens == NULL) {
        perror("Error reallocating token buffer");
        exit(EXIT_FAILURE);
    }

    buffer->tokens = new_tokens;
    buffer->capacity = new_capacity;
}

/**
 * @brief Appends a token to the buffer.
 *
 * @param buffer The buffer to append the token to.
 */
extern void appendToken(LZ77_buffer* buffer, const LZ77_compressed token) {
    if (buffer->size >= buffer->capacity) {
        expandBuffer(buffer);
    }

    buffer->tokens[buffer->size] = token;
    buffer->size++;
}

/**
 * @brief Frees all dynamically allocated memory associated with the buffer.
 *
 * @param buffer The buffer structure to be freed.
 */
extern void freeLZ77Buffer(LZ77_buffer* buffer) {
    if (buffer == NULL) return;

    // 1. Free the contiguous array of tokens
    if (buffer->tokens != NULL) {
        free(buffer->tokens);
        buffer->tokens = NULL;
    }

    // 2. Free the buffer structure itself
    free(buffer);
}