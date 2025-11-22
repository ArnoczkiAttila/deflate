//
// Created by Attila Arnóczki on 10/18/2025.
// Node version 0.0.1
//

#ifndef HUFFMAN_NODE_H
#define HUFFMAN_NODE_H


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

MinHeap* createMinHeap(int capacity);

void addToMinHeap(MinHeap* minHeap, Node* node);

void printHeap(MinHeap* minHeap);

Node* extractMin(MinHeap* minHeap);

Node* buildHuffmanTree(MinHeap* minHeap);

void buildMinHeap(MinHeap* minHeap);

void freeMinHeap(MinHeap* minHeap);

Node* createNode(unsigned short usSymbol, int freq);


#endif //HUFFMAN_NODE_H