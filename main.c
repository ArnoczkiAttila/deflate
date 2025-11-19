#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "LZ77.h"
#include "distance.h"
#include "length.h"

#define HASH_BITS 15
#define HASH_SHIFT 5
#define HASH_MASK 0x7FFF // 32767 for 15 bits
#define HASH_SIZE (1 << HASH_BITS) // 32768
#define WINDOW_SIZE 32768
#define BUFFER_SIZE (WINDOW_SIZE * 2)
#define EMPTY_INDEX 0xFFFF



/**
 * @brief Reset Hash Table values
 *
 * The fnResetHashTable function takes an uint16_t pointer and uses the
 * predefined variables (EMPTY_INDEX and HASH_SIZE) for setting a default value
 * for the whole table.
 *
 * The default value defined in EMPTY_INDEX is 0xFFFF.
 *
 * @param uint16_t* hash_table
 * @returns void
 */
static void fnResetHashTable(uint16_t *hash_table) {
    memset(hash_table, EMPTY_INDEX, 2 * HASH_SIZE);
}

static void notHardReset(uint16_t *hash_table) {
    for (int i = 0; i<HASH_SIZE; i++) {
        if (hash_table[i] >= WINDOW_SIZE) {
            hash_table[i] -= WINDOW_SIZE;
        }
    }
}

static uint16_t *initHashTable() {
    uint16_t *hash_table = malloc(2 * HASH_SIZE);
    if (hash_table == NULL) {
        exit(-1);
    }
    fnResetHashTable(hash_table);
    return hash_table;
}


static uint16_t generateHashKey(const unsigned char *p) {
    return (uint16_t) ((p[0] << HASH_SHIFT) ^ p[1] ^ p[2]) & HASH_MASK;
}

uint16_t updateHashKey(uint16_t prev_hash, const unsigned char *p) {
    // 1. Shift the previous hash value left by HASH_SHIFT bits.
    // This pushes out the influence of the oldest byte (p[-1])
    // and prepares the space for the newest byte (p[2]).
    // 2. XOR with the new byte p[2] that just entered the window.
    // 3. Mask the result to ensure it fits in HASH_BITS.

    // p[0] is the middle byte of the new 3-byte block
    // p[1] is the last byte of the new 3-byte block
    // p[2] is the newest byte entering the block (at P+2)

    // For the hash function: hash = (b1 << SHIFT) ^ b2 ^ b3:

    // We update the hash to reflect the new sequence (p[1], p[2], p[3])
    // We need to move the old (p[0]) out and bring the new (p[3]) in.

    // For a simple combined shift/XOR hash like we used:
    // A simplified update used by some compressors is:

    unsigned int hash_val = (prev_hash << HASH_SHIFT) ^ p[2];

    return (uint16_t) (hash_val & HASH_MASK);
}

static unsigned char *initBuffer() {
    unsigned char *buffer = malloc(BUFFER_SIZE);
    if (buffer == NULL) exit(-1);
    return buffer;
}

static int findMatchLength(const unsigned char *currentPointer,
                           const unsigned char *oldPointer,
                           const unsigned char *inputEndPointer) {

    // 1. Calculate the number of bytes remaining in the input from the current position.
    // This is the absolute limit for the match length (safety boundary).
    ptrdiff_t availableBytes = inputEndPointer - currentPointer;

    // 2. Determine the maximum match length to check (258 is the DEFLATE limit).
    // The search is limited by the lesser of the DEFLATE maximum (258) and the buffer boundary.
    int maxCheckLength = 258;
    if (availableBytes < maxCheckLength) {
        maxCheckLength = (int)availableBytes;
    }

    int length = 0;

    // Loop until the length limit is reached or a mismatch is found.
    // We use length as the offset for both pointers.
    while (length < maxCheckLength) {
        // Compare the byte at the current offset for both pointers.
        if (currentPointer[length] == oldPointer[length]) {
            length++;
        } else {
            // Mismatch found
            break;
        }
    }

    return length;
}

