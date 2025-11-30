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

#include "debug.h"
#include "HUFFMAN_TABLE.h"

#define ID 0x1F8B
#define CM 0x08
#define FLAG 0x00
#define MAX_BITS 15 // Max code length in Deflate
#define CL_SYMBOLS 19 // Total symbols in Code Length Alphabet (0-18)
#define BYTE uint8_t
#define WORD uint16_t

#define WINDOW_SIZE 32768
#define BUFFER_SIZE (WINDOW_SIZE * 2)

// --- DEFLATE Tables (RFC 1951) ---

// Extra bits for Length Codes (257-285)
static const int length_extra_bits[29] = {
    0, 0, 0, 0, 0, 0, 0, 0, // 257-264
    1, 1, 1, 1,             // 265-268
    2, 2, 2, 2,             // 269-272
    3, 3, 3, 3,             // 273-276
    4, 4, 4, 4,             // 277-280
    5, 5, 5, 5,             // 281-284
    0                       // 285 (always 258)
};

// Base values for Length Codes (257-285)
static const int length_base[29] = {
    3, 4, 5, 6, 7, 8, 9, 10,  // 257-264
    11, 13, 15, 17,           // 265-268
    19, 23, 27, 31,           // 269-272
    35, 43, 51, 59,           // 273-276
    67, 83, 99, 115,          // 277-280
    131, 163, 195, 227,       // 281-284
    258                       // 285
};

// Extra bits for Distance Codes (0-29)
static const int dist_extra_bits[30] = {
    0, 0, 0, 0,             // 0-3
    1, 1, 2, 2,             // 4-7
    3, 3, 4, 4,             // 8-11
    5, 5, 6, 6,             // 12-15
    7, 7, 8, 8,             // 16-19
    9, 9, 10, 10,           // 20-23
    11, 11, 12, 12,         // 24-27
    13, 13                  // 28-29
};

// Base values for Distance Codes (0-29)
static const int dist_base[30] = {
    1, 2, 3, 4,             // 0-3
    5, 7, 9, 13,            // 4-7
    17, 25, 33, 49,         // 8-11
    65, 97, 129, 193,       // 12-15
    257, 385, 513, 769,     // 16-19
    1025, 1537, 2049, 3073, // 20-23
    4097, 6145, 8193, 12289,// 24-27
    16385, 24577            // 28-29
};

static BIT_WRITER* openBIT_WRITER(const char* filename) {
    BIT_WRITER* bw = initBIT_WRITER(BUFFER_SIZE);
    const size_t fileNameLen = strlen(filename);

    bw->fileName = (char*) malloc(sizeof(char)*(fileNameLen-3+1)); //subtract the .gz and then add one byte for the '\0'

    if (bw->fileName == NULL) {
        freeBIT_WRITER(bw);
        return NULL;
    }

    for (size_t i = 0; i < fileNameLen-3; i++) {
        bw->fileName[i] = filename[i];
    }

    bw->fileName[fileNameLen-3] = '\0';
    FILE* output = fopen(bw->fileName,"wb");

    if (output==NULL) {
        freeBIT_WRITER(bw);
        free(bw->fileName);
        return NULL;
    }

    bw->file = output;
    return bw;
}


