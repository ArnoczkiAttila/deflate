//
// Created by Rendszergazda on 11/23/2025.
//

#include "HUFFMAN_TABLE.h"

#define INVALID_NODE_SYMBOL 286

#include "node.h"

extern void buildCodeLookupTable(Node* node, HUFFMAN_CODE* table, uint16_t current_code, int depth) {
    if (!node) return;

    if (node->usSymbol != INVALID_NODE_SYMBOL) {
        if (depth > 0) {
            table[node->usSymbol].code = current_code;
            table[node->usSymbol].length = (uint8_t)depth;
        } else {

            table[node->usSymbol].code = 0;
            table[node->usSymbol].length = 1;
        }
    } else {

        uint16_t left_code = current_code << 1;
        buildCodeLookupTable(node->pnLeft, table, left_code, depth + 1);

        uint16_t right_code = (current_code << 1) | 1;
        buildCodeLookupTable(node->pnRight, table, right_code, depth + 1);
    }
}