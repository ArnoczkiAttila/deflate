//
// Created by Rendszergazda on 11/19/2025.
//

#include "compress.h"
#include "debugmalloc.h"

#include <stdio.h>
#include <string.h>

#include "bitwriter.h"
#include "distance.h"
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
    unsigned char* ucpBuffer = (unsigned char*) malloc(BUFFER_SIZE);
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

extern void processBlock(BitWriter* bw, uint16_t* LLFrequency, uint16_t* distanceCodeFrequency, const LZ77_buffer* output_ucpBuffer) {
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

    MinHeap* literalTree = createMinHeap(LITERAL_LENGTH_SIZE);
    MinHeap* distanceTree = createMinHeap(DISTANCE_CODE_SIZE);

    for (int i = 0; i < LITERAL_LENGTH_SIZE; i++) {
        addToMinHeap(literalTree, createNode(i,LLFrequency[i]));
    }
    buildMinHeap(literalTree);
    Node* literalTop = buildHuffmanTree(literalTree);

    for (int i = 0; i < DISTANCE_CODE_SIZE; i++) {
        addToMinHeap(distanceTree, createNode(i,distanceCodeFrequency[i]));
    }

    buildMinHeap(distanceTree);
    Node* distanceTop = buildHuffmanTree(distanceTree);

    addData(bw,0b0,1); //not the last block
    addData(bw,0b10,2); //dynamic huffman

    //HLIT 5bit
    addData(bw,calculateHLIT(LLFrequency),5);
    //HDIST 5bit
    addData(bw,calculateHDIST(distanceCodeFrequency),5);
    //HCLEN 4bit

    resetUint16_tArray(LLFrequency,LITERAL_LENGTH_SIZE);
    resetUint16_tArray(distanceCodeFrequency,DISTANCE_CODE_SIZE);
    freeMinHeap(literalTree);
    freeMinHeap(distanceTree);
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

    LZ77_buffer* outputBuffer = initLZ77Buffer();

    uint16_t LLFrequency[LITERAL_LENGTH_SIZE] = {0};
    uint16_t distanceCodeFrequency[DISTANCE_CODE_SIZE] = {0};

    BitWriter* bitWriter = initBitWriter();
    createFile(bitWriter, "teszt","gz");


    size_t records = 0;
    size_t allbytes = 0;

    size_t bytes_read =  fread(ucpBuffer, 1, BUFFER_SIZE, file);
    compress_data(ucpBuffer,bytes_read,hashTable,outputBuffer);
    allbytes+= bytes_read;
    records+=outputBuffer->size;

    processBlock(bitWriter, LLFrequency, distanceCodeFrequency, outputBuffer);

    memcpy(ucpBuffer,ucpBuffer+WINDOW_SIZE,WINDOW_SIZE);
    vfSubtractWindowSizeFromHashTable(hashTable);

    freeLZ77Buffer(outputBuffer);
    outputBuffer = initLZ77Buffer();

    while ((bytes_read = fread(ucpBuffer+WINDOW_SIZE, 1, WINDOW_SIZE, file)) > 0) {
        allbytes+= bytes_read;
        //printf("%llu\n",bytes_read);

        compress_data(ucpBuffer+WINDOW_SIZE,bytes_read,hashTable,outputBuffer);

        memcpy(ucpBuffer,ucpBuffer+WINDOW_SIZE,WINDOW_SIZE);
        vfSubtractWindowSizeFromHashTable(hashTable);

        records+=outputBuffer->size;
        freeLZ77Buffer(outputBuffer);
        outputBuffer = initLZ77Buffer();
    }
    printf("Starting size: %llu, \nEnding size: %llu, \nDifference: %llu\n",allbytes,records,allbytes-records);


    // 5. Close the file
    fclose(file);

    free(hashTable);
    free(ucpBuffer);
    freeLZ77Buffer(outputBuffer);
    freeBitWriter(bitWriter);
    return status;
}
