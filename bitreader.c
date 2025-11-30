//
// Created by Attila on 11/24/2025.
//

#include "bitreader.h"
#define BUFFER_SIZE 4096

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "debugmalloc.h"

// --- GZIP Constants ---
#define GZIP_ID1 0x1f
#define GZIP_ID2 0x8b
#define GZIP_CM_DEFLATE 0x08

// --- GZIP Flags (FLG byte) ---
#define FTEXT    0x01 // Header contains text file indication
#define FHCRC    0x02 // Header CRC16 is present
#define FEXTRA   0x04 // Extra field is present
#define FNAME    0x08 // Original file name is present
#define FCOMMENT 0x10 // File comment is present


static int load_next_chunk(BIT_READER *reader) {
    size_t bytes_read = fread(reader->buffer, 1, BUFFER_SIZE, reader->file);

    reader->buffer_index = 0;
    reader->buffer_size = bytes_read;

    if (bytes_read == 0) {
        if (ferror(reader->file)) return -1; // Error
        return 0; // EOF
    }
    return 1;
}

// Loads the next byte from the memory buffer into the bit accumulator (reader->byte).
static int load_next_byte(BIT_READER *reader) {
    // Check if we exhausted the memory buffer
    if (reader->buffer_index >= reader->buffer_size) {
        int load_status = load_next_chunk(reader);
        if (load_status <= 0) return load_status; // EOF or Error
    }

    // Load byte from memory buffer
    reader->byte = reader->buffer[reader->buffer_index];
    reader->buffer_index++;
    reader->currentPosition = 0;
    return 1;
}

// --- Public Functions ---

BIT_READER* init_bit_reader(const char *filePath) {
    BIT_READER* reader = (BIT_READER*) malloc(sizeof(BIT_READER));
    reader->file = fopen(filePath, "rb");
    if (!reader->file) {
        perror("Failed to open file for reading");
        free(reader);
        return NULL;
    }

    // Allocate the read buffer
    reader->buffer = (uint8_t*) malloc(BUFFER_SIZE);

    // Initialize state
    reader->currentPosition = 8; // Force load_next_byte on first read
    reader->buffer_index = 0;
    reader->buffer_size = 0;
    reader->byte = 0;

    return reader;
}

int read_bit(BIT_READER *reader) {
    // 1. Check if the current byte accumulator is exhausted
    if (reader->currentPosition >= 8) {
        int load_status = load_next_byte(reader);
        if (load_status <= 0) return -1; // EOF or Error
    }

    // 2. Extract the bit: LSB-first order.
    uint8_t bit = (reader->byte >> reader->currentPosition) & 1;

    // 3. Advance the position
    reader->currentPosition++;

    return (int)bit;
}

// --- REWRITTEN: Function to read multiple bits ---
uint32_t read_bits(BIT_READER *reader, int numBits) {
    if (numBits < 1 || numBits > 32) {
        fprintf(stderr, "Error: Invalid number of bits requested (%d).\n", numBits);
        return 0xFFFFFFFF; // Error value
    }

    uint32_t result = 0;

    // Read bits one by one, constructing the value in LSB-first order
    for (int i = 0; i < numBits; i++) {
        int bit = read_bit(reader);

        if (bit == -1) {
            // Reached unexpected end of stream
            fprintf(stderr, "Error: Unexpected EOF while reading %d bits (got %d of %d).\n", numBits, i, numBits);
            return 0xFFFFFFFF; // Error value
        }

        // Place the read bit (0 or 1) into the correct position in the result.
        // Since DEFLATE header fields and extra bits are read LSB-first,
        // the first bit read goes to result[0], the second to result[1], and so on.
        result |= (uint32_t)bit << i;
    }

    return result;
}
// --------------------------------------------------

void freeBIT_READER(BIT_READER *reader) {
    if (reader->file) fclose(reader->file);
    if (reader->buffer) free(reader->buffer);
    free(reader);
}


