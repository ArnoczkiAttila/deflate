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


/**
 * @brief Processes the 10-byte GZIP header and any optional fields.
 * * @param reader Pointer to the initialized BIT_READER.
 * @return true if the header is valid and optional fields were processed (or skipped).
 * @return false if the file is not a valid GZIP stream or required features are unsupported.
 */
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
            fprintf(stderr, "Error: Unexpected EOF while reading %d bits.\n", numBits);
            return 0xFFFFFFFF; // Error value
        }

        // Place the read bit (0 or 1) into the correct position in the result.
        // Since we read in LSB-first order, the first bit read goes to result[0],
        // the second to result[1], and so on.
        result |= (uint32_t)bit << i;
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


/**
 * Loads the next byte from the file into the reader's 'byte' field.
 * Resets 'currentPosition' to 0.
 * @param reader Pointer to the initialized BIT_READER.
 * @return 1 on successful load, 0 on EOF, -1 on read error.
 */
static int load_next_byte(BIT_READER *reader) {
    int result = fgetc(reader->file);
    if (result == EOF) {
        if (ferror(reader->file)) {
            // Handle read error
            return -1;
        }
        // Handle EOF
        return 0;
    }

    // Successfully read a byte
    reader->byte = (uint8_t)result;
    reader->currentPosition = 0; // Reset bit position (0 means we start reading from bit 0)
    return 1;
}

// --- Public Functions ---

BIT_READER* init_bit_reader(const char *filePath) {
    BIT_READER* reader = (BIT_READER*) malloc(sizeof(BIT_READER));

    reader->file = fopen(filePath, "rb");
    if (!reader->file) {
        perror("Failed to open file for reading");
        return NULL;
    }

    // Initialize the bit-level state
    // We don't load the first byte immediately; it will be loaded on the first call to read_bit.
    reader->currentPosition = 8; // Set to 8 to force load_next_byte on first read

    // Note: The 'buffer', 'bufferSize', 'index', and 'fileName' fields are unused
    // in this simplified, byte-by-byte implementation for clarity.

    return reader;
}

int read_bit(BIT_READER *reader) {
    // 1. Check if the current byte is exhausted (8 bits read)
    if (reader->currentPosition >= 8) {
        int load_status = load_next_byte(reader);
        if (load_status <= 0) {
            // 0: EOF, -1: Error
            return -1;
        }
    }

    // 2. Extract the bit: DEFLATE reads bits in LSB-first order.
    // The bit at position 0 is the least significant bit (LSB).
    uint8_t bit = (reader->byte >> reader->currentPosition) & 1;

    // 3. Advance the position
    reader->currentPosition++;

    return (int)bit;
}

void close_bit_reader(BIT_READER *reader) {
    if (reader->file) {
        fclose(reader->file);
        reader->file = NULL;
    }
    // Free other allocated resources if they were used (e.g., reader->buffer, reader->fileName)
}

static int peek_bit(BIT_READER* temp_reader) {
    // If we've read all bits in the current byte, move to the next byte in the buffer
    if (temp_reader->currentPosition == 8) {
        if (temp_reader->index >= temp_reader->bufferSize) {
            // Cannot fetch more bits from the buffer/file without reading
            return -1; // Indicate end of available data for peeking
        }
        temp_reader->byte = temp_reader->buffer[temp_reader->index];
        temp_reader->index++;
        temp_reader->currentPosition = 0;
    }

    // Extract the bit at the current position (LSB-first assuming your read_bits logic)
    int bit = (temp_reader->byte >> temp_reader->currentPosition) & 1;
    temp_reader->currentPosition++;

    return bit;
}

// Function to peek 'n' bits into the stream without advancing the actual reader
extern uint16_t peek_bits(BIT_READER* reader, uint8_t n) {
    if (n < 1 || n > 16) {
        // We return uint16_t, so limit n to 16 bits.
        fprintf(stderr, "Error: Invalid number of bits requested for peek (%d).\n", n);
        return 0xFFFF; // Error value
    }

    // Create a temporary, local copy of the reader state.
    // We only need to copy the *state variables*, not the pointers to the file or buffer themselves.
    // We *must not* call functions that perform I/O operations (like fread) inside peek.
    BIT_READER temp_reader;
    memcpy(&temp_reader, reader, sizeof(BIT_READER));
    // The buffer pointer in temp_reader now points to the *same* buffer memory as the original reader. This is fine because we're only reading from the buffer, not modifying it or loading new data via file I/O.

    uint16_t result = 0;

    // Read bits using the temporary reader state
    for (int i = 0; i < n; i++) {
        int bit = peek_bit(&temp_reader);

        if (bit == -1) {
            // Reached end of currently buffered stream
            fprintf(stderr, "Error: Not enough buffered data to peek %d bits.\n", n);
            return 0xFFFF; // Error value
        }

        // Place the read bit into the correct position in the result (LSB-first)
        result |= (uint16_t)bit << i;
    }

    // The 'reader' pointer's state remains untouched.
    return result;
}