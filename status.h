//
// Created by Attila on 11/19/2025.
//

#ifndef DEFLATE_STATUS_H
#define DEFLATE_STATUS_H
#include <stdlib.h>
#include <string.h>

typedef enum {
    COMPRESSION_SUCCESS,
    COMPRESSION_FAILED,
    CANT_OPEN_FILE,
    CANT_ALLOCATE_MEMORY,
    DECOMPRESS_SUCCESS,
    DECOMPRESS_FAILED,
} STATUS_CODE;

typedef struct {
    STATUS_CODE code;
    char* message;
} STATUS;

extern void createSTATUSMessage(STATUS* STATUS, const char* message);

extern STATUS* initSTATUS(void);

#endif //DEFLATE_STATUS_H