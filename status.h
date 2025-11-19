//
// Created by Rendszergazda on 11/19/2025.
//

#ifndef DEFLATE_STATUS_H
#define DEFLATE_STATUS_H

typedef enum {
    COMPRESSION_SUCCESS,
    COMPRESSION_FAILED
} StatusCode;

typedef struct {
    char* message;
} StatusMessage;

typedef struct {
    StatusCode code;
    StatusMessage message;
} Status;

#endif //DEFLATE_STATUS_H