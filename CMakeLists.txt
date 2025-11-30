cmake_minimum_required(VERSION 4.0)
project(deflate C)

set(CMAKE_C_STANDARD 99)

add_executable(deflate main.c
        LZ77.h
        LZ77.c
        distance.h
        distance.c
        length.h
        length.c
        node.c
        compress.h
        status.h
        compress.c
        debugmalloc.h
        bitwriter.c
        bitwriter.h
        HUFFMAN_TABLE.h
        HUFFMAN_TABLE.c
        CRC_CHECKSUM.h
        CRC_CHECKSUM.c
        decompress.c
        bitreader.c
        bitreader.h
        decompress.h
        status.c
        debug.h
        debug.c)

#target_compile_options(deflate PRIVATE -Wall -Werror)
