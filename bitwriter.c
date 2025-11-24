//
// Created by Attila on 11/19/2025.
//

#include "bitwriter.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

#define BUFFER_SIZE 4096

#define MAGIC_NUMER 0x1F8B
#define COMPRESSION_METHOD 0x08 //deflate
#define FLAG 0b00001001 //RESERVED,RESERVED,RESERVED, FCOMMENT, FNAME, FEXTRA, FHCRC FTEXT
#define XFL 0x00
#define OS 0x00 //FAT filesystem

static size_t flushBitWriterBuffer(BitWriter* bw) {
    printf("index = %d\n",bw->index);
    size_t elementsWritten = fwrite(bw->buffer,1,bw->index,bw->file);
    printf("Elements written to file: %llu \n", elementsWritten);
    bw->index = 0;
    return elementsWritten;
}

extern BitWriter* initBitWriter(void) {
    BitWriter* bw = (BitWriter*) malloc(sizeof(BitWriter));
    bw->file = NULL; //temp
    bw->byte = 0;
    bw->buffer = (uint8_t*) malloc(BUFFER_SIZE);
    bw->currentPosition = 0;
    bw->bufferSize = BUFFER_SIZE;
    bw->index = 0;
    return bw;
}

static void flushByte(BitWriter* bw) {
    bw->buffer[bw->index] = bw->byte;
    bw->index++;
    bw->byte = 0;
    bw->currentPosition = 0;
    //printf("%d\t",bw->index);
    if (bw->index == bw->bufferSize) flushBitWriterBuffer(bw);
}


extern void addData(BitWriter* bw, uint32_t value, uint8_t bitLength) {
    if (bitLength == 0 || bitLength > 32) {
        return;
    }
    uint32_t current_value = value;
    while (bitLength > 0) {
        uint8_t bitsLeftInByte = 8 - bw->currentPosition;

        // Calculate how many bits we can write into the current buffer.
        // This is the minimum of the bits left in the input and the space left in the buffer.
        uint8_t bits_to_transfer = (bitLength < bitsLeftInByte) ? bitLength : bitsLeftInByte;

        // --- LSB-FIRST INSERTION LOGIC ---

        // 1. Create a mask to isolate the 'bits_to_transfer' LSBs from the input 'value'.
        //    Use a wider type for the mask calculation to ensure correct behavior if bits_to_transfer == 8.
        uint32_t mask = (1 << bits_to_transfer) - 1;

        // 2. Extract the relevant LSB chunk from the input value.
        //    The snippet is now up to 8 bits, stored in a 32-bit container.
        uint32_t snippet = current_value & mask;

        // 3. Shift the snippet to its correct position in the byte buffer
        //    (left-shifted by currentPosition) and OR it in.
        //    The cast to (uint8_t) on the right side is safe since snippet is <= 255.
        bw->byte |= (uint8_t)(snippet << bw->currentPosition);

        // --- UPDATE STATE ---

        // 4. Advance the buffer position.
        bw->currentPosition += bits_to_transfer;

        // 5. Update the input value and length to discard the bits we just wrote.
        current_value >>= bits_to_transfer; // Discard the LSBs that were just written.
        bitLength -= bits_to_transfer;

        // --- HANDLE FLUSH (Byte Boundary Crossing) ---

        // If the byte is now full (currentPosition == 8), flush it.
        if (bw->currentPosition == 8) {
            flushByte(bw);
            // The position and byte are reset inside flushByte()
        }
    }
}

extern void flush_bitstream_writer(BitWriter* bw) {
    // Check if the current byte is only partially full (currentPosition is 1 to 7)
    if (bw->currentPosition > 0) {
        // Calculate the number of zero bits needed to fill the byte
        uint8_t bits_to_pad = 8 - bw->currentPosition;

        // Since the current value is 0, we can use addData to write 'bits_to_pad' zeros.
        // addData handles the correct bit shifting and calls flushByte when done.
        addData(bw, 0, bits_to_pad);
    }

    // After this, bw->currentPosition must be 0, and the last byte is flushed
    // into the buffer (bw->buffer[bw->index] is the last padded byte).
    // The final flush to disk (fwrite) is usually handled by closeBitWriter or a large buffer flush.
}

extern void addBytesFromMSB(BitWriter* bw, uint32_t value, uint8_t bytes) {
    // 1. Ensure bitstream is aligned.
    flush_bitstream_writer(bw);

    // 2. Write the 4 bytes in LSB-first order (Little-Endian)
    for (int i = 0; i < bytes; i++) {
        // Extract byte i (0, 1, 2, 3). The LSB byte (i=0) is written first.
        bw->byte = (uint8_t)((value >> (i * 8)) & 0xFF);
        flushByte(bw); // Writes the byte and resets position to 0
    }
}

extern void createFile(BitWriter* bw, char* fileName, char* extension) {
    char* newFileName = (char*) malloc(strlen(fileName) + strlen(extension) + 2);
    strcpy(newFileName, fileName);
    strcat(newFileName, ".");
    strcat(newFileName, extension);

    FILE* file = fopen(newFileName, "wb");
    bw->fileName = newFileName;
    bw->file = file;

    addBytesFromMSB(bw, MAGIC_NUMER, 2); //ID1 ID2
    addBytesFromMSB(bw, COMPRESSION_METHOD, 1); //CM
    addBytesFromMSB(bw, FLAG,1); //FLG
    addBytesFromMSB(bw, (uint32_t) time(NULL), 4); //MTIME
    addBytesFromMSB(bw, XFL,1); //XFL
    addBytesFromMSB(bw, OS, 1);
    //No extra fields

    size_t len = strlen(fileName);
    for (size_t i = 0; i<len; i++) {
        addBytesFromMSB(bw, fileName[i], 1);
    }
    addBytesFromMSB(bw,'\0',1);
}



extern void freeBitWriter(BitWriter* bw) {
    flushBitWriterBuffer(bw);
    free(bw->fileName);
    fclose(bw->file);
    free(bw->buffer);
    free(bw);
}
