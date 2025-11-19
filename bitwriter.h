//
// Created by Attila on 11/19/2025.
//

#ifndef DEFLATE_BITWRITER_H
#define DEFLATE_BITWRITER_H

#include <stdint.h>
#include <stdio.h>

typedef struct {
    FILE *file;
    uint8_t* buffer;
    uint8_t byte;
    uint8_t currentPosition;
    size_t bufferSize;
    uint16_t index;
    char* fileName;
} BitWriter;

BitWriter* initBitWriter(void);

void addData(BitWriter* bw, uint32_t value, uint8_t bitLength);

void createFile(BitWriter* bw, char* fileName, char* extension);

void freeBitWriter(BitWriter* bw);

#endif //DEFLATE_BITWRITER_H