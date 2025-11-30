//
// Created by Attila on 11/19/2025.
//

#include "bitwriter.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAGIC_NUMER 0x8B1F
#define COMPRESSION_METHOD 0x08 //deflate
#define FLAG 0b00000000 //RESERVED,RESERVED,RESERVED, FCOMMENT, FNAME, FEXTRA, FHCRC FTEXT
#define XFL 0x00
#define OS 0x03 //FAT filesystem

/**
 * @brief Reverses the bits of a 16-bit integer.
 * Helper function for writing Huffman codes.
 */
static uint16_t reverse_bits_write(uint16_t val, int bits) {
    uint16_t res = 0;
    for (int i = 0; i < bits; i++) {
        if (val & (1 << i)) {
            res |= (1 << (bits - 1 - i));
        }
    }
    return res;
}

/**
 * @brief Writes a Huffman Code to the stream.
 *
 * CRITICAL: Huffman codes in DEFLATE are defined MSB-first (e.g., 110),
 * but the bit stream is filled LSB-first. This function reverses the code
 * so that the MSB of the code becomes the first bit written to the file.
 *
 * @param bw BIT_WRITER* object.
 * @param code The canonical Huffman code.
 * @param length The bit length of the code.
 */
extern void writeHuffmanCode(BIT_WRITER* bw, uint16_t code, uint8_t length) {
    if (length == 0) return;

    // Reverse the code so the MSB is written first
    uint16_t reversed = reverse_bits_write(code, length);

    // Write the reversed value using the standard LSB-first writer
    addBits(bw, reversed, length);
}

/**
 * @brief Flush BIT_WRITER Buffer
 *
 * This function dumps all the buffer data into the file, and then sets the BIT_WRITER's current index pointer back to 0.
 *
 * @param bw BIT_WRITER* object.
 *
 * @return size_t Elements written to a file. If none then 0.
 *
 * Maximum memory required:
 *  - 32bit systems: 12 bytes
 *  - 64bit systems: 24 bytes
 */
extern size_t flushBIT_WRITERBuffer(BIT_WRITER* bw) {
    //printf("index = %d\n",bw->index);
    size_t elementsWritten = fwrite(bw->buffer,1,bw->index,bw->file);
    //printf("Elements written to file: %llu \n", elementsWritten);
    bw->index = 0;
    return elementsWritten;
}

/**
 * @brief Initialize BIT_WRITER
 *
 * This function creates a BIT_WRITER object. @note This does not open a file in the first place, just initializes
 * the object!
 *
 * @param bufferSize The actual buffer size used for writing files.
 *
 * @return BIT_WRITER* The pointer to a BIT_WRITER object OR NULL.
 *
 * Maximum memory required:
 *  - 32bit systems: 24 bytes
 *  - 64bit systems: 48 bytes
 */
extern BIT_WRITER* initBIT_WRITER(const size_t bufferSize) {
    BIT_WRITER* bw = (BIT_WRITER*) malloc(sizeof(BIT_WRITER));
    bw->file = NULL; //temp
    bw->byte = 0;
    bw->buffer = (uint8_t*) malloc(bufferSize);
    bw->currentPosition = 0;
    bw->bufferSize = bufferSize;
    bw->index = 0;
    return bw;
}

/**
 * @brief Flush Byte
 *
 * This function flushes a byte into the BIT_WRITER's buffer, and sets the currentPosition and the byte to 0.
 *
 * @param bw BIT_WRITER* object.
 *
 * @returns void
 * Maximum memory required:
 *  - 32bit systems: 4 bytes
 *  - 64bit systems: 8 bytes
 */
static void flushByte(BIT_WRITER* bw) {
    bw->buffer[bw->index] = bw->byte;
    bw->index++;
    bw->byte = 0;
    bw->currentPosition = 0;
    //printf("%d\t",bw->index);
    if (bw->index == bw->bufferSize) flushBIT_WRITERBuffer(bw);
}

/**
 * @brief Add Bits
 *
 * This handles the file writing phase, since deflate uses bit by bit file writing from the LSB.
 *
 * @param bw BIT_WRITER* object.
 * @param value The value to be written. Maximum 4 bytes in length.
 * @param bitLength The desired bitLength which is to be written into the buffer or the byte by from value.
 *
 * @returns void
 *
 * Maximum memory required:
 *  - 32bit systems: 14 bytes
 *  - 64bit systems: 22 bytes
 */
extern void addBits(BIT_WRITER* bw, uint32_t value, uint8_t bitLength) {
    if (bitLength == 0 || bitLength > 32) {
        return;
    }

    for (int i = 0; i < bitLength; i++) {

        uint8_t bit = (value >> i) & 1;

        bw->byte |= (bit << bw->currentPosition);

        bw->currentPosition++;

        if (bw->currentPosition == 8) {
            flushByte(bw);
        }
    }
}

/**
 * @brief Flush Byte
 *
 * All this function does is it pads the BIT_WRITER's current byte with 0-s until the BIT_WRITER's currentPosition reaches 8.
 *
 * @param bw BIT_WRITER* object.
 *
 * @returns void
 *
 * Maximum memory required:
 *  - 32bit systems: 5 bytes
 *  - 64bit systems: 9 bytes
 */
extern void flushBitstreamWriter(BIT_WRITER* bw) {
    if (bw->currentPosition > 0) {
        uint8_t bits_to_pad = 8 - bw->currentPosition;

        addBits(bw, 0, bits_to_pad);
    }
}

