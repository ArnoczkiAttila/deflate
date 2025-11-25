//
// Created by Attila on 11/24/2025.
//


#include "status.h"
#include <stdint.h>

#include "bitreader.h"
#include "compress.h"
#include "decompress.h"

#include <stdlib.h>
#include <string.h>

#include "HUFFMAN_TABLE.h"

#define ID 0x1F8B
#define CM 0x08
#define FLAG 0x00
#define MAX_BITS 15 // Max code length in Deflate
#define CL_SYMBOLS 19 // Total symbols in Code Length Alphabet (0-18)

extern Status decompress(char* filename) {
    Status status = {.code = DECOMPRESS_SUCCESS,.message = "Decompression succeeded!"};

    BIT_READER* reader = init_bit_reader(filename);

    if (process_gzip_header(reader)) {
        printf("Valid header!\n");
    }

    const uint8_t cl_order[19] = { 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15 };
    uint16_t cl_lengths[19] = {0};

    uint8_t BFINAL;
    uint8_t BYTYPE;
    uint16_t HLIT;
    uint16_t HDIST;
    uint16_t HCLEN;
    do {
        BFINAL = read_bit(reader);
        BYTYPE = read_bits(reader,2);
        if (BYTYPE != 0b10) {
            status.code  = DECOMPRESS_FAILED;
            status.message = "Found a block with not fixed huffman!";
            return status;
        }
        HLIT = read_bits(reader,5) + 257;
        HDIST = read_bits(reader,5) + 1;
        HCLEN = read_bits(reader, 4) + 4;
        for (uint8_t i = 0; i < HCLEN; i++) {
            cl_lengths[cl_order[i]] = read_bits(reader,3);
            printf("%d %d\n",cl_order[i],cl_lengths[cl_order[i]]);
        }
        uint16_t bl_count[MAX_BITS + 1] = {0};
        for (uint8_t i = 0; i < CL_SYMBOLS; i++) {
            uint8_t length = cl_lengths[i]; // cl_lengths is the array of 19 lengths you populated

            // If the length is non-zero, increment the count for that length
            if (length > 0) {
                // You must ensure length <= MAX_BITS (which it is, max 7 for T_CL)
                bl_count[length]++;
            }
        }

        uint16_t next_code[MAX_BITS + 1];
        uint16_t code = 0;
        next_code[0] = 0; // Not strictly needed, but good practice

        // 2. Calculate the starting code for lengths 1 through MAX_BITS
        for (int L = 1; L <= MAX_BITS; L++) {
            // Add the count of symbols from the previous length
            code = code + bl_count[L - 1];

            // Left shift the accumulated code by 1 bit for the new length
            code <<= 1;
            next_code[L] = code;

        }


        HUFFMAN_CODE* cl_canonical_codes = (HUFFMAN_CODE*) malloc(sizeof(HUFFMAN_CODE)*CL_SYMBOLS);

        // Iterate through the 19 Code Length symbols (index i is the symbol value)
        for (int i = 0; i < CL_SYMBOLS; i++) {
            uint8_t length = cl_lengths[i];

            if (length > 0) {
                // 1. Get the current starting code for this length
                code = next_code[length];

                // 2. Store the symbol's Canonical Code and length
                cl_canonical_codes[i].code = code;
                cl_canonical_codes[i].length = length;

                // 3. Increment the code for the next symbol of the same length
                next_code[length]++;
            }
        }


        // 1. Allocate and Initialize the HuffmanTree structure (T_CL_Tree)
        // Note: Depending on your language/style, this might be a stack-allocated struct or a dynamically allocated pointer.
        HuffmanTree* T_CL_Tree = (HuffmanTree*) malloc(sizeof(HuffmanTree));
        // Initialize metadata (optional but recommended)
        T_CL_Tree->total_symbols = CL_SYMBOLS;
        // Copy the full list of canonical codes for the slow path resolution
        memcpy(T_CL_Tree->codes_list, cl_canonical_codes, sizeof(HUFFMAN_CODE)*CL_SYMBOLS);

        // 2. Populate the Fast Lookup Table
        // You need to call the function we discussed, which fills the 512-entry lookup_table
        // using the generated cl_canonical_codes.
        buildFastLookupTable(
            cl_canonical_codes,
            CL_SYMBOLS,
            T_CL_Tree->lookup_table
        );

        uint16_t total_lengths = HLIT + HDIST;
        // Array to store the combined code lengths for T_LL and T_D (max 286 + 32)
        uint8_t all_lengths[total_lengths];
        uint16_t current_len_index = 0;
        uint8_t previous_len = 0;

        while (current_len_index < total_lengths) {
            // 1. Decode the next symbol using the T_CL tree (0-18)
            uint32_t symbol = decode_symbol(reader, T_CL_Tree);

            if (symbol <= 15) {
                // Case 1: The symbol IS an actual code length (0-15)
                all_lengths[current_len_index++] = (uint8_t)symbol;
                // Keep track of the last *non-zero* length
                previous_len = (symbol != 0) ? (uint8_t)symbol : previous_len;
            } else if (symbol == 16) {
                // Case 2: Repeat previous length (3 to 6 times)
                // Read 2 extra bits: 00 -> 3, 01 -> 4, 10 -> 5, 11 -> 6
                uint32_t repeat_count = read_bits(reader, 2) + 3;

                if (previous_len == 0) { /* ERROR: Cannot repeat a zero length with 16 */ }

                for (int j = 0; j < repeat_count && current_len_index < total_lengths; j++) {
                    all_lengths[current_len_index++] = previous_len;
                }
            } else if (symbol == 17) {
                // Case 3: Repeat length of 0 (3 to 10 times)
                // Read 3 extra bits: 000 -> 3, ..., 111 -> 10
                uint32_t repeat_count = read_bits(reader, 3) + 3;
                for (int j = 0; j < repeat_count && current_len_index < total_lengths; j++) {
                    all_lengths[current_len_index++] = 0;
                }
                previous_len = 0;
            } else if (symbol == 18) {
                // Case 4: Repeat length of 0 (11 to 138 times)
                // Read 7 extra bits: 0000000 -> 11, ..., 1111111 -> 138
                uint32_t repeat_count = read_bits(reader, 7) + 11;
                for (int j = 0; j < repeat_count && current_len_index < total_lengths; j++) {
                    all_lengths[current_len_index++] = 0;
                }
                previous_len = 0;
            }
        }

        uint16_t ll_bl_count[MAX_BITS + 1] = {0};
        for (uint16_t i = 0; i < HLIT; i++) {
            uint8_t length = all_lengths[i];
            if (length > 0) {
                ll_bl_count[length]++;
            }
        }

        // 2. Recalculate next_code (Starting codes) for T_LL
        uint16_t ll_next_code[MAX_BITS + 1];
        uint16_t ll_code = 0;
        ll_next_code[0] = 0;

        for (int L = 1; L <= MAX_BITS; L++) {
            ll_code = (ll_code + ll_bl_count[L - 1]) << 1;
            ll_next_code[L] = ll_code;
        }

        // 3. Assign Final Canonical Codes and Build the Decoder Structure
        HUFFMAN_CODE ll_canonical_codes[HLIT];
        // Re-initialize ll_code to 0 for proper assignment in the final loop.
        ll_code = 0;

        for (uint16_t i = 0; i < HLIT; i++) {
            uint8_t length = all_lengths[i];

            if (length > 0) {
                ll_code = ll_next_code[length];

                ll_canonical_codes[i].code = ll_code;
                ll_canonical_codes[i].length = length;

                ll_next_code[length]++;
            }
        }

        // 4. Create and Populate the HuffmanTree structure (T_LL_Tree)
        HuffmanTree* T_LL_Tree = (HuffmanTree*) malloc(sizeof(HuffmanTree));
        T_LL_Tree->total_symbols = HLIT;
        memcpy(T_LL_Tree->codes_list, ll_canonical_codes, HLIT * sizeof(HUFFMAN_CODE));

        buildFastLookupTable(
            ll_canonical_codes,
            HLIT,
            T_LL_Tree->lookup_table
        );

        uint16_t dist_bl_count[MAX_BITS + 1] = {0};
        for (uint16_t i = 0; i < HDIST; i++) {
            uint8_t length = all_lengths[HLIT + i]; // Start reading from HLIT
            if (length > 0) {
                dist_bl_count[length]++;
            }
        }

        // 2. Recalculate next_code (Starting codes) for T_D
        uint16_t dist_next_code[MAX_BITS + 1];
        uint16_t dist_code = 0;
        dist_next_code[0] = 0;

        for (int L = 1; L <= MAX_BITS; L++) {
            dist_code = (dist_code + dist_bl_count[L - 1]) << 1;
            dist_next_code[L] = dist_code;
        }

        // 3. Assign Final Canonical Codes and Build the Decoder Structure
        HUFFMAN_CODE dist_canonical_codes[HDIST];
        dist_code = 0;

        for (uint16_t i = 0; i < HDIST; i++) {
            uint8_t length = all_lengths[HLIT + i]; // Read from HLIT offset

            if (length > 0) {
                dist_code = dist_next_code[length];

                dist_canonical_codes[i].code = dist_code;
                dist_canonical_codes[i].length = length;

                dist_next_code[length]++;
            }
        }

        // 4. Create and Populate the HuffmanTree structure (T_D_Tree)
        HuffmanTree* T_D_Tree = (HuffmanTree*) malloc(sizeof(HuffmanTree));
        T_D_Tree->total_symbols = HDIST;
        memcpy(T_D_Tree->codes_list, dist_canonical_codes, HDIST * sizeof(HUFFMAN_CODE));

        buildFastLookupTable(
            dist_canonical_codes,
            HDIST,
            T_D_Tree->lookup_table
        );

        while (1) {
            // 1. Decode Literal/Length Symbol using T_LL
            uint16_t symbol = decode_symbol(reader, T_LL_Tree);

            if (symbol <= 255) {
                // LITERAL: Write symbol (byte) to the output buffer
                write_byte_to_output(output_buffer, (uint8_t)symbol);

            } else if (symbol == 256) {
                // EOD: End-of-Block marker
                break;

            } else {
                // LENGTH CODE: Decode the match

                // 2. Decode the Match Length (reads extra bits based on symbol)
                uint16_t length = decode_match_length(reader, symbol);

                // 3. Decode the Distance Symbol using T_D
                uint16_t distance_symbol = decode_symbol(reader, T_D_Tree);

                // 4. Decode the Match Distance (reads extra bits based on distance_symbol)
                uint16_t distance = decode_match_distance(reader, distance_symbol);

                // 5. Perform the history copy (Sliding Window operation)
                copy_from_history(output_buffer, length, distance);
            }
        }
        free(cl_canonical_codes);
        free(T_LL_Tree);
        free(T_CL_Tree);
        free(T_D_Tree);
        break;
    } while (BFINAL != 0b1);
    free(reader);

    return status;
}
