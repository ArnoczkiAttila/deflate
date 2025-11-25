//
// Created by Attila Arnóczki on 10/18/2025.
// Node version 0.0.1
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

extern void compressCodeLengths(
    const uint8_t* all_lengths,
    size_t count,
    uint8_t* compressed_lengths, // Output buffer for RLE symbols (0-18)
    uint16_t* cl_frequencies,    // Output array of size 19
    uint8_t* extra_bits_values,  // Output buffer for RLE extra bit values
    size_t* compressed_count     // Final count of symbols generated
);

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