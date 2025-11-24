#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "compress.h"
#include "LZ77.h"
#include "distance.h"
#include "length.h"
#include "status.h"
#include "bitwriter.h"

int main(int argc, char** argv) {
    compress( "D:\\Cprojects\\deflate\\_DSC5810-Enhanced-NR.jpg");
    if (strcmp(argv[1],"compress")==0) {
        compress(argv[2]);
    }
    return 0;
}
