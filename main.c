#include <stdio.h>
#include <string.h>
#include "compress.h"
#include "decompress.h"
#include "status.h"

#define LIB_NAME        "Deflate"
#define LIB_VERSION     "1.3.7"
#define LIB_BUILD       __DATE__ " " __TIME__
#define LIB_COPYRIGHT   "(c) 2025 Arnoczki Attila All rights reserved."

static void printVersion(void) {
    printf(
       "\n"
       "================================================\n"
       "     ____       __ _      _        _       \n"
       "    |  _ \\ ___ / _| | ___| |_ __ _| |_ ___ \n"
       "    | | | / _ \\ |_| |/ _ \\ __/ _` | __/ _ \\\n"
       "    | |_| |  __/  _| |  __/ || (_| | ||  __/\n"
       "    |____/ \\___|_| |_|\\___|\\__\\__,_|\\__\\___|\n"
       "                                           \n"
       "                   DEFLATE\n"
       "                 Version %s\n"
       " %s\n"
       "================================================\n\n",
       LIB_VERSION,
       LIB_COPYRIGHT
   );
}

static void printHelp(void)
{
    printf(
        "\n"
        "Usage:\n"
        "  program help | -h      Show this help message\n"
        "  program version | -v   Show version information\n"
        "  program compress | -c <file>\n"
        "                        Compress the given file\n"
        "  program decompress | -d <file>\n"
        "                        Decompress the given file\n"
        "\n"
        "Examples:\n"
        "  program -c input.txt\n"
        "  program decompress archive.gz\n"
        "\n"
        "Note:\n"
        "  - All commands require valid file paths where appropriate.\n"
        "\n"
    );
}

extern int main(const int argc, char** argv) {
    if (argc == 1) {
        printVersion();
    }
    if (argc == 2) {
        if (strcmp(argv[1], "help") == 0 || strcmp(argv[1], "-h") == 0) {
            printHelp();
        } else if (strcmp(argv[1], "version") == 0 || strcmp(argv[1], "-v") == 0) {
            printVersion();
        } else {
            printf("Unknown command! \n Please read the provided help before using the program.\n\n");
            printHelp();
        }
    }
    STATUS* status = NULL;
    if (strcmp(argv[1], "compress") == 0 || strcmp(argv[1], "-c") == 0 || strcmp(argv[1], "decompress") == 0 ||
        strcmp(argv[1], "-d") == 0) {
        if (argc == 2) {
            printf("Not enough arguments.\n Please read the provided help before using the program.\n\n");
            printHelp();
        } else if (argc == 3) {
            if (strcmp(argv[1],"compress")==0 || strcmp(argv[1], "-c") == 0) {
                status = compress(argv[2]);
            } else if (strcmp(argv[1],"decompress")==0 || strcmp(argv[1], "-d") == 0) {
                status = decompress(argv[2]);
            }
        }
    }
    if (status != NULL) {
        free(status);
    }
    return 0;
}
