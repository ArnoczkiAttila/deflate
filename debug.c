//
// Created by Rendszergazda on 11/29/2025.
//

#include "debug.h"

#include <stdio.h>
#include "HUFFMAN_TABLE.h"

// Helper to print binary
extern void print_binary(uint16_t num, int len) {
    for (int i = len - 1; i >= 0; i--) {
        printf("%d", (num >> i) & 1);
    }
}

extern void print_debug_tree(HuffmanTree* tree, const char* name) {
    printf("\n--- DEBUGGING TREE: %s ---\n", name);
    printf("Sym\tLen\tCode\n");
    printf("------------------------\n");

    // Only print a few interesting symbols to avoid spamming console
    // 1. Literals (common text chars)
    // 2. EOB (256)
    // 3. The suspicious symbol (271)

    int interesting_symbols[] = {
        'A', 'a', ' ', 'e', '\n', // Common text
        256,                      // End of Block
        271                       // Your error symbol
    };

    for (int i = 0; i < 7; i++) {
        int sym = interesting_symbols[i];
        if (sym < tree->total_symbols) {
            CanonicalCode hc = tree->codes_list[sym];
            if (hc.length > 0) {
                printf("%d\t%d\t", sym, hc.length);
                print_binary(hc.code, hc.length);
                printf("\n");
            } else {
                printf("%d\t(Not used)\n", sym);
            }
        }
    }
    printf("------------------------\n");
}