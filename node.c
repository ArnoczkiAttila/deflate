//
// Created by Attila Arnóczki on 10/18/2025.
// Node version 0.0.1
//

#include "node.h"

#include <stdlib.h>
#include <stdio.h>


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

    // Loop until only one node (the root) remains in the heap
    while (minHeap->iSize > 1) {
        // 1. Extract the two nodes with the minimum frequency (lowest priority)
        pnLeft = extractMin(minHeap);
        pnRight = extractMin(minHeap);

        // 2. Create a new internal node (the parent)
        // Set the symbol to 286, which is outside the Deflate Literal/Length range (0-285)
        pnParent = createNode(286, 0);

        // 3. Assign the extracted nodes as children
        // The order of pnLeft/pnRight is arbitrary but consistent
        pnParent->pnLeft = pnLeft;
        pnParent->pnRight = pnRight;

        // 4. Set the new node's frequency (sum of children)
        pnParent->iFrequency = pnLeft->iFrequency + pnRight->iFrequency;

        // 5. Insert the new internal node back into the heap
        insertMinHeap(minHeap, pnParent);
    }

    // The last remaining node is the root of the Huffman Tree
    return extractMin(minHeap);
}

