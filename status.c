//
// Created by Attila on 11/29/2025.
//

#include "status.h"

/**
 * @brief Create STATUS Message
 *
 * This function frees up the current message stored in the STATUS's message char* and then
 * allocates memory for the new message, and then copies it over, with of coures the '\0' symbol at the end.
 *
 * @param STATUS The STATUS object.
 * @param message Zero terminated char*
 *
 * @returns void
 *
 * Maximum memory required:
 *  - 32bit systems: 16 + messageSize +1 bytes
 *  - 64bit systems: 32 + messageSize + 1 bytes
 */
extern void createSTATUSMessage(STATUS* STATUS, const char* message) {
    if (STATUS->message != NULL) {
        free(STATUS->message);
    }
    const size_t messageSize = strlen(message);
    STATUS->message = (char*) malloc(messageSize+1);
    strcpy(STATUS->message,message);
    STATUS->message[messageSize] = '\0';
}

/**
 * @brief Initialize STATUS
 *
 * This function initializes the STATUS object and returns with its pointer.
 *
 * @returns STATUS* STATUS object OR NULL.
 *
 * Maximum memory required:
 *  - 32bit systems: 16 bytes
 *  - 64bit systems: 32 bytes
 */
extern STATUS* initSTATUS(void) {
    STATUS* status = (STATUS*) malloc(sizeof(STATUS));
    if (status == NULL) {
        return NULL;
    }
    status->code = 0;
    status->message = NULL;
    return status;
}

