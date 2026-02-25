#include "../narsirabad.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

int main() {
    // This buffer should represent 4 integers
    int* b = allocate(4);
    if (b == NULL) {
        printf("Failed to allocate block of size %d", 128);
        exit(1);
    }
    b[1] = 3;
    b[2] = 5;
    // Should print 0 3 5 0
    printf("%d %d %d %d\n", *b, b[1], b[2], b[3]);

    deallocate(b);

    // This buffer should represent 4 integers
    int* c = allocate(10);
    if (c == NULL) {
        printf("Failed to allocate block of size %d", 10 * 8);
        exit(1);
    }
    c[1] = 4;
    c[9] = 6;
    // Should print 0 4 0 6
    printf("%d %d %d %d\n", *c, c[1], c[2], c[9]);

    deallocate(c);

    assert(b == c);
}
