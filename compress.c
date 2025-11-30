//
// Created by Arnóczki Attila on 11/19/2025.
//

#include "compress.h"
#include "debugmalloc.h"

#include <stdio.h>
#include <string.h>

#include "bitwriter.h"
#include "CRC_CHECKSUM.h"
#include "distance.h"
#include "HUFFMAN_TABLE.h"
#include "length.h"
#include "LZ77.h"
#include "node.h"
#include "STATUS.h"

#define HASH_BITS 15
#define HASH_SHIFT 5
#define HASH_MASK 0x7FFF // 32767 for 15 bits
#define HASH_SIZE (1 << HASH_BITS) // 32768
#define WINDOW_SIZE 32768
#define BUFFER_SIZE (WINDOW_SIZE * 2)
#define EMPTY_INDEX 0xFFFF
#define LITERAL_LENGTH_SIZE 286
#define END_OF_BLOCK 256
#define DISTANCE_CODE_SIZE 30
#define CODE_LENGTH_FREQUENCIES 19

#define BYTE uint8_t

/**
 * @brief Subtract Window Size From Hash Table
 *
 * When a shift occurs in the buffer (when the buffer's second half gets copied to the first half)
 * we need to subtract the WINDOW_SIZE from all valid entries in the hashTable so that the newly
 * copied data can have a history from the previously processed data.
 *
 * @param uiarrHashTable The hashTable which stores the last occurrence of a 3 byte hash.
 *
 * @returns void
 * Maximum memory required:
 *  - 32bit systems: 8 bytes
 *  - 64bit systems: 16 bytes
 */
static void vfSubtractWindowSizeFromHashTable(uint16_t *uiarrHashTable) {
    for (int i = 0; i < HASH_SIZE; i++) {
        if (uiarrHashTable[i] >= WINDOW_SIZE) {
            uiarrHashTable[i] -= WINDOW_SIZE;
        }
    }
}

/**
 * @brief Generate Hash Key
 *
 * This function generates a Hash key which can be used to access values in the hashTable.
 * The process is very simple. Shift the first byte at P by HASH_SHIFT and then xor the result
 * with P+1 and P+2. With this simple logic we've created a key which actually depends on 3 bytes
 * and can index a 32kb hashTable.
 *
 * @param p The pointer for the buffer.
 *
 * @returns uint16_t Hash key.
 *
*  Maximum memory required:
 *  - 32bit systems: 4 bytes
 *  - 64bit systems: 8 bytes
 */
static uint16_t uifGenerateHashKey(const unsigned char *p) {
    return (uint16_t) ((p[0] << HASH_SHIFT) ^ p[1] ^ p[2]) & HASH_MASK;
}

/**
 * @brief Init Buffer only allocates BUFFER_SIZE byte unsigned char and returns with its pointer.
 * If the memory allocation fails then it returns with a NULL pointer.
 *
 * @returns unsigned char* The freshly allocated BUFFER_SIZE sized buffer.
 *
 *  Maximum memory required:
 *  - 32bit systems: BUFFER_SIZE+WINDOW_SIZE + 4 bytes
 *  - 64bit systems: BUFFER_SIZE+WINDOW_SIZE + 8 bytes
 */
static unsigned char *cpInitBuffer(void) {
    unsigned char *ucpBuffer = (unsigned char *) malloc(BUFFER_SIZE+WINDOW_SIZE);
    if (ucpBuffer == NULL) {
        return NULL;
    };
    return ucpBuffer;
}

/**
 * @brief Find Match Length
 *
 * The function takes in 3 pointers from unsigned char type and starts counts up the matching bytes at (ucpCurrent + n)
 * and (ucpOld + n) until there is a missmatch. If we reach a total length of 258 or ucpCurrent reaches ucpInputEndPointer,
 * we stop the search since in deflate the longest match can only be 258.
 *
 * @param ucpCurrent Current pointer
 * @param ucpOld Old pointer
 * @param ucpInputEndPointer This is the maximum Current pointer can be (the boundary of the buffer)
 *
 * @return int The length of the match in bytes
 *
 * Maximum memory required:
 *  - 32bit systems: 24 bytes
 *  - 64bit systems: 48 bytes
 */
