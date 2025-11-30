//
// Created by Attila on 10/18/2025.
//

#ifndef HUFFMAN_NODE_H
#define HUFFMAN_NODE_H

#include <stdint.h>

typedef struct Node {
    int iFrequency;
    unsigned short usSymbol;
    struct Node* pnLeft;
    struct Node* pnRight;
} Node;

typedef struct {
    int iSize;
    int iCapacity;
    Node** ppnArray;
} MinHeap;

extern void flattenTree(uint8_t* lengths, int num_symbols, int max_depth);

extern void print_tree_visual(Node* node, int level, char* prefix);

extern void compressCodeLengths(const uint8_t* all_lengths, size_t count, uint8_t* compressed_lengths, uint16_t* cl_frequencies, uint8_t* extra_bits_values, size_t* compressed_count);

extern void findCodeLengthsInTree(Node* node, uint8_t* lengths, uint8_t depth);

extern MinHeap* createMinHeap(int capacity);

extern void addToMinHeap(MinHeap* minHeap, Node* node);

extern void printHeap(MinHeap* minHeap);

extern Node* extractMin(MinHeap* minHeap);

extern Node* buildHuffmanTree(MinHeap* minHeap);

extern void buildMinHeap(MinHeap* minHeap);

extern void freeMinHeap(MinHeap* minHeap);

extern Node* createNode(unsigned short usSymbol, int freq);

extern void freeTree(Node* top);

#endif //HUFFMAN_NODE_H