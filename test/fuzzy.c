#include "../alloc.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main() {
    int iterations = rand() % 5000;

    printf("Begin Fuzzy Testing for %d Iterations\n", iterations);

    for (int i = 0; i < 305; i++) {
        int bytes = rand() % (sizeof(int) * 1000);
        int ints = bytes / sizeof(int);

        char* block = allocate(bytes);
        assert(block != NULL);
        printf("\tReserved %d bytes or %d ints at %p\n", bytes, ints,
               (void*)block);

        memset(block, 3, bytes);
        assert(block[bytes / 2] == 3);
    }

    printf("\nFuzzy Testing Successful\n");
}
