//
// Created by Rendszergazda on 11/19/2025.
//

#ifndef DEFLATE_STATUS_H
#define DEFLATE_STATUS_H

typedef enum {
    COMPRESSION_SUCCESS,
    COMPRESSION_FAILED,
    CANT_OPEN_FILE,
    CANT_ALLOCATE_MEMORY,
    DECOMPRESS_SUCCESS,
    DECOMPRESS_FAILED,
} StatusCode;

typedef struct {
    StatusCode code;
    char* message;
} Status;

#endif //DEFLATE_STATUS_H