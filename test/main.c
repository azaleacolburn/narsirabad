#include "../narsirabad.h"
#include <stdio.h>

int main() {
    Allocator alloc = new_allocator(128 * 128, sizeof(Block) * 8);
    // This buffer should represent 4 integers
    int* b = allocate(&alloc, 128);
    b[1] = 3;
    printf("%d", b[1]);
}
