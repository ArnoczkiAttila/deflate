//
// Created by Rendszergazda on 11/16/2025.
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

/**
* @brief Create Literal Struct for LZ77 compression
*
* The createLiteralLZ77 function allocates memory for one single LZ77_compresed struct
* filled with enum LITERAL and the actual byte.
*
* @param byte The actual byte to be written into the file
* @return LZ77_compressed* The location in memory. MUST BE FREED afterward!
*/
LZ77_compressed createLiteralLZ77(const uint8_t byte);

/**
* @brief Create MATCH Struct for LZ77 compression
*
* The createMatchLZ77 function allocates memory for one single LZ77_compresed struct
* filled with enum MATCH and the actual distance / length pair.
*
* @param distance The distance from the last occurrence
* @param length The length which specifies the length from last occurrence
* @return LZ77_compressed* The location in memory. MUST BE FREED afterward!
*/
LZ77_compressed createMatchLZ77(const uint16_t distance, const uint16_t length);


// --- Buffer Management Functions ---

/**
 * @brief Initalizes the LZ77_buffer struct.
 *
 * Allocates initial memory for the token array.
 *
 * @return LZ77_buffer* The location in memory. MUST BE FREED afterward!
 */
LZ77_buffer* initLZ77Buffer(void);

/**
 * @brief Expands the buffer capacity by EXPAND_BY tokens.
 *
 * @param buffer The buffer to expand.
 */
void expandBuffer(LZ77_buffer* buffer);

/**
 * @brief Appends a token to the buffer.
 *
 * @param buffer The buffer to append the token to.
 * @param token The LZ77_compressed token.
 */
void appendToken(LZ77_buffer* buffer, LZ77_compressed token);

/**
 * @brief Frees all dynamically allocated memory associated with the buffer.
 *
 * @param buffer The buffer structure to be freed.
 */
void freeLZ77Buffer(LZ77_buffer* buffer);

#endif //DEFLATE_LZ77_H