extern STATUS* decompress(const char* filename) {

    STATUS* status = initSTATUS();
    status->code = DECOMPRESS_SUCCESS;
    createSTATUSMessage(status,"Decompression succeeded!");

    BIT_READER* reader = init_bit_reader(filename);
    BIT_WRITER* bw = openBIT_WRITER(filename);

    if (process_gzip_header(reader)) {
        printf("Valid header!\n");
    }

    const BYTE cl_order[19] = { 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15 };
    WORD cl_lengths[19] = {0};

    BYTE BFINAL;
    BYTE BYTYPE;
    size_t blockCount = 0;
    do {
        BFINAL = read_bit(reader);
        BYTYPE = read_bits(reader,2);
        if (BYTYPE != 0b10) {
            status->code  = DECOMPRESS_FAILED;
            createSTATUSMessage(status, "Found a block with not fixed huffman!");
            free(reader);
            freeBIT_WRITER(bw);
            return status;
        }
        blockCount++;
        WORD HLIT = read_bits(reader, 5) + 257;
        WORD HDIST = read_bits(reader, 5) + 1;
        WORD HCLEN = read_bits(reader, 4) + 4;
        printf("HCLEN: %d\n",HCLEN);

        for (WORD i = 0; i < HCLEN; i++) {
            cl_lengths[cl_order[i]] = read_bits(reader,3);
            printf("%d %d\n",cl_order[i],cl_lengths[cl_order[i]]);
        }
        printf("\n\n");
        // --- 1. Build Code Length Tree ---
        WORD bl_count[MAX_BITS + 1] = {0};
        for (BYTE i = 0; i < CL_SYMBOLS; i++) {
            if (cl_lengths[i] > 0) bl_count[cl_lengths[i]]++;
        }

        WORD next_code[MAX_BITS + 1];
        WORD code = 0;
        next_code[0] = 0;

        for (int L = 1; L <= MAX_BITS; L++) {
            code = (code + bl_count[L - 1]) << 1;
            next_code[L] = code;
        }

        HUFFMAN_CODE* cl_canonical_codes = (HUFFMAN_CODE*) malloc(sizeof(HUFFMAN_CODE)*CL_SYMBOLS);

        for (int i = 0; i < CL_SYMBOLS; i++) {
            BYTE length = cl_lengths[i];
            if (length > 0) {
                cl_canonical_codes[i].code = next_code[length];
                cl_canonical_codes[i].length = length;
                next_code[length]++;
            } else {
                cl_canonical_codes[i].length = 0;
            }
        }

        HuffmanTree* T_CL_Tree = (HuffmanTree*) malloc(sizeof(HuffmanTree));
        T_CL_Tree->total_symbols = CL_SYMBOLS;
        memcpy(T_CL_Tree->codes_list, cl_canonical_codes, sizeof(HUFFMAN_CODE)*CL_SYMBOLS);

        buildFastLookupTable(cl_canonical_codes, CL_SYMBOLS, T_CL_Tree->lookup_table);

        // --- 2. Decode Literal/Length and Distance Tree Lengths ---
        WORD total_lengths = HLIT + HDIST;
        BYTE* all_lengths = (BYTE*) calloc(total_lengths, sizeof(BYTE));
        WORD current_len_index = 0;
        BYTE previous_len = 0;

        while (current_len_index < total_lengths) {
            uint32_t symbol = decode_symbol(reader, T_CL_Tree);
            if (symbol <= 15) {
                all_lengths[current_len_index++] = (BYTE)symbol;
                previous_len = (BYTE)symbol;
            } else if (symbol == 16) {
                uint32_t repeat_count = read_bits(reader, 2) + 3;
                for (int j = 0; j < repeat_count && current_len_index < total_lengths; j++) {
                    all_lengths[current_len_index++] = previous_len;
                }
            } else if (symbol == 17) {
                uint32_t repeat_count = read_bits(reader, 3) + 3;
                for (int j = 0; j < repeat_count && current_len_index < total_lengths; j++) {
                    all_lengths[current_len_index++] = 0;
                }
                previous_len = 0;
            } else if (symbol == 18) {
                uint32_t repeat_count = read_bits(reader, 7) + 11;
                for (int j = 0; j < repeat_count && current_len_index < total_lengths; j++) {
                    all_lengths[current_len_index++] = 0;
                }
                previous_len = 0;
            }
        }

        // --- 3. Build Literal/Length Tree (T_LL) ---
        WORD ll_bl_count[MAX_BITS + 1] = {0};
        for (WORD i = 0; i < HLIT; i++) {
            if (all_lengths[i] > 0) ll_bl_count[all_lengths[i]]++;
        }

        WORD ll_next_code[MAX_BITS + 1];
        WORD ll_code = 0;
        ll_next_code[0] = 0;
        for (int L = 1; L <= MAX_BITS; L++) {
            ll_code = (ll_code + ll_bl_count[L - 1]) << 1;
            ll_next_code[L] = ll_code;
        }

        HUFFMAN_CODE* ll_canonical_codes = (HUFFMAN_CODE*) malloc(HLIT * sizeof(HUFFMAN_CODE));
        for (WORD i = 0; i < HLIT; i++) {
            BYTE length = all_lengths[i];
            if (length > 0) {
                ll_canonical_codes[i].code = ll_next_code[length];
                ll_canonical_codes[i].length = length;
                ll_next_code[length]++;
            } else {
                ll_canonical_codes[i].length = 0;
            }
        }

        HuffmanTree* T_LL_Tree = (HuffmanTree*) malloc(sizeof(HuffmanTree));
        T_LL_Tree->total_symbols = HLIT;
        memcpy(T_LL_Tree->codes_list, ll_canonical_codes, HLIT * sizeof(HUFFMAN_CODE));
        buildFastLookupTable(ll_canonical_codes, HLIT, T_LL_Tree->lookup_table);
        print_debug_tree(T_CL_Tree,"Literal/Length");
        // --- 4. Build Distance Tree (T_D) ---
        WORD dist_bl_count[MAX_BITS + 1] = {0};
        for (WORD i = 0; i < HDIST; i++) {
            BYTE length = all_lengths[HLIT + i];
            if (length > 0) dist_bl_count[length]++;
        }

        WORD* distanceNextCode = (WORD*) malloc(sizeof(WORD)*(MAX_BITS + 1));
        WORD dist_code = 0;
        distanceNextCode[0] = 0;
        for (int L = 1; L <= MAX_BITS; L++) {
            dist_code = (dist_code + dist_bl_count[L - 1]) << 1;
            distanceNextCode[L] = dist_code;
        }

        HUFFMAN_CODE* distanceCanonicalCodes = (HUFFMAN_CODE*) malloc(HDIST*sizeof(HUFFMAN_CODE));
        for (WORD i = 0; i < HDIST; i++) {
            BYTE length = all_lengths[HLIT + i];
            if (length > 0) {
                distanceCanonicalCodes[i].code = distanceNextCode[length];
                distanceCanonicalCodes[i].length = length;
                distanceNextCode[length]++;
            } else {
                distanceCanonicalCodes[i].length = 0;
            }
        }

        HuffmanTree* T_D_Tree = (HuffmanTree*) malloc(sizeof(HuffmanTree));
        T_D_Tree->total_symbols = HDIST;
        memcpy(T_D_Tree->codes_list, distanceCanonicalCodes, HDIST * sizeof(HUFFMAN_CODE));
        buildFastLookupTable(distanceCanonicalCodes, HDIST, T_D_Tree->lookup_table);

        // --- 5. Main Decompression Loop ---
        size_t count = 0;
        while (1) {
            WORD symbol = decode_symbol(reader, T_LL_Tree);
            //printf("%d\n",symbol);
            if (symbol <= 255) {
                // Literal
                // printf("Lit: %c\n", (char)symbol); // Debug
                addFastByte(bw,(BYTE)symbol);
                count++;
            } else if (symbol == 256) {
                // EOB
                printf("EOB reached at count: %llu\n", count);
                break;
            } else {
                // Length/Distance Pair
                if (symbol > 285) {
                    printf("Error: Invalid length symbol %d\n", symbol);
                    break;
                }

                // A. Decode Length
                // 1. Get base length
                int length_idx = symbol - 257;
                int length = length_base[length_idx];

                // 2. Read extra bits
                int extra_bits = length_extra_bits[length_idx];
                if (extra_bits > 0) {
                    int extra = read_bits(reader, extra_bits);
                    length += extra;
                }

                // B. Decode Distance
                WORD dist_symbol = decode_symbol(reader, T_D_Tree);
                if (dist_symbol > 29) {
                    printf("Error: Invalid distance symbol %d\n", dist_symbol);
                    //break;
                }

                // 1. Get base distance
                int dist_base_val = dist_base[dist_symbol];

                // 2. Read extra bits
                int dist_extra = dist_extra_bits[dist_symbol];
                int distance = dist_base_val;
                if (dist_extra > 0) {
                    int extra = read_bits(reader, dist_extra);
                    distance += extra;
                }

                // printf("Len: %d, Dist: %d\n", length, distance); // Debug

                copyFromBufferHistory(bw,distance,length);
                count += length;
            }
        }
        printf("block count: %llu\n",blockCount);
        if (BFINAL == 0b01) printf("final block\n");
        // Cleanup
        free(cl_canonical_codes);
        free(T_LL_Tree);
        free(T_CL_Tree);
        free(T_D_Tree);
        free(all_lengths);
        free(distanceCanonicalCodes);
        free(distanceNextCode);
        free(ll_canonical_codes); // Don't forget this one
    } while (BFINAL != 0b1);
    printf("Kilépve\n");

    freeBIT_READER(reader);
    freeBIT_WRITER(bw);

    return status;
}