// Function to peek 'n' bits into the stream without advancing the actual reader
extern uint16_t peek_bits(BIT_READER* reader, uint8_t n) {
    if (n < 1 || n > 16) return 0xFFFF; // Max uint16_t (16 bits)

    // --- 1. State Copy (Memory Only) ---

    // Create a local copy of the reader state variables for calculation
    uint16_t result = 0;

    // State of the current byte being consumed:
    uint8_t temp_byte = reader->byte;
    uint8_t temp_pos = reader->currentPosition;

    // State of the memory buffer:
    size_t temp_buf_idx = reader->buffer_index;
    size_t temp_buf_size = reader->buffer_size;

    // --- 2. Calculation Loop (Safe, no file I/O) ---

    for (int i = 0; i < n; i++) {
        // If current byte accumulator is exhausted, look in the buffer
        if (temp_pos >= 8) {
            // Check if buffer is exhausted
            if (temp_buf_idx >= temp_buf_size) {
                // If we reach here, we need to read from file, which is unsafe for peek.
                // We stop and return an error.
                return 0xFFFF;
            }
            // Load next byte from buffer (in memory)
            temp_byte = reader->buffer[temp_buf_idx];
            temp_buf_idx++;
            temp_pos = 0; // Reset bit position
        }

        // Extract the bit: LSB-first order.
        uint8_t bit = (temp_byte >> temp_pos) & 1;
        temp_pos++;

        // Add bit to result (LSB-first)
        result |= (uint16_t)bit << i;
    }

    return result;
}

bool process_gzip_header(BIT_READER *reader) {
    uint8_t id1, id2, cm, flg, xfl, os;

    // 1. Check Magic Bytes (ID1, ID2) - 2 bytes
    id1 = read_bits(reader, 8);
    id2 = read_bits(reader, 8);

    if (id1 != GZIP_ID1 || id2 != GZIP_ID2) {
        fprintf(stderr, "Error: Invalid magic bytes. Not a GZIP file (got 0x%02x 0x%02x, expected 0x%02x 0x%02x).\n", id1, id2, GZIP_ID1, GZIP_ID2);
        return false;
    }

    // 2. Compression Method (CM) - 1 byte
    cm = read_bits(reader, 8);
    if (cm != GZIP_CM_DEFLATE) {
        fprintf(stderr, "Error: Unsupported compression method (got 0x%02x, expected DEFLATE 0x%02x).\n", cm, GZIP_CM_DEFLATE);
        return false;
    }

    // 3. Flags (FLG) - 1 byte
    flg = read_bits(reader, 8);

    // --- BEGIN User's requested simple flag check ---
    if (flg != 0x00) {
        fprintf(stderr, "Warning: Only GZIP files with FLAG=0x00 are supported at this stage. Found flag 0x%02x.\n", flg);
        // For now, we will fail if it's not 0x00, as requested by the user's initial logic.
        return false;
    }
    // --- END User's requested simple flag check ---

    read_bits(reader,32);

    // 5. Extra Flags (XFL) - 1 byte
    xfl = read_bits(reader, 8);
    if (xfl == 0xFF) return false; // Check for EOF

    // 6. Operating System (OS) - 1 byte
    os = read_bits(reader, 8);
    if (os == 0xFF) return false; // Check for EOF


    // --- Handling Optional Fields (Disabled for FLG=0x00) ---
    /*
    if (flg & FEXTRA) {
        // Read 16-bit extra field length (XLEN)
        uint16_t xlen = read_le_u16(reader);
        // Skip XLEN bytes
        for (int i = 0; i < xlen; i++) {
            if (read_byte_aligned_u8(reader) == 0xFF) return false;
        }
    }

    // ... logic for FNAME, FCOMMENT, FHCRC would follow here ...
    */

    printf("GZIP Header successfully processed. Ready for DEFLATE stream.\n");
    return true;
}