static int iFindMatchLength(const unsigned char *ucpCurrent, const unsigned char *ucpOld,
                            const unsigned char *ucpInputEndPointer) {
    ptrdiff_t pdiffAvailableBytes = ucpInputEndPointer - ucpCurrent;

    int iMaxCheckLength = 258;
    if (pdiffAvailableBytes < iMaxCheckLength) {
        iMaxCheckLength = (int) pdiffAvailableBytes;
    }

    int iLength = 0;


    while (iLength < iMaxCheckLength) {
        if (ucpCurrent[iLength] == ucpOld[iLength]) {
            iLength++;
        } else {
            break;
        }
    }

    return iLength;
}

/**
 * @brief Reset Hash Table values
 *
 * The fvpResetHashTable function takes an uint16_t pointer and uses the
 * predefined variables (EMPTY_INDEX and HASH_SIZE) for setting a default value
 * for the whole table.
 *
 * The default value defined in EMPTY_INDEX is 0xFFFF.
 *
 * @param uiarrHashTable The pointer to the hashTable.
 *
 * @returns void* or NULL
 *
 * Maximum memory required:
 *  - 32bit systems: 4 bytes
 *  - 64bit systems: 8 bytes
 */
static void *fvpResetHashTable(uint16_t *uiarrHashTable) {
    return memset(uiarrHashTable, EMPTY_INDEX, 2 * HASH_SIZE);
}


/**
 * @brief initHashTable returns with an allocated hashTable. Required space in memory is 2*HASH_SIZE.
 * Must be freed afterward.
 *
 * @returns uint16_t* The pointer to the hashTable or NULL
 *
 *  Maximum memory required:
 *  - 32bit systems: 2 * HASH_SIZE + 4  bytes
 *  - 64bit systems: 2 * HASH_SIZE + 8 bytes
 */
static uint16_t *initHashTable(void) {
    uint16_t *uipHashTable = malloc(2 * HASH_SIZE);
    if (uipHashTable == NULL) {
        return NULL;
    }
    if (fvpResetHashTable(uipHashTable) == NULL) {
        free(uipHashTable);
        return NULL;
    }
    return uipHashTable;
}

/**
 * @brief Opens a file in rb (read binary) mode.
 *
 * @param filename The name of the file. (probably with absolute path)
 *
 * @return FILE* or NULL
 *
 *  Maximum memory required:
 *  - 32bit systems: 8 bytes
 *  - 64bit systems: 16 bytes
 */
extern FILE *ffOpenFile(const char *filename) {
    FILE *fpFile = fopen(filename, "rb");
    return fpFile;
}

/**
 * @brief Compress Data
 *
 * This function takes in a BUFFER SIZED buffer containing BYTES from a file, and fills up an LZ77_buffer containing
 * match/literal distance/length codes which will be used later in the processBlock function.
 *
 * @param ucpBuffer The start of the buffer (pointer).
 * @param bytesRead The bytes processed in this function.
 * @param hash_table The hash lookup table for matches.
 * @param output_ucpBuffer The LZ77_buffer containing the matches/literals.
 *
 * @returns void
 *
 * Maximum memory required:
 *  - 32bit systems: 40 bytes
 *  - 64bit systems: 80 bytes
 */
extern void compressData(const unsigned char *ucpBuffer, const size_t bytesRead, uint16_t *hash_table,
                         LZ77_buffer *output_ucpBuffer) {
    // --- Set the End Pointer ---
    const unsigned char *ucpInputEndPointer = ucpBuffer + bytesRead;

    // --- Loop Control ---
    // Loop only up to (bytesRead - 2) because we need at least 3 bytes to hash/match.
    int i;
    for (i = 0; i < (int) bytesRead - 2;) {
        const uint16_t hashKey = uifGenerateHashKey(ucpBuffer + i);
        const uint16_t hashIndex = hash_table[hashKey];

        int bestLength = 0;
        int bestDistance = 0;

        if (hashIndex != EMPTY_INDEX) {
            int distance = i - hashIndex;

            // Is distance within the 32KB LZ77 window?
            if (distance > 0 && distance <= WINDOW_SIZE) {
                bestLength = iFindMatchLength(ucpBuffer + i, ucpBuffer + hashIndex, ucpInputEndPointer);
                bestDistance = distance;

                if (bestLength < 3) {
                    bestLength = 0; // Discard match if too short
                }
            }
        }

        hash_table[hashKey] = (uint16_t) i;

        if (bestLength >= 3) {
            // Output the match token
            if (bestDistance>40000) {
                printf("Valamiért van egy 40k-nál nagyobb distance!\n");
            }
            appendToken(output_ucpBuffer, createMatchLZ77(bestDistance, bestLength));
            i += bestLength;
        } else {
            // NO MATCH (Length < 3) or Invalid distance

            // Output the literal byte at the current position 'i'.
            //printf("Literal: %c\n", ucpBuffer[i]);
            appendToken(output_ucpBuffer, createLiteralLZ77(ucpBuffer[i]));

            // Advance the window by 1 byte (Literal case).
            i++;
        }
    }

    // --- 4. HANDLE REMAINING BYTES ---
    // The loop ended at bytesRead - 2. Handle the last 1 or 2 bytes as literals.
    for (; i < (int) bytesRead; i++) {
        //printf("Literal: %c\n", ucpBuffer[i]);
        appendToken(output_ucpBuffer, createLiteralLZ77(ucpBuffer[i]));
    }
}

