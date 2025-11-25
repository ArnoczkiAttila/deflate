//
// Created by Rendszergazda on 11/23/2025.
//

#ifndef DEFLATE_HUFFMAN_TABLE_H
#define DEFLATE_HUFFMAN_TABLE_H

#include <stdint.h>

#include "bitreader.h"
#include "node.h"

typedef struct {
    uint16_t code; //for example 101 in binary which means right, left, right in the tree
    uint8_t length; //in this example 3 which says how many bits are there
} HUFFMAN_CODE;

#define FAST_BITS 9
#define FAST_SIZE (1 << FAST_BITS) // 512 entries
#define MAX_CODE_SYMBOLS 286 // Max symbols for T_LL (the largest tree)

typedef struct {
    uint16_t symbol; // The decoded symbol
    uint8_t bits;   // Number of bits consumed (Length)
} HuffmanEntry;

// --- 2. Full Code/Length Storage (for the Slow Path) ---
// This stores the mathematically generated canonical codes for *all* symbols.
typedef struct {
    uint16_t code;   // The Canonical Code value (must be bit-reversed if using LSB-first reading)
    uint8_t length; // The length of the code (up to 15)
} CanonicalCode;

// --- 3. The Unified Tree Structure ---
typedef struct {
    // I. Fast Lookup Table: Resolves all codes <= FAST_BITS
    HuffmanEntry lookup_table[1 << FAST_BITS];

    // II. Storage for Long Codes (The Slow Path Data):
    // Used to resolve codes longer than FAST_BITS.
    // This allows the slow path in decode_symbol to check all possible codes.
    CanonicalCode codes_list[MAX_CODE_SYMBOLS];

    // III. Metadata
    uint16_t total_symbols; // e.g., 19, HLIT, or HDIST+1
    uint8_t max_length;     // Max code length observed in this tree (up to 15)

} HuffmanTree;


extern void buildCodeLookupTable(Node* node, HUFFMAN_CODE* table, uint16_t current_code, int depth);

extern void buildFastLookupTable(
    const HUFFMAN_CODE* canonical_codes,
    int total_symbols,
    HuffmanEntry* lookup_table
);
uint16_t decode_symbol(BIT_READER* reader, const HuffmanTree* tree);
#endif //DEFLATE_HUFFMAN_TABLE_H