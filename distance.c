//
// Created by Rendszergazda on 11/19/2025.
//

#include "distance.h"

#include <stdio.h>

/**
 *                  Extra           Extra               Extra
 *            Code Bits Dist  Code Bits   Dist     Code Bits Distance
 *            ---- ---- ----  ---- ----  ------    ---- ---- --------
 *              0   0    1     10   4     33-48    20    9   1025-1536
 *              1   0    2     11   4     49-64    21    9   1537-2048
 *              2   0    3     12   5     65-96    22   10   2049-3072
 *              3   0    4     13   5     97-128   23   10   3073-4096
 *              4   1   5,6    14   6    129-192   24   11   4097-6144
 *              5   1   7,8    15   6    193-256   25   11   6145-8192
 *              6   2   9-12   16   7    257-384   26   12  8193-12288
 *              7   2  13-16   17   7    385-512   27   12 12289-16384
 *              8   3  17-24   18   8    513-768   28   13 16385-24576
 *              9   3  25-32   19   8   769-1024   29   13 24577-32768
 *
 * For more information check out (https://datatracker.ietf.org/doc/html/rfc1951#page-11) the rfc1951 standard.ű
 *
 * The table above represents the code for each distance. Some distance (almost all of them after 4) require
 * extra bits to be represented. Since its more likely to have smaller match distances in huge files, it is better to
 * have less corresponding bits for smaller distances and thus make the file even smaller.
 *
 */
static const int DISTANCE_BASE[] = {
    1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025,
    1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577
};
static const int DISTANCE_EXTRA_BITS[] = {
    0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13
};

#define NUM_DIST_CODES 30
#define MAX_ALLOWED_DISTANCE 32768

extern DistanceCode getDistanceCode(int distance) {
    DistanceCode result = {0, 0, 0};

    if (distance < 1) {
        fprintf(stderr, "Error: Distance must be at least 1.\n");
        return result;
    }
    if (distance > MAX_ALLOWED_DISTANCE) {
        fprintf(stderr, "Warning: Distance %d truncated to max allowed (%d).\n", distance, MAX_ALLOWED_DISTANCE);
        distance = MAX_ALLOWED_DISTANCE;
    }

    // Distances 1-4 are special (Symbol IDs 0-3 with 0 extra bits)
    if (distance >= 1 && distance <= 4) {
        result.usSymbolID = distance - 1; // 1 -> ID 0, 4 -> ID 3
        return result;
    }

    // For distances 5 and up (Symbol IDs 4-29), search the table
    // The loop starts at symbol ID 4 (index 4)
    for (int i = 4; i < NUM_DIST_CODES; i++) {
        int base = DISTANCE_BASE[i];
        int extra_bits = DISTANCE_EXTRA_BITS[i];

        // Calculate the maximum distance covered by this code
        // (1 << extra_bits) is equivalent to 2^extra_bits
        int max_dist = base + ( (1 << extra_bits) - 1 );

        if (distance >= base && distance <= max_dist) {
            result.usSymbolID = i;
            result.iExtraBits = extra_bits;

            // The extra value is the offset from the base distance
            result.iExtraValue = distance - base;
            return result;
        }
    }

    // This should only be reached if distance is 32769 and was truncated to 32768,
    // which corresponds to the max range of Symbol ID 29 (24577 to 32768).
    // The maximum possible distance for ID 29 is 24577 + (2^13 - 1) = 24577 + 8191 = 32768.
    if (distance == MAX_ALLOWED_DISTANCE) {
        result.usSymbolID = 29;
        result.iExtraBits = 13;
        result.iExtraValue = distance - DISTANCE_BASE[29]; // 32768 - 24577 = 8191
        return result;
    }

    fprintf(stderr, "Error: Unexpected distance %d failed mapping.\n", distance);
    return result;
}