/**
 * @brief Reset uint16_t array
 *
 * This function takes in a pointer to an array, and its size, and sets 0 for each value.
 *
 * @param array The pointer to the uint16_t array.
 * @param size The size of the array.
 *
 * @returns void
 *
 * Maximum memory required:
 *  - 32bit systems: 12 bytes
 *  - 64bit systems: 24 bytes
 */
static void resetUint16_tArray(uint16_t *array, const size_t size) {
    for (int i = 0; i < size; i++) array[i] = 0;
}

/**
 * @brief Calculate HLIT
 *
 * This function calculates the highest literal in use. Which is crucial determining how many literal/length code is in use
 * for the whole block. It takes in an uint16_t array which holds the frequencies of the literal/length codes and returns
 * the first index from the back which is not 0. To store this effectively on return it subtracts 257 from the index. Since
 * the END OF BLOCK, which is 256 that will be the highest index in the worst case.
 *
 * @param LLFrequiency The frequency table.
 *
 * @returns BYTE
 *
 * Maximum memory required:
 *  - 32bit systems: 10 bytes
 *  - 64bit systems: 18 bytes
 */
static BYTE calculateHLIT(const uint16_t *LLFrequiency) {
    int highestUsedIndex = 256; // Minimum valid is EOB (256)

    // Scan down from 285 to 257.
    for (int i = LITERAL_LENGTH_SIZE - 1; i > 256; i--) {
        if (LLFrequiency[i] > 0) {
            highestUsedIndex = i;
            break;
        }
    }

    // FIX: Formula is (Count - 257). Count is (Index + 1).
    // So HLIT = (Index + 1) - 257 = Index - 256.
    return (BYTE) (highestUsedIndex - 256);
}

/**
 * @brief Calculate HDIST
 *
 * This function calculates the highest distance code in use. Since there are cases where not all distance codes are in use
 * we save bytes from the output with not writing out unnecessary distance codes to the file. Works just like the HLIT function.
 *
 * @param distanceCodeFrequency The distanceCode table.
 * @return BYTE
 *
 * Maximum memory required:
 *  - 32bit systems: 10 bytes
 *  - 64bit systems: 18 bytes
 */
static BYTE calculateHDIST(const uint16_t *distanceCodeFrequency) {
    int highestUsedIndex = 0;
    for (int i = DISTANCE_CODE_SIZE - 1; i >= 0; i--) {
        if (distanceCodeFrequency[i] > 0) {
            highestUsedIndex = i;
            break;
        }
    }
    return (BYTE) highestUsedIndex;
}


/**
 * @brief Calculate HCLEN
 *
 * This function calculates the highest code length in use. This number could be from 1 to 15.
 * Each code read from a huffman tree has a code length which is essentially a number from the
 * top node to the leaf. In the codeLengthFrequency array we store the frequency of each code length. And here
 * we determine which is the 'biggest' one in use.
 *
 * @param codeLengthFrequency The code lengths table.
 *
 * @returns BYTE
 *
 * Maximum memory required:
 *  - 32bit systems: 10 bytes
 *  - 64bit systems: 18 bytes
 */