void compress_data(const unsigned char *buffer, size_t bytes_read, uint16_t* hash_table, LZ77_buffer* output_buffer) {

    // --- Set the End Pointer ---
    const unsigned char *inputEndPointer = buffer + bytes_read;

    // --- Loop Control ---
    // Loop only up to (bytes_read - 2) because we need at least 3 bytes to hash/match.
    int i;
    for (i = 0; i < (int)bytes_read - 2; ) {

        // --- 1. HASH LOOKUP ---
        uint16_t hashKey = generateHashKey(buffer + i);
        uint16_t hashIndex = hash_table[hashKey];

        // --- 2. TRY TO FIND A MATCH ---
        int bestLength = 0;
        int bestDistance = 0;

        // Check 1: Is the hash table entry valid?
        if (hashIndex != EMPTY_INDEX) {

            // Check 2: Is the distance valid? (i - hashIndex must be positive)
            int distance = i - hashIndex;

            // Check 3: Is distance within the 32KB LZ77 window?
            if (distance > 0 && distance <= WINDOW_SIZE) {

                // Get the match length at this historical index
                bestLength = findMatchLength(buffer + i, buffer + hashIndex, inputEndPointer);
                bestDistance = distance;

                // CRITICAL FIX: Ensure the match is actually worth taking (Length >= 3)
                if (bestLength < 3) {
                    bestLength = 0; // Discard match if too short
                }
            }
        }

        // --- 3. PROCESS THE RESULT ---

        // A. Update the hash table for the sequence starting at current position 'i'.
        // This MUST happen *before* advancing the window.
        hash_table[hashKey] = (uint16_t)i;

        if (bestLength >= 3) {
            // MATCH FOUND (Length >= 3)

            // Output the match token
            //printf("Match: (Distance: %d, Length: %d)\n", bestDistance, bestLength);
            appendToken(output_buffer, createMatchLZ77(bestDistance, bestLength));
            DistanceCode dc = getDistanceCode(bestDistance);
            LengthCode lc = getLengthCode(bestLength);
            //printf("Length code: %d, required extra bits: %d, extra value: %d    -    ", lc.usSymbolID,lc.iExtraBits,lc.iExtraValue);
            //printf("Distance code: %d, required extra bits: %d, extra value: %d\n", dc.usSymbolID,dc.iExtraBits,dc.iExtraValue);
            // CRITICAL FIX: Advance the window past the matched bytes.
            i += bestLength;

        } else {
            // NO MATCH (Length < 3) or Invalid distance

            // Output the literal byte at the current position 'i'.
            //printf("Literal: %c\n", buffer[i]);
            appendToken(output_buffer, createLiteralLZ77(buffer[i]));

            // Advance the window by 1 byte (Literal case).
            i++;
        }
    }

    // --- 4. HANDLE REMAINING BYTES ---
    // The loop ended at bytes_read - 2. Handle the last 1 or 2 bytes as literals.
    for (; i < (int)bytes_read; i++) {
        //printf("Literal: %c\n", buffer[i]);
        appendToken(output_buffer, createLiteralLZ77(buffer[i]));
    }
}

static void readFileWithBuffer(const char* filename, unsigned char* buffer, uint16_t* hash_table, LZ77_buffer* output_buffer) {
    FILE *file = fopen(filename, "rb");

    if (ferror(file)) {
        perror("\n\nError reading file");
    } else {
        printf("\n\n--- Finished Reading ---\n");
    }
    size_t records = 0;
    size_t allbytes = 0;

    size_t bytes_read =  fread(buffer, 1, BUFFER_SIZE, file);
    compress_data(buffer,bytes_read,hash_table,output_buffer);
    allbytes+= bytes_read;
    records+=output_buffer->size;

    memcpy(buffer,buffer+WINDOW_SIZE,WINDOW_SIZE);
    notHardReset(hash_table);

    while ((bytes_read = fread(buffer+WINDOW_SIZE, 1, WINDOW_SIZE, file)) > 0) {
        allbytes+= bytes_read;
        //printf("%llu\n",bytes_read);

        compress_data(buffer+WINDOW_SIZE,bytes_read,hash_table,output_buffer);

        memcpy(buffer,buffer+WINDOW_SIZE,WINDOW_SIZE);
        notHardReset(hash_table);

        records+=output_buffer->size;
        freeLZ77Buffer(output_buffer);
        output_buffer = initLZ77Buffer();
    }
    printf("Starting size: %llu, \nEnding size: %llu, \nDifference: %llu\n",allbytes,records,allbytes-records);


    // 5. Close the file
    fclose(file);
}


int main(void) {
    uint16_t *hash_table = initHashTable();
    unsigned char *buffer = initBuffer();
    LZ77_buffer* outputBuffer = initLZ77Buffer();

    readFileWithBuffer("D:\\Cprojects\\deflate\\_DSC4947.ARW", buffer, hash_table, outputBuffer);

    for (int i = 0; i < outputBuffer->size; i++) {
        LZ77_compressed temp = outputBuffer->tokens[i];
        if (temp.type == LITERAL) {
            //printf("%c\n", temp.data.literal);
        } else {
            //printf("Distance: %d, Length: %d\n", temp.data.match.distance, temp.data.match.length);
        }
    }

    //printf("%d\n", outputBuffer->size);

    free(hash_table);
    free(buffer);
    freeLZ77Buffer(outputBuffer);
    return 0;
}
