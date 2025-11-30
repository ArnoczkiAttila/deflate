//
// Created by Rendszergazda on 11/19/2025.
//

#include "length.h"

#include <stdio.h>

#define MIN_MATCH_LENGTH 3
#define MAX_MATCH_LENGTH 258
#define LITERAL_LENGTH_CODE_START 257
#define NUM_LENGTH_CODES 29

/**
 *                  Extra               Extra               Extra
 *           Code Bits Length(s) Code Bits Lengths   Code Bits Length(s)
 *           ---- ---- ------     ---- ---- -------   ---- ---- -------
 *            257   0     3       267   1   15,16     277   4   67-82
 *            258   0     4       268   1   17,18     278   4   83-98
 *            259   0     5       269   2   19-22     279   4   99-114
 *            260   0     6       270   2   23-26     280   4  115-130
 *            261   0     7       271   2   27-30     281   5  131-162
 *            262   0     8       272   2   31-34     282   5  163-194
 *            263   0     9       273   3   35-42     283   5  195-226
 *            264   0    10       274   3   43-50     284   5  227-257
 *            265   1  11,12      275   3   51-58     285   0    258
 *            266   1  13,14      276   3   59-66
 *
 * For more information check out (https://datatracker.ietf.org/doc/html/rfc1951#page-11) the rfc1951 standard.
 *
 * The table above represents the code for each length. Some length (almost all of them after 10) require
 * extra bits to be represented. Since its more likely to have smaller match lengths in huge files, it is better to
 * have less corresponding bits for smaller lengths and thus make the file even smaller.
 */
static const int LENGTH_BASE[] = {
    3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59,
    67, 83, 99, 115, 131, 163, 195, 227, 258
};
static const int LENGTH_EXTRA_BITS[] = {
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0
};


/**
 * @brief LengthCode
 *
 * The getLengthCode function calculates the Length Code for each match length, and returns a struct with the
 * necessary data in it. For more information on Length Code's check out the official rfc1951 standard documentation.
 * (https://datatracker.ietf.org/doc/html/rfc1951#page-11)
 *
 * @param length The actual length between (3 and 258)
 *
 * @return LengthCode struct which stores the length code, the required extra bits, and then the extra value in those extra bits,
 *
 * Maximum memory required:
 *  - 32bit systems: 30 bytes
 *  - 64bit systems: 60 bytes
 */
extern LENGTH_CODE getLengthCode(int length) {
    LENGTH_CODE result = {0, 0, 0};

    if (length < MIN_MATCH_LENGTH) {
        fprintf(stderr, "Error: Length %d is too short (min is %d).\n", length, MIN_MATCH_LENGTH);
        return result;
    }
    if (length > MAX_MATCH_LENGTH) {
        fprintf(stderr, "Warning: Length %d truncated to max allowed (%d).\n", length, MAX_MATCH_LENGTH);
        length = MAX_MATCH_LENGTH;
    }

    if (length == MAX_MATCH_LENGTH) {
        result.usSymbolID = LITERAL_LENGTH_CODE_START + NUM_LENGTH_CODES - 1; // 257 + 29 - 1 = 285
        result.iExtraBits = 0;
        result.iExtraValue = 0;
        return result;
    }

    for (int i = 0; i < NUM_LENGTH_CODES - 1; i++) {
        int base = LENGTH_BASE[i];
        int extra_bits = LENGTH_EXTRA_BITS[i];

        int max_len = base + ( (1 << extra_bits) - 1 );

        if (length >= base && length <= max_len) {
            result.usSymbolID = LITERAL_LENGTH_CODE_START + i;
            result.iExtraBits = extra_bits;

            result.iExtraValue = length - base;
            return result;
        }
    }

    fprintf(stderr, "Error: Unexpected length %d failed mapping.\n", length);
    return result;
}