static BYTE calculateHCLEN(const uint16_t *codeLengthFrequency) {
    // The permutation order defined by RFC 1951
    const BYTE cl_order[19] = { 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15 };

    // Scan the order BACKWARDS. Find the last index that is actually used.
    int highestPermutationIndex = 0;

    for (int i = 18; i >= 0; i--) {
        BYTE symbol = cl_order[i];
        if (codeLengthFrequency[symbol] > 0) {
            highestPermutationIndex = i;
            break;
        }
    }

    // HCLEN = (Count - 4). Count is (Index + 1).
    // HCLEN = (Index + 1) - 4 = Index - 3.
    // Minimum count is 4 (HCLEN = 0), so we clamp the result.
    if (highestPermutationIndex < 3) return 0;

    return (BYTE)(highestPermutationIndex - 3);
}

/**
 * @brief Count Frequencies
 *
 * This function is accountable for counting the LZ77_buffer's match/literals' frequency. For the distance codes, it uses
 * the distanceCodeFrequency table as an output, and for the literal/length codes the LLFrequency.
 *
 * @param output_ucpBuffer LZ77_buffer table which stores the match/literals of a block.
 * @param LLFrequency The output table for literal/length codes.
 * @param distanceCodeFrequency The output table for distance codes.
 *
 * @returns void
 *
 * Maximum memory required:
 *  - 32bit systems: 24 bytes
 *  - 64bit systems: 44 bytes
 */
static void countFrequencies(const LZ77_buffer *output_ucpBuffer, uint16_t *LLFrequency,
                             uint16_t *distanceCodeFrequency) {
    for (int i = 0; i < output_ucpBuffer->size; i++) {
        LZ77_compressed token = output_ucpBuffer->tokens[i];
        if (token.type == LITERAL) {
            LLFrequency[token.data.literal]++;
        } else {
            distanceCodeFrequency[getDistanceCode(token.data.match.distance).usSymbolID]++;
            LLFrequency[getLengthCode(token.data.match.length).usSymbolID]++;
        }
    }
    LLFrequency[END_OF_BLOCK]++;
}

/**
 * @brief Create Literal Tree
 *
 * This function initializes, fills and then builds up a MinHeap which helps in the process of generating a huffman tree, by
 * having the lowest frequency element at the top of the tree every time.
 *
 * @param LLFrequency The frequency table for literal/length codes.
 *
 * @returns MinHeap* Must be freed afterward.
 *
 * Maximum memory required:
 *  - 32bit systems: 16 bytes
 *  - 64bit systems: 32 bytes
 */
static MinHeap *createLiteralTree(const uint16_t *LLFrequency) {
    MinHeap *literalTree = createMinHeap(LITERAL_LENGTH_SIZE);
    for (int i = 0; i < LITERAL_LENGTH_SIZE; i++) {
        if (LLFrequency[i] == 0) continue;
        addToMinHeap(literalTree, createNode(i, LLFrequency[i]));
    }
    buildMinHeap(literalTree);
    return literalTree;
}

/**
 * @brief Create Distance Tree
 *
 * This function initializes, fills and then builds up a MinHeap which helps in the process of generating a huffman tree, by
 * having the lowest frequency element at the top of the tree every time.
 *
 * @param distanceCodeFrequency The distance code frequency table.
 *
 * @returns MinHeap* Must be freed afterward.
 *
 * Maximum memory required:
 *  - 32bit systems: 24 bytes
 *  - 64bit systems: 44 bytes
 */
static MinHeap *createDistanceTree(const uint16_t *distanceCodeFrequency) {
    MinHeap *distanceTree = createMinHeap(DISTANCE_CODE_SIZE);
    for (int i = 0; i < DISTANCE_CODE_SIZE; i++) {
        if (distanceCodeFrequency[i] == 0) continue;
        addToMinHeap(distanceTree, createNode(i, distanceCodeFrequency[i]));
    }
    buildMinHeap(distanceTree);
    return distanceTree;
}

/**
 * @brief Create Code Length Tree
 *
 * This function initializes, fills and then builds up a MinHeap which helps in the process of generating a huffman tree, by
 * having the lowest frequency element at the top of the tree every time.
 * @param codeLengthFrequencies The code length frequency table.
 *
 * @returns MinHeap* Must be freed afterward.
 *
 * Maximum memory required:
 *  - 32bit systems: 24 bytes
 *  - 64bit systems: 44 bytes
 */
