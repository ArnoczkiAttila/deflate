//
// Created by Rendszergazda on 11/23/2025.
//

#include "HUFFMAN_TABLE.h"

#define INVALID_NODE_SYMBOL 286

#include "bitreader.h"
#include "node.h"

#define MAX_BITS 15 // Max code length in Deflate


uint16_t decode_symbol(BIT_READER* reader, const HuffmanTree* tree) {
    // 1. Peek the next FAST_BITS (e.g., 9 bits) from the stream.
    uint16_t peeked_bits = peek_bits(reader, FAST_BITS);

    // 2. Use the peeked bits as an index into the fast lookup table.
    HuffmanEntry entry = tree->lookup_table[peeked_bits];

    // --- Fast Path Check ---
    if (entry.bits <= FAST_BITS) {
        // The symbol was resolved by entry.bits or fewer.

        // Consume the bits used for the successful decode.
        read_bits(reader, entry.bits);

        return entry.symbol;

    } else {
        // --- Slow Path (The code is longer than the fast lookup size) ---

        // This is necessary for codes 10 bits and longer (up to MAX_BITS).
        // Since the fast lookup couldn't resolve it, we need to read bit-by-bit
        // or use a secondary table/trie structure.

        // Example: Traversing the longer codes bit-by-bit (SLOW, but works):

        // Start with the first 9 bits already peeked (we'll consume 1 bit at a time).
        uint16_t current_code = peeked_bits;
        uint8_t bits_consumed = FAST_BITS;

        // Loop until we reach MAX_BITS (or find a match in the longer code structure)
        while (bits_consumed < MAX_BITS) {
            // Read one more bit
            uint8_t new_bit = read_bit(reader);
            bits_consumed++;

            // Append the new bit to the current code (left-shift the code, add the bit)
            current_code = (current_code << 1) | new_bit;

            // --- Secondary Lookup / Full Match Check ---
            // The logic here is highly dependent on how you store the long codes.
            // You must search your full list of canonical codes to see if
            // (current_code, bits_consumed) matches a known symbol.

            // Assuming you have an auxiliary array of ALL canonical codes:
            // if (check_full_code_list(current_code, bits_consumed, &symbol_found)) {
            //     return symbol_found;
            // }

            // Since Deflate is LSB-first, the check is more complex:
            // You must check if the REVERSED current_code of length bits_consumed
            // matches a symbol's canonical code.
        }

        // If the loop finishes without a match (should not happen in valid data)
        // return ERROR_SYMBOL or handle failure.
        return 0; // Placeholder for error handling or final symbol.
    }
}

extern void buildFastLookupTable(
    const HUFFMAN_CODE* canonical_codes,
    int total_symbols,
    HuffmanEntry* lookup_table
) {
    // Initialize the lookup table
    // (Optional: Initialize to a safe 'ERROR' state)
    // for (int k = 0; k < FAST_SIZE; k++) { lookup_table[k].symbol = ERROR_SYMBOL; }

    for (int i = 0; i < total_symbols; i++) {
        uint16_t code = canonical_codes[i].code;
        uint8_t length = canonical_codes[i].length;

        if (length > 0) {
            // Deflate codes are left-aligned for lookup.
            // The code is stored in the MOST SIGNIFICANT bits of the lookup index.

            if (length <= FAST_BITS) {
                // Case 1: Code is short enough for the fast table (e.g., length <= 9)

                // 1. Left-align the code to FAST_BITS
                // The code must be reversed if your reader reads LSB-first (standard Deflate)

                // *** CRITICAL NOTE: Deflate codes are transmitted LSB-first. ***
                // Your lookup table should be indexed by bits in the order they appear.

                // Assuming you've already **bit-reversed** the Canonical Code if needed
                // (or your reader handles LSB-first directly):

                // Use the canonical code 'code' as the start of the index block
                uint16_t index_base = code << (FAST_BITS - length);

                // 2. Calculate the number of entries that point to this symbol
                uint16_t num_entries = 1 << (FAST_BITS - length);

                // 3. Populate all relevant table entries
                for (uint16_t j = 0; j < num_entries; j++) {
                    // The lookup index is the base plus the loop offset
                    uint16_t index = index_base | j;

                    lookup_table[index].symbol = i;
                    lookup_table[index].bits = length;
                }
            } else {
                // Case 2: Code is longer than FAST_BITS (e.g., length > 9)
                // This code is too long for the fast lookup table.
                // It must be handled by the 'slow path' in decode_symbol.
                // For building the table, you can stop here.

                // Optional: Store a pointer/reference to the slow path data
                // in the table entry corresponding to the first FAST_BITS of the long code.
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