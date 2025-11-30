//
// Created by Attila on 11/19/2025.
//

#ifndef DEFLATE_BIT_WRITER_H
#define DEFLATE_BIT_WRITER_H

#include <stdint.h>
#include <stdio.h>

typedef struct {
    FILE *file;
    uint8_t* buffer;
    uint8_t byte;
    uint8_t currentPosition;
    size_t bufferSize;
    size_t index;
    char* fileName;
} BIT_WRITER;

extern void copyFromBufferHistory(BIT_WRITER* bw, uint16_t distance, uint16_t length);

extern BIT_WRITER* initBIT_WRITER(size_t bufferSize);

extern void addFastByte(BIT_WRITER* bw, uint8_t byte);

extern void addBits(BIT_WRITER* bw, uint32_t value, uint8_t bitLength);

extern void createFile(BIT_WRITER* bw, const char* fileName, const char* extension);

extern void freeBIT_WRITER(BIT_WRITER* bw);

extern void addBytes(BIT_WRITER* bw, uint32_t value, uint8_t bytes);

extern void flushBitstreamWriter(BIT_WRITER* bw);

extern void writeHuffmanCode(BIT_WRITER* bw, uint16_t code, uint8_t length);

#endif //DEFLATE_BIT_WRITER_H