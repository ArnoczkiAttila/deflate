//
// Created by Rendszergazda on 11/29/2025.
//

#ifndef DEFLATE_DEBUG_H
#define DEFLATE_DEBUG_H
#include <stdint.h>

#include "HUFFMAN_TABLE.h"

extern void print_binary(uint16_t num, int len);
extern void print_debug_tree(HuffmanTree* tree, const char* name);
#endif //DEFLATE_DEBUG_H