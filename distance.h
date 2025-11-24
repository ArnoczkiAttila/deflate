//
// Created by Rendszergazda on 11/19/2025.
//

#ifndef DEFLATE_DISTANCE_H
#define DEFLATE_DISTANCE_H

typedef struct {
    unsigned short usSymbolID;
    int iExtraBits;
    int iExtraValue;
} DISTANCE_CODE;

/**
 * @brief Maps a raw LZ77 distance to its Deflate Symbol ID and extra bit information.
 * * @param distance The raw look-back distance (1 to 32768).
 * @return DISTANCE_CODE The structure containing the Symbol ID, extra bits count, and value.
 */
extern DISTANCE_CODE getDistanceCode(int distance);

#endif //DEFLATE_DISTANCE_H