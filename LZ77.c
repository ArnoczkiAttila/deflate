#include <stdint.h>
#include "LZ77.h"

//
// Created by Attila on 11/16/2025.
//

/**
* @brief Create Literal Struct for LZ77 compression
*
* The createLiteralLZ77 function allocates memory for one single LZ77_compressed struct
* filled with enum LITERAL and the actual byte.
*
* @param byte The actual byte to be written into the file
*
* @returns LZ77_compressed
*
 * Maximum memory required:
 *  - 32bit systems: 5 bytes
 *  - 64bit systems: 5 bytes
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
 * The createMatchLZ77 function allocates memory for one single LZ77_compressed struct
 * filled with enum MATCH and the actual distance / length pair.
 *
 * @param distance The distance from the last occurrence
 * @param length The length which specifies the length from last occurrence
 *
 * @returns LZ77_compressed
 *
 * Maximum memory required:
 *  - 32bit systems: 8 bytes
 *  - 64bit systems: 8 bytes
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


/**
 * @brief Initializes the LZ77_buffer struct.
 *
 * Allocates initial memory for the token array.
 *
 * @returns LZ77_buffer* The location in memory or NULL. MUST BE FREED afterward!
 *
 * Maximum memory required:
 *  - 32bit systems: 24 bytes
 *  - 64bit systems: 44 bytes
 */
extern LZ77_buffer* initLZ77Buffer(void) {
    LZ77_buffer* buffer = (LZ77_buffer*) malloc(sizeof(LZ77_buffer));
    if (buffer == NULL) {
        return NULL;
    }

    buffer->size = 0;
    buffer->capacity = EXPAND_BY;

    buffer->tokens = (LZ77_compressed*) malloc(sizeof(LZ77_compressed) * buffer->capacity);
    if (buffer->tokens == NULL) {
        free(buffer);
        return NULL;
    }

    return buffer;
}

/**
 * @brief Expands the buffer capacity by EXPAND_BY tokens.
 *
 * @param buffer The buffer to expand.
 *
 * @returns void
 *
 * Maximum memory required:
 *  - 32bit systems: 16 bytes
 *  - 64bit systems: 32 bytes
 */
extern void expandBuffer(LZ77_buffer* buffer) {
    const size_t new_capacity = buffer->capacity + EXPAND_BY;

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
 *
 * @returns void
 *
 * Maximum memory required:
 *  - 32bit systems: 8 bytes
 *  - 64bit systems: 12 bytes
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
 *
 * @returns void
 *
 * Maximum memory required:
 *  - 32bit systems: 4 bytes
 *  - 64bit systems: 8 bytes
 */
extern void freeLZ77Buffer(LZ77_buffer* buffer) {
    if (buffer == NULL) return;

    if (buffer->tokens != NULL) {
        free(buffer->tokens);
        buffer->tokens = NULL;
    }

    free(buffer);
}