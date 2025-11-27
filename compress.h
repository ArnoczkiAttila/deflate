//
// Created by Arnóczki Attila on 11/19/2025.
//

#ifndef DEFLATE_COMPRESS_H
#define DEFLATE_COMPRESS_H

#include <stddef.h>

#include "bitwriter.h"
#include "status.h"

extern Status compress(char* fileName);
extern FILE* ffOpenFile(const char* filename);
extern size_t flushBitWriterBuffer(BitWriter* bw);

#endif //DEFLATE_COMPRESS_H