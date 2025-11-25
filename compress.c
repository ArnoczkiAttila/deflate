//
// Created by Rendszergazda on 11/19/2025.
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
#include "status.h"

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

/**
 * @brief Subtract Window Size From Hash Table
 *
 * When a shift occurs in the buffer (when the buffer's second half gets copied to the first half)
 * we need to subtract the WINDOW_SIZE from all valid entries in the hashTable so that the newly
 * copied data can have a history from the previously processed data.
 *
 * @param uiarrHashTable The hashTable which stores the last occurrence of a 3 byte hash.
 * @returns void
 */
static void vfSubtractWindowSizeFromHashTable(uint16_t* uiarrHashTable) {
    for (int i = 0; i<HASH_SIZE; i++) {
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
 * @returns uint16_t Hash key.
 */
static uint16_t uifGenerateHashKey(const unsigned char* p) {
    return (uint16_t) ((p[0] << HASH_SHIFT) ^ p[1] ^ p[2]) & HASH_MASK;
}

/**
 * @brief Init Buffer only allocates BUFFER_SIZE byte unsigned char and returns with its pointer.
 * If the memory allocation fails then it returns with a NULL pointer.
 * @returns unsigned char* The freshly allocated BUFFER_SIZE sized buffer.
 */
static unsigned char* cpInitBuffer(void) {
    unsigned char* ucpBuffer = (unsigned char*) malloc(BUFFER_SIZE+WINDOW_SIZE);
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
 * @return int The length of the match in bytes
 */
static int iFindMatchLength(const unsigned char* ucpCurrent, const unsigned char *ucpOld, const unsigned char *ucpInputEndPointer) {

    ptrdiff_t pdiffAvailableBytes = ucpInputEndPointer - ucpCurrent;

    int iMaxCheckLength = 258;
    if (pdiffAvailableBytes < iMaxCheckLength) {
        iMaxCheckLength = (int)pdiffAvailableBytes;
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
 * @returns void
 */
static void* fvpResetHashTable(uint16_t *uiarrHashTable) {
    return memset(uiarrHashTable, EMPTY_INDEX, 2 * HASH_SIZE);
}


/**
 * @brief initHashTable returns with an allocated hashTable. Required space in memory is 2*HASH_SIZE.
 * Must be freed afterward.
 * @returns uint16_t* The pointer to the hashTable.
 */
static uint16_t* initHashTable(void) {
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
 * @param filename The name of the file. (probably with absolute path)
 * @return FILE* or NULL
 */
extern FILE* ffOpenFile(const char* filename) {
    FILE* fpFile = fopen(filename, "rb");
    return fpFile;
}


void compress_data(const unsigned char *ucpBuffer, size_t bytes_read, uint16_t* hash_table, LZ77_buffer* output_ucpBuffer) {

    // --- Set the End Pointer ---
    const unsigned char *ucpInputEndPointer = ucpBuffer + bytes_read;

    // --- Loop Control ---
    // Loop only up to (bytes_read - 2) because we need at least 3 bytes to hash/match.
    int i;
    for (i = 0; i < (int)bytes_read - 2; ) {
        uint16_t hashKey = uifGenerateHashKey(ucpBuffer + i);
        uint16_t hashIndex = hash_table[hashKey];

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

        hash_table[hashKey] = (uint16_t)i;

        if (bestLength >= 3) {
            // Output the match token

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
    // The loop ended at bytes_read - 2. Handle the last 1 or 2 bytes as literals.
    for (; i < (int)bytes_read; i++) {
        //printf("Literal: %c\n", ucpBuffer[i]);
        appendToken(output_ucpBuffer, createLiteralLZ77(ucpBuffer[i]));
    }
}

static void resetUint16_tArray(uint16_t* array, const size_t size) {
    for (int i = 0; i<size; i++) array[i] = 0;
}

static uint8_t calculateHLIT(const uint16_t* LLFrequiency) {
    int highestUsedIndex = 256;
    for (int i = LITERAL_LENGTH_SIZE-1; i>255;i--) {
        if (LLFrequiency[i] > 0) {
            highestUsedIndex = i;
            break;
        }
    }
    return (uint8_t)(highestUsedIndex - 257);
}

static uint8_t calculateHDIST(const uint16_t* distanceCodeFrequency) {
    int highestUsedIndex = 0;
    for (int i = DISTANCE_CODE_SIZE-1; i>=0;i--) {
        if (distanceCodeFrequency[i] > 0) {
            highestUsedIndex = i;
            break;
        }
    }
    return (uint8_t) highestUsedIndex;
}

static uint8_t calculateHCLEN(const uint16_t* codeLengthFrequency) {
    int highestUsedIndex = 0;
    for (int i = CODE_LENGTH_FREQUENCIES-1; i>=0;i--) {
        if (codeLengthFrequency[i] > 0) {
            highestUsedIndex = i;
            break;
        }
    }
    return (uint8_t) highestUsedIndex - 4;
}

static void countFrequencies(const LZ77_buffer* output_ucpBuffer, uint16_t* LLFrequency, uint16_t* distanceCodeFrequency) {
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
static MinHeap* createLiteralTree(const uint16_t* LLFrequency) {
    MinHeap* literalTree = createMinHeap(LITERAL_LENGTH_SIZE);
    for (int i = 0; i < LITERAL_LENGTH_SIZE; i++) {
        if (LLFrequency[i] == 0) continue;
        addToMinHeap(literalTree, createNode(i,LLFrequency[i]));
    }
    buildMinHeap(literalTree);
    return literalTree;
}

static MinHeap* createDistanceTree(const uint16_t* distanceCodeFrequency) {
    MinHeap* distanceTree = createMinHeap(DISTANCE_CODE_SIZE);
    for (int i = 0; i < DISTANCE_CODE_SIZE; i++) {
        if (distanceCodeFrequency[i] == 0) continue;
        addToMinHeap(distanceTree, createNode(i,distanceCodeFrequency[i]));
    }
    buildMinHeap(distanceTree);
    return distanceTree;
}

static MinHeap* createCodeLengthTree(const uint16_t* codeLengthFrequencies) {
    MinHeap* codeLengthTree = createMinHeap(DISTANCE_CODE_SIZE);
    for (int i = 0; i < CODE_LENGTH_FREQUENCIES; i++) {
        if (codeLengthFrequencies[i] == 0) continue;
        addToMinHeap(codeLengthTree, createNode(i,codeLengthFrequencies[i]));
    }
    buildMinHeap(codeLengthTree);
    return codeLengthTree;
}

extern void writeGzipTrailer(BitWriter* bw, uint32_t crc32_checksum, uint32_t total_uncompressed_size) {

    // 1. Finalize CRC32: In standard implementation, the result needs to be XORed with 0xFFFFFFFF
    uint32_t final_crc32 = crc32_checksum ^ 0xFFFFFFFF;

    // 2. Flush DEFLATE bits to the next byte boundary
    // This function pads the last compressed byte with zeros to align the stream.
    flush_bitstream_writer(bw);

    // 3. Write CRC32 (4 bytes, LSB first)
    // The write_uint32_lsb function ensures the correct Little-Endian byte order.
    addBytesFromMSB(bw, final_crc32,4);

    // 4. Write ISIZE (4 bytes, LSB first)
    addBytesFromMSB(bw, total_uncompressed_size,4);

    // The stream is now complete, ready for closeBitWriter() which handles the final buffer flush to disk.
}
static bool flag = true;

extern void processBlock(BitWriter* bw, uint16_t* LLFrequency, uint16_t* distanceCodeFrequency, const LZ77_buffer* output_ucpBuffer, const bool lastBlock) {
    resetUint16_tArray(LLFrequency,LITERAL_LENGTH_SIZE);
    resetUint16_tArray(distanceCodeFrequency,DISTANCE_CODE_SIZE);
    for (int i = 0; i< LITERAL_LENGTH_SIZE;i++) {
        //printf("LLFrequency at index %d is %d\n", i, LLFrequency[i]);
    }
    countFrequencies(output_ucpBuffer,LLFrequency,distanceCodeFrequency);
    /*
     * Create 2 MinHeaps to make the huffman tree build possible
     */
    MinHeap* literalTree = createLiteralTree(LLFrequency);
    MinHeap* distanceTree = createDistanceTree(distanceCodeFrequency);

    Node* literalTop = buildHuffmanTree(literalTree);
    Node* distanceTop = buildHuffmanTree(distanceTree);

    const uint8_t highestLiteralInUse = calculateHLIT(LLFrequency);
    const uint8_t highestDistanceCodeInUse = calculateHDIST(distanceCodeFrequency);

    //Find how deap a leaf is in a tree
    //for this we use an uint8_t array
    uint8_t ll_lengths[LITERAL_LENGTH_SIZE] = {0};
    uint8_t distance_lengths[DISTANCE_CODE_SIZE] = {0};
    size_t total_lengths = (highestLiteralInUse + 257) + (highestDistanceCodeInUse + 1);

    uint8_t combinedLL_Distance_lengths[total_lengths];

    //now we go through the tree using the function in node.h
    //and find the deepness of each leaf
    findCodeLengthsInTree(literalTop,ll_lengths,0);
    findCodeLengthsInTree(distanceTop,distance_lengths,0);

    //combine the two results into one array
    memcpy(combinedLL_Distance_lengths,ll_lengths,highestLiteralInUse+257);
    memcpy(combinedLL_Distance_lengths+highestLiteralInUse+257,distance_lengths,highestDistanceCodeInUse+1);

    //now call the compress code lenghts function on this combined distance and literal/lenghts pair
    uint8_t compressed_ll_dist_lengths[total_lengths+20];
    uint8_t extra_bits_values[total_lengths+20];
    uint16_t code_length_frequencies[CODE_LENGTH_FREQUENCIES] = {0};
    size_t compressed_symbol_count = 0;
    compressCodeLengths(combinedLL_Distance_lengths,total_lengths,compressed_ll_dist_lengths,code_length_frequencies,extra_bits_values,&compressed_symbol_count);

    MinHeap* codeLengthMinHeap = createCodeLengthTree(code_length_frequencies);
    Node* clTop = buildHuffmanTree(codeLengthMinHeap);
    uint8_t highestCodeLengthInUse = calculateHCLEN(code_length_frequencies);

    for (int i = 0; i < LITERAL_LENGTH_SIZE; i++) {
        //printf("Length for symbol %c, is %d \n",(char)i,ll_lengths[i]);
    }

    for (int i = 0; i < CODE_LENGTH_FREQUENCIES; i++) {
        //printf("Code Length frequency for index %d %d \n",i,code_length_frequencies[i]);
    }

    //Write to file
    if (lastBlock) {
        addData(bw, 0b01,1);
    } else {
        addData(bw,0b0,1); //not the last block
    }
    addData(bw,0b10,2); //dynamic huffman

    //HLIT 5bit
    addData(bw, highestLiteralInUse,5);
    //printf("highest literal in use: %d\n",highestLiteralInUse);
    //HDIST 5bit
    addData(bw, highestDistanceCodeInUse,5);
    //printf("highest distance code in use: %d\n",highestDistanceCodeInUse);
    //HCLEN 4bit
    addData(bw, highestCodeLengthInUse,4);
    //printf("highest code length in use: %d\n",highestCodeLengthInUse);

    uint8_t cl_lengths[CODE_LENGTH_FREQUENCIES] = {0};
    findCodeLengthsInTree(clTop, cl_lengths, 0);

    // 2. Write the Code Lengths of the Code Lengths (Meta-Tree Definition)
    const uint8_t cl_order[19] = { 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15 };
    uint8_t hclen_value = highestCodeLengthInUse + 4; // This is the total count of symbols (4-19)

    for (int i = 0; i < hclen_value; ++i) {
        // Each length is 3 bits
        uint8_t symbol = cl_order[i];
        addData(bw, cl_lengths[symbol], 3);
        if (flag) {printf("%d %d\n",symbol,cl_lengths[symbol]);}

    }
    flag = false;
    printf("\n\n");

    HUFFMAN_CODE ll_table[LITERAL_LENGTH_SIZE] = {0};
    HUFFMAN_CODE distance_table[DISTANCE_CODE_SIZE] = {0};
    HUFFMAN_CODE cl_table[CODE_LENGTH_FREQUENCIES] = {0};

    buildCodeLookupTable(literalTop, ll_table,0,0);
    buildCodeLookupTable(distanceTop, distance_table,0,0);
    buildCodeLookupTable(clTop, cl_table,0,0);


    //printf("compressed_symbol_count: %llu, - - %llu\n",compressed_symbol_count, total_lengths);
    for (int i = 0; i < compressed_symbol_count; i++) {
        uint8_t symbol = compressed_ll_dist_lengths[i];
        HUFFMAN_CODE hCode = cl_table[symbol];

        // 1. Write the Huffman Code for the RLE symbol
        addData(bw, hCode.code, hCode.length);

        // 2. Write the Extra Bits if the symbol requires them
        if (symbol == 16) {
            // Repeat previous length 3-6 times (2 extra bits)
            addData(bw, extra_bits_values[i], 2);
        } else if (symbol == 17) {
            // Repeat 0 length 3-10 times (3 extra bits)
            addData(bw, extra_bits_values[i], 3);
        } else if (symbol == 18) {
            // Repeat 0 length 11-138 times (7 extra bits)
            addData(bw, extra_bits_values[i], 7);
        }
    }

    // @todo make the LENGTH_CODE table first and then the lookup should be much easier and faster
    for (int i = 0; i < output_ucpBuffer->size; i++) {
        LZ77_compressed token = output_ucpBuffer->tokens[i];
        if (token.type == LITERAL) {
            HUFFMAN_CODE hc = ll_table[token.data.literal];
            addData(bw,hc.code,hc.length);
        } else {
            LENGTH_CODE lc = getLengthCode(token.data.match.length);
            // Use the symbol ID for lookup, NOT the raw length
            HUFFMAN_CODE lengthCode = ll_table[lc.usSymbolID];

            // Write the code and extra bits
            addData(bw,lengthCode.code,lengthCode.length);
            if (lc.iExtraBits>0) {
                addData(bw, lc.iExtraValue, lc.iExtraBits);
            }

            // Look up the distance code data
            DISTANCE_CODE dc = getDistanceCode(token.data.match.distance);
            // Use the symbol ID for lookup, NOT the raw distance
            HUFFMAN_CODE distanceCode = distance_table[dc.usSymbolID];

            // Write the code and extra bits
            addData(bw,distanceCode.code, distanceCode.length);
            if (dc.iExtraBits> 0) {
                addData(bw,dc.iExtraValue, dc.iExtraBits);
            }
        }
    }
    const HUFFMAN_CODE EOB = ll_table[END_OF_BLOCK];
    addData(bw, EOB.code, EOB.length);

    //Free up memory pointers
    freeTree(clTop);
    freeTree(distanceTop);
    freeTree(literalTop);

    freeMinHeap(literalTree);
    freeMinHeap(distanceTree);
    freeMinHeap(codeLengthMinHeap);
}

extern Status compress(char* filename) {
    Status status = {.code = COMPRESSION_SUCCESS,.message = "File compression succeeded!"};

    FILE* file = ffOpenFile(filename);
    if (file==NULL) {
        status.code = CANT_OPEN_FILE;
        status.message = "Can\'t open input file!";
        return status;
    }

    uint16_t* hashTable = initHashTable();

    if (hashTable == NULL) {
        status.code = CANT_ALLOCATE_MEMORY;
        status.message = "Can\'t allocate memory for a hash table!";
        return status;
    }

    unsigned char* ucpBuffer = cpInitBuffer();

    if (ucpBuffer == NULL) {
        status.code = CANT_ALLOCATE_MEMORY;
        status.message = "Can\'t allocate memory for a ucpBuffer!";
        free(hashTable);
        return status;
    }

    // --- 1. Initialization and Setup ---

    // LZ77/Huffman structures
    LZ77_buffer* outputBuffer = initLZ77Buffer();

    // Huffman Frequency arrays
    uint16_t LLFrequency[LITERAL_LENGTH_SIZE] = {0};
    uint16_t distanceCodeFrequency[DISTANCE_CODE_SIZE] = {0};

    // Bitstream writer
    BitWriter* bitWriter = initBitWriter();
    createFile(bitWriter, filename, "gz"); // Use "gz" extension

    uint32_t crc32_checksum = 0xFFFFFFFF; // CRC32 is initialized to all ones
    uint32_t total_uncompressed_size = 0;

    // Counters and state
    size_t total_bytes_read = 0;
    size_t chunk_bytes_read = 0;
    size_t current_buffer_pos = 0; // Tracks the start of the current compression window
    bool is_final_block = false;

    // --- 2. Main Compression Loop ---

    // Load the first complete window or the entire file if small.
    // Read directly into the first part of the buffer.
    chunk_bytes_read = fread(ucpBuffer, 1, WINDOW_SIZE, file);
    total_bytes_read += chunk_bytes_read;

    // Loop continues as long as we read any data.
    do {
        // Prepare the next LZ77 compression step
        size_t bytes_to_compress = chunk_bytes_read;

        if (bytes_to_compress > 0) {
            // Update CRC32 with the data we just read
            // CRC32 calculation must be done over the UNCOMPRESSED data (ucpBuffer)
            crc32_checksum = calculate_crc32(
                crc32_checksum,
                ucpBuffer + current_buffer_pos,
                bytes_to_compress
            );
            // Update the total size
            total_uncompressed_size += (uint32_t)bytes_to_compress;
        }

        // --- A. Perform Compression ---
        // compress_data uses the history (window) up to current_buffer_pos
        compress_data(
            ucpBuffer + current_buffer_pos,
            bytes_to_compress,
            hashTable,
            outputBuffer
        );

        // --- B. Determine Final Block Status ---
        // If the last read was less than WINDOW_SIZE, this is the last chunk of data.
        // If the last read was 0, it means the previous block was the last one.
        is_final_block = (chunk_bytes_read < WINDOW_SIZE) || feof(file);


        // --- C. Process and Write Block ---
        // This is where the Huffman trees are built and the block is written.
        processBlock(
            bitWriter,
            LLFrequency,
            distanceCodeFrequency,
            outputBuffer,
            is_final_block
        );

        // --- D. Prepare for Next Iteration ---

        // 1. Reset Buffer for next chunk (LZ77 compression resets the output buffer)
        freeLZ77Buffer(outputBuffer);
        outputBuffer = initLZ77Buffer();

        // 2. Shift the window: The new window starts where the compressed data ended.
        // We only need to keep the history (the previous WINDOW_SIZE) in ucpBuffer.
        // In this setup, we compress the entire current window and then slide it.

        // Shift the data buffer for the next read
        if (!is_final_block) {
            // Keep the last WINDOW_SIZE of data for history
            memmove(ucpBuffer, ucpBuffer + WINDOW_SIZE, WINDOW_SIZE);
            vfSubtractWindowSizeFromHashTable(hashTable);

            // Read the next WINDOW_SIZE chunk into the second half of the buffer
            chunk_bytes_read = fread(ucpBuffer + WINDOW_SIZE, 1, WINDOW_SIZE, file);
            total_bytes_read += chunk_bytes_read;

            // The effective data to compress is now at ucpBuffer + WINDOW_SIZE
            current_buffer_pos = WINDOW_SIZE;
        } else {
            // If it's the final block, stop the loop.
            chunk_bytes_read = 0;
        }


    } while (chunk_bytes_read > 0);

    crc32_checksum = crc32_checksum ^ 0xFFFFFFFF;

    addBytesFromMSB(bitWriter,crc32_checksum,4);
    addBytesFromMSB(bitWriter,total_uncompressed_size,4);

    // 5. Close the file
    fclose(file);

    free(hashTable);
    free(ucpBuffer);
    freeLZ77Buffer(outputBuffer);
    freeBitWriter(bitWriter);
    return status;
}
