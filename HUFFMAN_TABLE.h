//
// Created by Rendszergazda on 11/23/2025.
//

#ifndef DEFLATE_HUFFMAN_TABLE_H
#define DEFLATE_HUFFMAN_TABLE_H

#include <stdint.h>

#include "node.h"

typedef struct {
    uint16_t code; //for example 101 in binary which means right, left, right in the tree
    uint8_t length; //in this example 3 which says how many bits are there
} HUFFMAN_CODE;

extern void buildCodeLookupTable(Node* node, HUFFMAN_CODE* table, uint16_t current_code, int depth);

#endif //DEFLATE_HUFFMAN_TABLE_H