/**
 * @brief Add Bytes
 *
 * This is basically the same as addBits, but it first flushes the current byte and then write the whole bytes in LSB order.
 * The maximum size of the value you can call the function with is 4 bytes and the bytes count can't be bigger than 4.
 *
 * @param bw BIT_WRITER* object.
 * @param value The maximum 4 byte long unsigned integer.
 * @param bytes The number of bytes in the value.
 *
 * @returns void
 *
 * Maximum memory required:
 *  - 32bit systems: 13 bytes
 *  - 64bit systems: 21 bytes
 */
extern void addBytes(BIT_WRITER* bw, const uint32_t value, const uint8_t bytes) {
    if (bytes > 4) {
        return;
    }
    flushBitstreamWriter(bw);
    for (int i = 0; i < bytes; i++) {
        bw->byte = (uint8_t)((value >> (i * 8)) & 0xFF);
        flushByte(bw);
    }
}

/**
 * @brief Helper to handle sliding window when the buffer is full.
 * Instead of flushing everything to 0, it keeps the second half of the buffer
 * as history at the beginning of the buffer.
 */
static void handleBufferSlide(BIT_WRITER* bw) {
    // We assume the buffer is allocated as 2 * WINDOW_SIZE (e.g. 64KB).
    // We keep the last half as history (32KB).
    size_t keepSize = bw->bufferSize / 2;
    size_t writeSize = bw->index - keepSize;
    printf("keepSize: %llu, writeSize: %llu\n",keepSize,writeSize);
    // 1. Write the old data (the first half) to disk
    if (writeSize > 0) {
        size_t elementsWrittenToTheFile = fwrite(bw->buffer, 1, writeSize, bw->file);
        printf("Elements written to the file: %llu\n",elementsWrittenToTheFile);
    }

    // 2. Move the recent data (history) to the start of the buffer
    memmove(bw->buffer, bw->buffer + writeSize, keepSize);

    // 3. Reset index to point immediately after the kept history
    // This ensures subsequent writes happen after the history.
    bw->index = keepSize;
}

extern void addFastByte(BIT_WRITER* bw, const uint8_t byte) {
    bw->buffer[bw->index++] = byte;
    //printf("%llu\n",bw->index);
    if (bw->index == bw->bufferSize) {
        handleBufferSlide(bw);
    }
}

// 2. Copies 'length' bytes starting from 'distance' bytes back.
extern void copyFromBufferHistory(BIT_WRITER* bw, const uint16_t distance, const uint16_t length) {
    for (uint16_t i = 0; i < length; i++) {
        // We look back 'distance' bytes from the CURRENT write position.
        // Even if we just wrote a byte (incremented index), we look back from the new index.
        int source_index = bw->index - distance;

        // CRITICAL CHECK: Did we fall off the start of the buffer?
        // This happens if bufferSize is too small for the file being decompressed.
        if (source_index < 0) {
            fprintf(stderr, "Error: LZ77 Copy Underflow at index %d, dist %d. Buffer too small/Flushed!\n", bw->index, distance);
            // In a real app, you would handle this error (e.g. abort).
            // For now, writing 0 to avoid crash, but output is corrupt.
            addFastByte(bw, 0);
            continue;
        }

        // Read the byte from history
        uint8_t byte = bw->buffer[source_index];

        // Write it to the current position (this handles overlaps like dist=1 len=10)
        addFastByte(bw, byte);
    }
}

/**
 * @brief Create File
 *
 * This function opens/creates a file with the fileName and extension parameters, and then writes the .gz header information
 * into the opened file.
 *
 * @param bw The BIT_WRITER object.
 * @param fileName The file name to be created.
 * @param extension The extension of the file.
 *
 * Maximum memory required:
 *  - 32bit systems: 20 bytes
 *  - 64bit systems: 40 bytes
 */
extern void createFile(BIT_WRITER* bw, const char* fileName, const char* extension) {
    char* newFileName = (char*) malloc(strlen(fileName) + strlen(extension) + 2);
    strcpy(newFileName, fileName);
    strcat(newFileName, ".");
    strcat(newFileName, extension);

    FILE* file = fopen(newFileName, "wb");
    bw->fileName = newFileName;
    bw->file = file;

    addBytes(bw, MAGIC_NUMER, 2); //ID1 ID2
    addBytes(bw, COMPRESSION_METHOD, 1); //CM
    addBytes(bw, FLAG,1); //FLG
    addBytes(bw, (uint32_t) time(NULL), 4); //MTIME
    addBytes(bw, XFL,1); //XFL
    addBytes(bw, OS, 1);

    printf("CurrentPosition: %d\n",bw->currentPosition);

}


/**
 * @brief Free BIT_WRITER
 *
 * This is a must called function at the end of the program, since this is accountable for flushing te final bytes and
 * closing the file. And of course frees the allocated memory.
 *
 * @param bw The BIT_WRITER object.
 *
 * Maximum memory required:
 *  - 32bit systems: 4 bytes
 *  - 64bit systems: 8 bytes
 */
extern void freeBIT_WRITER(BIT_WRITER* bw) {
    printf("belepve\n");
    flushBitstreamWriter(bw);
    printf("belepve2\n");

    const size_t flushedBytes = flushBIT_WRITERBuffer(bw);
    printf("Last 8 byte: ");
    for (int i = 8; i > 0;i--) {
        printf("%d\t",bw->buffer[bw->index-i]);
    }
    printf("\nFlushed bytes %llu\n",flushedBytes);
    free(bw->fileName);
    fclose(bw->file);
    free(bw->buffer);
    free(bw);
}