static MinHeap *createCodeLengthTree(const uint16_t *codeLengthFrequencies) {
    MinHeap *codeLengthTree = createMinHeap(DISTANCE_CODE_SIZE);
    for (int i = 0; i < CODE_LENGTH_FREQUENCIES; i++) {
        if (codeLengthFrequencies[i] == 0) continue;
        addToMinHeap(codeLengthTree, createNode(i, codeLengthFrequencies[i]));
    }
    buildMinHeap(codeLengthTree);
    return codeLengthTree;
}

//Flag for debug pourposes only.
static bool flag = true;


static void generateCanonicalCodes(const uint8_t *lengths, int size, HUFFMAN_CODE *table) {
    uint16_t bl_count[16] = {0};
    uint16_t next_code[16] = {0};

    for (int i = 0; i < size; i++) {
        if (lengths[i] > 0) {
            bl_count[lengths[i]]++;
        }
    }

    uint16_t code = 0;
    bl_count[0] = 0;
    for (int bits = 1; bits <= 15; bits++) {
        code = (code + bl_count[bits - 1]) << 1;
        next_code[bits] = code;
    }

    for (int i = 0; i < size; i++) {
        int len = lengths[i];
        if (len > 0) {
            table[i].code = next_code[len];
            table[i].length = len;
            next_code[len]++;
        } else {
            table[i].code = 0;
            table[i].length = 0;
        }
    }
}

/**
 * @brief Process Block
 *
 * This function takes the frequencies of both the literal/length code and the distance code frequencies, with also their
 * exact position in the buffer.
 *  - First, it constructs 2 huffman trees. One for the literal/length code frequencies and one for the distance codes.
 *  - After that it constructs a third tree which encodes the first 2 huffman trees' code length frequencies (how deep each leaf is from the top),
 *  and then writes itself into the file.
 *  - And finally, assign an encoded code for each byte and then write it to a file bit by bit for the entirety of the block.
 *
 * @param bw BIT_WRITER for writing bits to a file.
 * @param LLFrequency Literal/length code frequency table.
 * @param distanceCodeFrequency Distance code frequency table.
 * @param output_ucpBuffer The LZ77_buffer containing the match/literal objects.
 * @param lastBlock The flag for the last block.
 *
 * @returns void
 *
 * Maximum memory required:
 *  - 32bit systems: 1000 bytes
 *  - 64bit systems: 2000 bytes
 */
