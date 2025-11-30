//
// Created by Rendszergazda on 11/19/2025.
//

#ifndef DEFLATE_LENGTH_H
#define DEFLATE_LENGTH_H

typedef struct {
    unsigned short usSymbolID;
    int iExtraBits;
    int iExtraValue;
} LENGTH_CODE;

extern LENGTH_CODE getLengthCode(int length);

#endif //DEFLATE_LENGTH_H