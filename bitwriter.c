//
// Created by Attila on 11/19/2025.
//

#include "bitwriter.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

#define BUFFER_SIZE 4096

#define MAGIC_NUMER 0x8B1F
#define COMPRESSION_METHOD 0x08 //deflate
#define FLAG 0b00000000 //RESERVED,RESERVED,RESERVED, FCOMMENT, FNAME, FEXTRA, FHCRC FTEXT
#define XFL 0x00
#define OS 0x03 //FAT filesystem

extern size_t flushBitWriterBuffer(BitWriter* bw) {
    //printf("index = %d\n",bw->index);
    size_t elementsWritten = fwrite(bw->buffer,1,bw->index,bw->file);
    //printf("Elements written to file: %llu \n", elementsWritten);
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

    // A hiba a 'value' és 'bitLength' frissítésében lehetett.
    // Használjunk hurok (loop) indexet a LSB-first kiolvasáshoz.

    // A 'value' legalacsonyabb 'bitLength' bitjeit írjuk ki.
    for (int i = 0; i < bitLength; i++) {

        // 1. Olvassuk ki a BIT-et a 'value'-ból (LSB-first)
        // A (value >> i) & 1 mindig a value 'i.' bitjét adja
        uint8_t bit = (value >> i) & 1;

        // 2. Szúrjuk be a bitet a bw->byte regiszter megfelelő pozíciójára (LSB-first)
        // A bit a bw->currentPosition pozíciójára kerül, ami LSB-től MSB felé halad.
        bw->byte |= (bit << bw->currentPosition);

        // 3. Haladjunk a következő bit pozícióra
        bw->currentPosition++;

        // 4. Ha a bájt megtelt, írjuk ki a puffert
        if (bw->currentPosition == 8) {
            flushByte(bw);
            // bw->byte és bw->currentPosition resetelődnek a flushByte-ban
        }
    }
}

extern void flush_bitstream_writer(BitWriter* bw) {
    if (bw->currentPosition > 0) {
        uint8_t bits_to_pad = 8 - bw->currentPosition;
        // Zérókkal való kitöltés (addData(bw, 0, bits_to_pad);)
        addData(bw, 0, bits_to_pad);
    }
    // ...
}

extern void addBytesFromMSB(BitWriter* bw, uint32_t value, uint8_t bytes) {
    flush_bitstream_writer(bw);
    for (int i = 0; i < bytes; i++) {
        bw->byte = (uint8_t)((value >> (i * 8)) & 0xFF);
        flushByte(bw);
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

    printf("CurrentPosition: %d\n",bw->currentPosition);

}



extern void freeBitWriter(BitWriter* bw) {
    const size_t flushedBytes = flushBitWriterBuffer(bw);
    printf("Last 8 byte: ");
    for (int i = 8; i > 0;i--) {
        printf("%d\t",bw->buffer[bw->index-i]);
    }
    printf("\nFlushed bytes %llu\n",flushedBytes);
    free(bw->fileName);
    fclose(bw->file);
    free(bw->buffer);
    free(bw);
}