extern void processBlock(BIT_WRITER *bw, uint16_t *LLFrequency, uint16_t *distanceCodeFrequency,
                         const LZ77_buffer *output_ucpBuffer, const bool lastBlock) {
    resetUint16_tArray(LLFrequency,LITERAL_LENGTH_SIZE);
    resetUint16_tArray(distanceCodeFrequency,DISTANCE_CODE_SIZE);
    for (int i = 0; i < LITERAL_LENGTH_SIZE; i++) {
        //printf("LLFrequency at index %d is %d\n", i, LLFrequency[i]);
    }
    countFrequencies(output_ucpBuffer, LLFrequency, distanceCodeFrequency);
    /*
     * Create 2 MinHeaps to make the huffman tree build possible
     */
    MinHeap *literalTree = createLiteralTree(LLFrequency);
    MinHeap *distanceTree = createDistanceTree(distanceCodeFrequency);

    Node *literalTop = buildHuffmanTree(literalTree);
    Node *distanceTop = buildHuffmanTree(distanceTree);

    const BYTE highestLiteralInUse = calculateHLIT(LLFrequency);
    const BYTE highestDistanceCodeInUse = calculateHDIST(distanceCodeFrequency);

    //Find how deap a leaf is in a tree
    //for this we use an BYTE array
    BYTE ll_lengths[LITERAL_LENGTH_SIZE] = {0};
    BYTE distance_lengths[DISTANCE_CODE_SIZE] = {0};
    size_t total_lengths = (highestLiteralInUse + 257) + (highestDistanceCodeInUse + 1);

    BYTE combinedLL_Distance_lengths[total_lengths];

    //now we go through the tree using the function in node.h
    //and find the deepness of each leaf
    findCodeLengthsInTree(literalTop, ll_lengths, 0);
    findCodeLengthsInTree(distanceTop, distance_lengths, 0);

    flattenTree(ll_lengths, LITERAL_LENGTH_SIZE, 15);
    flattenTree(distance_lengths, DISTANCE_CODE_SIZE, 15);


    //combine the two results into one array
    memcpy(combinedLL_Distance_lengths, ll_lengths, highestLiteralInUse + 257);
    memcpy(combinedLL_Distance_lengths + highestLiteralInUse + 257, distance_lengths, highestDistanceCodeInUse + 1);

    //now call the compress code lenghts function on this combined distance and literal/lenghts pair
    BYTE compressed_ll_dist_lengths[total_lengths + 20];
    BYTE extra_bits_values[total_lengths + 20];
    uint16_t code_length_frequencies[CODE_LENGTH_FREQUENCIES] = {0};
    size_t compressed_symbol_count = 0;

    compressCodeLengths(combinedLL_Distance_lengths, total_lengths, compressed_ll_dist_lengths, code_length_frequencies,
                        extra_bits_values, &compressed_symbol_count);

    MinHeap *codeLengthMinHeap = createCodeLengthTree(code_length_frequencies);
    Node *clTop = buildHuffmanTree(codeLengthMinHeap);
    //if (flag) print_tree_visual(clTop,0,"");
    BYTE highestCodeLengthInUse = calculateHCLEN(code_length_frequencies);

    for (int i = 0; i < LITERAL_LENGTH_SIZE; i++) {
        //printf("Length for symbol %c, is %d \n",(char)i,ll_lengths[i]);
    }

    for (int i = 0; i < CODE_LENGTH_FREQUENCIES; i++) {
        //printf("Code Length frequency for index %d %d \n",i,code_length_frequencies[i]);
    }

    /// BTYPE = 10 (dinamikus Huffman) - LSB-től MSB felé: B_FINAL (1 bit) + BTYPE (2 bit)
    uint8_t header = (0b10 << 1) | (lastBlock ? 0b1 : 0b0);
    addBits(bw, header, 3);

    //HLIT 5bit
    addBits(bw, highestLiteralInUse, 5);
    //printf("highest literal in use: %d\n",highestLiteralInUse);
    //HDIST 5bit
    addBits(bw, highestDistanceCodeInUse, 5);
    //printf("highest distance code in use: %d\n",highestDistanceCodeInUse);
    //HCLEN 4bit
    addBits(bw, highestCodeLengthInUse, 4);
    //printf("highest code length in use: %d\n",highestCodeLengthInUse);

    BYTE cl_lengths[CODE_LENGTH_FREQUENCIES] = {0};
    findCodeLengthsInTree(clTop, cl_lengths, 0);
    /*if (flag) {
        for (int i = 0; i < CODE_LENGTH_FREQUENCIES; i++) {
            printf("Index: %d, value: %d\n", i, cl_lengths[i]);
        }
    }*/

    flattenTree(cl_lengths, 19, 7);

    /*if (flag) {
        for (int i = 0; i < CODE_LENGTH_FREQUENCIES; i++) {
            printf("Index: %d, value: %d\n", i, cl_lengths[i]);
        }
    }*/

    // 2. Write the Code Lengths of the Code Lengths (Meta-Tree Definition)
    const BYTE cl_order[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
    BYTE hclen_value = highestCodeLengthInUse + 4; // This is the total count of symbols (4-19)
    if (flag) printf("HCLEN: %d\n", hclen_value);
    for (int i = 0; i < hclen_value; ++i) {
        // Each length is 3 bits
        BYTE symbol = cl_order[i];
        /*if (flag) {
            printf("Byte before: %d, with a start index of: %d, and a current position of: %d\n", bw->byte, bw->index,bw->currentPosition);
        }*/
        addBits(bw, cl_lengths[symbol], 3);
        if (flag) { printf("%d %d\n", symbol, cl_lengths[symbol]); }
        /*if (flag) {
            printf(" Byte after: %d, with a start index of: %d, and a current position of: %d\n\n",bw->byte, bw->index, bw->currentPosition);
        }*/
    }
    if (flag) printf("\n\n");
    HUFFMAN_CODE ll_table[LITERAL_LENGTH_SIZE] = {0};
    HUFFMAN_CODE distance_table[DISTANCE_CODE_SIZE] = {0};
    HUFFMAN_CODE cl_table[CODE_LENGTH_FREQUENCIES] = {0};

    generateCanonicalCodes(ll_lengths, LITERAL_LENGTH_SIZE, ll_table);

    generateCanonicalCodes(distance_lengths, DISTANCE_CODE_SIZE, distance_table);

    /*if (flag) {
        for (int i= 0; i< DISTANCE_CODE_SIZE; i++) {
            printf("length: %d code:%x \n",distance_table[i].length,distance_table[i].code);
        }
    }*/
    flag = true;

    generateCanonicalCodes(cl_lengths, CODE_LENGTH_FREQUENCIES, cl_table);

    //printf("compressed_symbol_count: %llu, - - %llu\n",compressed_symbol_count, total_lengths);
    for (int i = 0; i < compressed_symbol_count; i++) {
        BYTE symbol = compressed_ll_dist_lengths[i];
        HUFFMAN_CODE hCode = cl_table[symbol];

        // CHANGE: This is a Huffman Code -> use writeHuffmanCode
        writeHuffmanCode(bw, hCode.code, hCode.length);

        // KEEP: These are "extra bits" (values), not codes -> keep addBits
        if (symbol == 16) {
            addBits(bw, extra_bits_values[i], 2);
        } else if (symbol == 17) {
            addBits(bw, extra_bits_values[i], 3);
        } else if (symbol == 18) {
            addBits(bw, extra_bits_values[i], 7);
        }
    }

    // 2. Writing the Compressed Data (Literals and Matches)
    for (int i = 0; i < output_ucpBuffer->size; i++) {
        LZ77_compressed token = output_ucpBuffer->tokens[i];

        if (token.type == LITERAL) {
            HUFFMAN_CODE hc = ll_table[token.data.literal];

            // CHANGE: Huffman Code -> use writeHuffmanCode
            writeHuffmanCode(bw, hc.code, hc.length);
        } else {
            // --- Length ---
            LENGTH_CODE lc = getLengthCode(token.data.match.length);
            HUFFMAN_CODE lengthCode = ll_table[lc.usSymbolID];

            // CHANGE: Huffman Code for the length symbol (257-285)
            writeHuffmanCode(bw, lengthCode.code, lengthCode.length);

            // KEEP: Extra bits for length (e.g. +3 bits to say length is 15)
            if (lc.iExtraBits > 0) {
                addBits(bw, lc.iExtraValue, lc.iExtraBits);
            }

            // --- Distance ---
            DISTANCE_CODE dc = getDistanceCode(token.data.match.distance);
            HUFFMAN_CODE distanceCode = distance_table[dc.usSymbolID];

            // CHANGE: Huffman Code for the distance symbol (0-29)
            writeHuffmanCode(bw, distanceCode.code, distanceCode.length);

            // KEEP: Extra bits for distance
            if (dc.iExtraBits > 0) {
                addBits(bw, dc.iExtraValue, dc.iExtraBits);
            }
        }
    }

    // 3. End of Block
    const HUFFMAN_CODE EOB = ll_table[END_OF_BLOCK]; // Symbol 256

    // CHANGE: EOB is a Huffman Code
    writeHuffmanCode(bw, EOB.code, EOB.length);
    if (lastBlock) {
        flushBitstreamWriter(bw);
    }
    //printf("End of block. EOB code: %d, EOB length: %d, bw possition: %d\n",EOB.code, EOB.length, bw->currentPosition);
    //Free up memory pointers
    freeTree(clTop);
    freeTree(distanceTop);
    freeTree(literalTop);

    freeMinHeap(literalTree);
    freeMinHeap(distanceTree);
    freeMinHeap(codeLengthMinHeap);
}


/**
 * @brief Compress
 *
 * This is the entry for the whole compression. In this function we initialize the core utilities and start reading the file
 * buffer to buffer.
 *
 * @param filename The file which we want to compress.
 * @returns STATUS object
 *
 * Maximum memory required:
 *  - 32bit systems: 24 bytes
 *  - 64bit systems: 44 bytes
 */
extern STATUS *compress(char *filename) {
    STATUS *status = initSTATUS();
    status->code = COMPRESSION_SUCCESS;
    createSTATUSMessage(status, "File compression succeeded!");

    FILE *file = ffOpenFile(filename);
    if (file == NULL) {
        status->code = CANT_OPEN_FILE;
        createSTATUSMessage(status, "Can\'t open input file!");
        return status;
    }

    uint16_t *hashTable = initHashTable();

    if (hashTable == NULL) {
        status->code = CANT_ALLOCATE_MEMORY;
        createSTATUSMessage(status, "Can\'t allocate memory for a hash table!");
        return status;
    }

    unsigned char *ucpBuffer = cpInitBuffer();

    if (ucpBuffer == NULL) {
        status->code = CANT_ALLOCATE_MEMORY;
        createSTATUSMessage(status, "Can\'t allocate memory for a ucpBuffer!");
        free(hashTable);
        return status;
    }


    LZ77_buffer *outputBuffer = initLZ77Buffer();

    uint16_t LLFrequency[LITERAL_LENGTH_SIZE] = {0};
    uint16_t distanceCodeFrequency[DISTANCE_CODE_SIZE] = {0};

    BIT_WRITER *BIT_WRITER = initBIT_WRITER(4096);
    createFile(BIT_WRITER, filename, "gz"); // Use "gz" extension

    uint32_t crc32Checksum = 0xFFFFFFFF; // CRC32 is initialized to all ones
    uint32_t totalUncompressedSize = 0;

    size_t chunkBytesRead = 0;
    size_t current_buffer_pos = 0; // Tracks the start of the current compression window
    bool isFinalBlock = false;


    chunkBytesRead = fread(ucpBuffer, 1, WINDOW_SIZE, file);

    // Loop continues as long as we read any data.
    do {
        // Prepare the next LZ77 compression step
        size_t bytesToCompress = chunkBytesRead;

        if (bytesToCompress > 0) {
            crc32Checksum = calculate_crc32(
                crc32Checksum,
                ucpBuffer + current_buffer_pos,
                bytesToCompress
            );
            totalUncompressedSize += (uint32_t) bytesToCompress;
        }


        compressData(
            ucpBuffer + current_buffer_pos,
            bytesToCompress,
            hashTable,
            outputBuffer
        );


        isFinalBlock = (chunkBytesRead < WINDOW_SIZE) || feof(file);

        processBlock(
            BIT_WRITER,
            LLFrequency,
            distanceCodeFrequency,
            outputBuffer,
            isFinalBlock
        );

        freeLZ77Buffer(outputBuffer);
        outputBuffer = initLZ77Buffer();

        // Shift the window: The new window starts where the compressed data ended.
        // We only need to keep the history (the previous WINDOW_SIZE) in ucpBuffer.
        // In this setup, we compress the entire current window and then slide it.

        if (!isFinalBlock) {
            memmove(ucpBuffer, ucpBuffer + WINDOW_SIZE, WINDOW_SIZE);
            vfSubtractWindowSizeFromHashTable(hashTable);

            chunkBytesRead = fread(ucpBuffer + WINDOW_SIZE, 1, WINDOW_SIZE, file);

            current_buffer_pos = WINDOW_SIZE;
        } else {
            chunkBytesRead = 0;
        }
    } while (chunkBytesRead > 0);

    crc32Checksum = crc32Checksum ^ 0xFFFFFFFF;
    printf("%d\n", (int) (crc32Checksum >> 0) & 0x000000FF);
    printf("%d\n", (int) (crc32Checksum >> 8) & 0x000000FF);
    printf("%d\n", (int) (crc32Checksum >> 16) & 0x000000FF);
    printf("%d\n", (int) (crc32Checksum >> 24) & 0x000000FF);

    addBytes(BIT_WRITER, crc32Checksum, 4);
    addBytes(BIT_WRITER, totalUncompressedSize, 4);
    printf("%d\n", (int) (totalUncompressedSize >> 0) & 0x000000FF);
    printf("%d\n", (int) (totalUncompressedSize >> 8) & 0x000000FF);
    printf("%d\n", (int) (totalUncompressedSize >> 16) & 0x000000FF);
    printf("%d\n", (int) (totalUncompressedSize >> 24) & 0x000000FF);

    fclose(file);

    free(hashTable);
    free(ucpBuffer);
    freeLZ77Buffer(outputBuffer);
    printf("Done\n");
    freeBIT_WRITER(BIT_WRITER);
    return status;
}
