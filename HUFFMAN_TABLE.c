//
// Created by Rendszergazda on 11/23/2025.
//

#include "HUFFMAN_TABLE.h"

#define INVALID_NODE_SYMBOL 286

#include <string.h>

#include "bitreader.h"
#include "node.h"

#define MAX_BITS 15 // Max code length in Deflate

uint16_t reverse_bits(uint16_t val, int bits) {
    uint16_t res = 0;
    for (int i = 0; i < bits; i++) {
        if (val & (1 << i)) {
            res |= (1 << (bits - 1 - i));
        }
    }
    return res;
}

/**
 * @bug There are cases where this function can not decode the distance symbol or the length symbol and the whole code breaks.
 * I couldn't find it in time.
 *
 * @param reader
 * @param tree
 * @return
 */
uint16_t decode_symbol(BIT_READER* reader, const HuffmanTree* tree) {
    // 1. FAST PATH: Peek 9 bits (or whatever FAST_BITS is)
    uint16_t peek_val = peek_bits(reader, FAST_BITS);
    printf("peaked bits:%d\n",peek_val);
    HuffmanEntry entry = tree->lookup_table[peek_val];

    // Check if we found a valid symbol in the fast table
    if (entry.bits > 0 && entry.bits <= FAST_BITS) {
        read_bits(reader, entry.bits); // Consume the bits
        return entry.symbol;
    }
    printf("turning to slowpath.\n");
    // 2. SLOW PATH: The code is longer than FAST_BITS (e.g. 10-15 bits)
    // We must search for a match bit-by-bit.

    uint16_t current_code = 0;
    int cur_len = 0;

    // Optimization: We know the first FAST_BITS didn't match a short code,
    // but they ARE the prefix of the long code.
    // We can start by consuming those bits.
    // (Or, simpler for now: just restart from bit 0 to be safe and clear)

    // Let's use the robust "try 1 bit, then 2..." method you described.
    // Because this happens rarely (only for very rare symbols), speed is less critical.

    while (cur_len < MAX_BITS) {
        // Read 1 bit from file
        int bit = read_bit(reader);
        cur_len++;

        // Add bit to current_code.
        // Note: Since we need to match the "MSB-First" definition of codes,
        // and we are reading bits in order...
        // current_code = (current_code << 1) | bit;
        // This builds the code in MSB-first order (matching the canonical_codes).
        current_code = (current_code << 1) | bit;

        for (int i = 0; i < tree->total_symbols; i++) {
            // Check length match first (fastest reject)
            if (tree->codes_list[i].length == cur_len) {
                // Check code value match
                if (tree->codes_list[i].code == current_code) {
                    return i; // Symbol found!
                }
            }
        }
    }
    printf("no code found\n");
    return 0xFFFF;
}

extern void buildFastLookupTable(
    const HUFFMAN_CODE* canonical_codes,
    int total_symbols,
    HuffmanEntry* lookup_table
) {

    memset(lookup_table, 0, sizeof(HuffmanEntry) * (1 << FAST_BITS));

    for (int i = 0; i < total_symbols; i++) {
        uint16_t raw_code = canonical_codes[i].code;
        uint8_t length = canonical_codes[i].length;

        // We only care about codes that fit in the fast table
        if (length > 0 && length <= FAST_BITS) {

            uint16_t reversed_code = reverse_bits(raw_code, length);

            int step = 1 << length;

            for (int index = reversed_code; index < (1 << FAST_BITS); index += step) {
                lookup_table[index].symbol = i;
                lookup_table[index].bits = length;
            }
        }
    }
}

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