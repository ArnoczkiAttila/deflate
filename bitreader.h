//
// Created by Arnóczki Attila on 11/24/2025.
//

#ifndef DEFLATE_BITREADER_H
#define DEFLATE_BITREADER_H
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

typedef struct {
    FILE *file;
    uint8_t byte;
    uint8_t currentPosition;

    uint8_t *buffer;          // New: Memory buffer
    size_t buffer_index;      // New: Index in buffer
    size_t buffer_size;       // New: Size of valid data in buffer
} BIT_READER;
/**
 * Initializes the BIT_READER structure.
 * Opens the file and resets the bit-reading state.
 * @param filePath Path to the input file.
 * @return 0 on success, -1 on failure (e.g., file not found).
 */
BIT_READER* init_bit_reader(const char *filePath);

/**
 * Reads a single bit from the stream.
 * Handles reading new bytes from the file when the current byte is exhausted.
 * @param reader Pointer to the initialized BIT_READER.
 * @return The bit value (0 or 1), or -1 if the end of file is reached unexpectedly.
 */
int read_bit(BIT_READER *reader);

/**
 * Reads a specified number of bits from the stream.
 * @param reader Pointer to the initialized BIT_READER.
 * @param numBits The number of bits to read (must be <= 32).
 * @return The unsigned integer value represented by the bits, or 0xFFFFFFFF on error.
 */
uint32_t read_bits(BIT_READER *reader, int numBits);

/**
 * Closes the file handle and performs cleanup.
 * @param reader Pointer to the BIT_READER structure.
 */
void freeBIT_READER(BIT_READER *reader);

//void createFile(BIT_READER* bw, char* fileName, char* extension);

bool process_gzip_header(BIT_READER *reader);

extern uint16_t peek_bits(BIT_READER* reader, uint8_t n);



#endif //DEFLATE_BITREADER_H