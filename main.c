#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

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

static int processMatch(const unsigned char *currentPointer, const unsigned char *oldPointer) {
    int length = 0;
    int index = 0;
    while (currentPointer[index] == oldPointer[index++]) {
        length++;
        if (length == 258) {
            return length;
        }
    }
    return length;
}

static void readFileWithBuffer(const char *filename, unsigned char *buffer, uint16_t *hash_table) {
    FILE *file = fopen(filename, "rb");
    uint16_t hashKey = generateHashKey(buffer);
    uint16_t hashValue = hash_table[hashKey];
    bool matchFound = false;
    int length = 0;

    if (file == NULL) {
        perror("Error opening file");
        return;
    }

    size_t bytes_read = fread(buffer, 1, BUFFER_SIZE, file);

    for (int i = 0; i < (int)bytes_read - 3; i++) {
        const int distance = i - hashValue;
        if (!matchFound) {
            // Check 1: Is the hash table entry valid (not the empty index)?
            if (hashValue != EMPTY_INDEX) {

                // Check 2: Is the distance within the 32KB window?
                // Note: Since i and hashValue are within 0-65535, distance will be <= 65535.
                if (distance <= WINDOW_SIZE) {

                    // Check 3: Is the distance greater than 0?
                    if (distance > 0) {
                        length = processMatch((buffer+i),(buffer+hashValue));
                        if (length >=3) {
                            printf("Match found! length=%d\n", length);
                            matchFound = true;
                        }
                    }
                }
            }
        } else {
            length--;
            if (length==0) {
                matchFound = false;
            }
        }

        hash_table[hashKey] = (uint16_t) i;

        //A végén
        hashKey = updateHashKey(hashKey, &buffer[i + 1]);
        hashValue = hash_table[hashKey];
    }
    printf("beolvasva %llu\n", bytes_read);

    if (ferror(file)) {
        perror("\n\nError reading file");
    } else {
        printf("\n\n--- Finished Reading ---\n");
    }

    // 5. Close the file
    fclose(file);
}


int main(void) {
    uint16_t *hash_table = initHashTable();
    unsigned char *buffer = initBuffer();

    readFileWithBuffer("C:\\Users\\Attila\\Documents\\GitHub\\deflate\\_DSC5810-Enhanced-NR.jpg", buffer, hash_table);

    free(hash_table);
    free(buffer);
    return 0;
}
