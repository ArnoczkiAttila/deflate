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


/**
 * @brief LengthCode
 *
 * The getLengthCode function calculates the Length Code for each match length, and returns a struct with the
 * necessary data in it. For more information on Length Code's check out the official rfc1951 standard documentation.
 * (https://datatracker.ietf.org/doc/html/rfc1951#page-11)
 *
 * @param length The actual length between (3 and 258)
 * @return LengthCode struct which stores the length code, the required extra bits, and then the extra value in those extra bits,
 */
extern LENGTH_CODE getLengthCode(int length);

#endif //DEFLATE_LENGTH_H