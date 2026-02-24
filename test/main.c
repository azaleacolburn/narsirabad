#include "../narsirabad.h"
#include <stdio.h>
#include <stdlib.h>

#define ALLOCATOR_SIZE 128 * 128

int main() {
    Allocator alloc = new_allocator(ALLOCATOR_SIZE, sizeof(Block) * 8);
    // This buffer should represent 4 integers
    int* b = allocate(&alloc, 128);
    if (b == NULL) {
        printf("Failed to allocate block of size %d in allocator of size %d",
               128, ALLOCATOR_SIZE);
        exit(1);
    }
    b[1] = 3;
    printf("%d", b[1]);
}
