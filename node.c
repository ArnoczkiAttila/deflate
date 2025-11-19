//
// Created by Attila Arnóczki on 10/18/2025.
// Node version 0.0.1
//

#include "node.h"

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#define INVALID_NODE_SYMBOL 286

Node* createNode(unsigned short usSymbol, int freq) {
    Node* n = (Node*)malloc(sizeof(Node));
    if (n == NULL) { return NULL; }
    n->iFrequency = freq;
    n->usSymbol = usSymbol;
    n->pnLeft = n->pnRight = NULL;
    return n;
}

extern MinHeap* createMinHeap(int capacity) {
    MinHeap* minHeap = (MinHeap*) malloc(sizeof(MinHeap));
    if (minHeap == NULL) {
        return NULL;
    }
    minHeap->iCapacity = capacity;
    minHeap->iSize = 0;
    Node** array = (Node**) malloc(sizeof(Node*)*capacity);
    if (array == NULL) {
        free(minHeap);
        return NULL;
    }
    minHeap->ppnArray = array;
    return minHeap;
}

extern void addToMinHeap(MinHeap* minHeap, Node* node) {
    minHeap->ppnArray[minHeap->iSize] = node;
    minHeap->iSize++;
}

void swapNodePointers(Node** a, Node** b) {
    Node* t = *a;
    *a = *b;
    *b = t;
}

void minHeapify(MinHeap* minHeap, int i) {
    // 1. Calculate indices of children
    int iLeft = 2 * i + 1;  // Index of the left child
    int iRight = 2 * i + 2; // Index of the right child
    int iSmallest = i;      // Assume the current node is the smallest

    // 2. Check if the left child exists and is smaller than the current node
    if (iLeft < minHeap->iSize &&
        minHeap->ppnArray[iLeft]->iFrequency < minHeap->ppnArray[iSmallest]->iFrequency) {

        iSmallest = iLeft;
        }

    // 3. Check if the right child exists and is smaller than the current smallest (i or iLeft)
    if (iRight < minHeap->iSize &&
        minHeap->ppnArray[iRight]->iFrequency < minHeap->ppnArray[iSmallest]->iFrequency) {

        iSmallest = iRight;
        }

    // 4. If the smallest is not the current node, swap and recursively call minHeapify
    if (iSmallest != i) {
        swapNodePointers(&minHeap->ppnArray[i], &minHeap->ppnArray[iSmallest]);

        // Recursively call minHeapify on the affected subtree
        minHeapify(minHeap, iSmallest);
    }
    // If iSmallest == i, the heap property is satisfied in this subtree, and the function terminates.
}


extern Node* extractMin(MinHeap* minHeap) {
    if (minHeap->iSize <= 0) {
        return NULL; // Heap is empty
    }

    // 1. Store the root (minimum)
    Node* root = minHeap->ppnArray[0];

    // 2. Move the last node to the root
    minHeap->iSize--;
    minHeap->ppnArray[0] = minHeap->ppnArray[minHeap->iSize];
    // Note: The last element is effectively removed by decrementing iSize

    // 3. Sift down the new root to maintain the heap property
    if (minHeap->iSize > 0) {
        minHeapify(minHeap, 0); // O(log n) operation
    }

    return root;
}

extern void freeMinHeap(MinHeap* minHeap) {
    for (int i = 0; i < minHeap->iSize; i++) {
        if (minHeap->ppnArray[i] != NULL) {
            free(minHeap->ppnArray[i]);
        }
    }
    free(minHeap->ppnArray);
    free(minHeap);
}

void printHeap(MinHeap* minHeap) {
    printf("Heap Array (Size %d): [", minHeap->iSize);
    for (int i = 0; i < minHeap->iSize; i++) {
        printf("%d", minHeap->ppnArray[i]->iFrequency);
        if (i < minHeap->iSize - 1) {
            printf(", ");
        }
    }
    printf("]\n");
}

void buildMinHeap(MinHeap* minHeap) {
    int n = minHeap->iSize;
    // Start from the last internal node: floor(n/2) - 1
    for (int i = (n / 2) - 1; i >= 0; --i) {
        minHeapify(minHeap, i);
    }
}

// Function to get the index of the parent node
int parentIndex(int i) {
    return (i - 1) / 2;
}

// Function to get the index of the left child
int leftChildIndex(int i) {
    return 2 * i + 1;
}


void insertMinHeap(MinHeap* minHeap, Node* newNode) {
    // Check for capacity
    if (minHeap->iSize == minHeap->iCapacity) {
        fprintf(stderr, "Error: Heap capacity exceeded.\n");
        return;
    }

    // 1. Place the new node at the end of the array
    int i = minHeap->iSize;
    minHeap->ppnArray[i] = newNode;
    minHeap->iSize++;

    // 2. "Sift up" the new node to maintain the heap property
    while (i != 0 && minHeap->ppnArray[parentIndex(i)]->iFrequency > minHeap->ppnArray[i]->iFrequency) {
        swapNodePointers(&minHeap->ppnArray[i], &minHeap->ppnArray[parentIndex(i)]);
        i = parentIndex(i);
    }
}

// --- HUFFMAN TREE CONSTRUCTION ---

/**
 * @brief Creates the Huffman tree from the populated Min-Heap.
 * * This is the greedy algorithm core: repeatedly combine the two smallest nodes.
 * * @param minHeap The initialized Min-Heap containing all leaf nodes.
 * @return Node* The root of the completed Huffman tree.
 */
Node* buildHuffmanTree(MinHeap* minHeap) {
    Node *pnLeft, *pnRight, *pnParent;
    while (minHeap->iSize > 1) {

        pnLeft = extractMin(minHeap);
        pnRight = extractMin(minHeap);

        pnParent = createNode(286, 0);

        pnParent->pnLeft = pnLeft;
        pnParent->pnRight = pnRight;

        pnParent->iFrequency = pnLeft->iFrequency + pnRight->iFrequency;

        insertMinHeap(minHeap, pnParent);
    }

    return extractMin(minHeap);
}

/**
 * @brief Traverses a Huffman tree to determine the bit length (depth) for every symbol.
 * * @param current_node The current node in the traversal (start with the tree root).
 * @param uiCurrentDepth The depth of the current node (start with 0 for the root).
 * @param uiLengthCodes The array where the resulting code lengths are stored.
 */
extern void extract_code_lengths(Node* npCurrent, uint8_t uiCurrentDepth, uint8_t* uiLengthCodes) {

    if (npCurrent == NULL) {
        return;
    }

    if (npCurrent->usSymbol != INVALID_NODE_SYMBOL) { //tehát valid
        uiLengthCodes[npCurrent->usSymbol] = uiCurrentDepth;
        return;
    }

    extract_code_lengths(npCurrent->pnLeft, uiCurrentDepth + 1, uiLengthCodes);

    extract_code_lengths(npCurrent->pnRight, uiCurrentDepth + 1, uiLengthCodes);
}