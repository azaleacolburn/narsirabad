#include "../alloc.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void allocate_lots() {
    int* b = allocate(100 * sizeof(int));
    assert(b != NULL);
    assert(*b == 0);
    // Don't deallocate, the GC should handle this
}

void gc_test() {
    allocate_lots();
    // This allocation exceeds the max capacity of the allocator, assuming
    // the old memory isn't freed (which it should be by the GC after this call
    // fails)
    int* b = allocate(100 * sizeof(int));
    assert(b != NULL);

    memset(b, 3, 100);
}

void no_reuse_test() {
    // This buffer should represent 4 integers
    int* b = allocate(4 * sizeof(int));
    assert(b != NULL);

    b[1] = 3;
    b[2] = 5;
    // Should print 0 3 5 0
    // printf("%d %d %d %d\n", *b, b[1], b[2], b[3]);

    // This buffer should represent 4 integers
    int* c = allocate(10 * sizeof(int));
    assert(c != NULL);

    c[0] = 4;
    c[9] = 6;
    // Should print 4 0 0 6
    // printf("%d %d %d %d\n", *c, c[1], c[2], c[9]);

    deallocate(b);
    deallocate(c);

    assert(b != c);
    b = NULL;
    c = NULL;

    puts("");
}

void reuse_test() {
    // This buffer should represent 4 integers
    int* b = allocate(10 * sizeof(int));
    if (b == NULL) {
        printf("Failed to allocate block of size %d", 128);
        exit(1);
    }
    b[1] = 3;
    b[2] = 5;
    // Should print 0 3 5 0
    // printf("%d %d %d %d\n", *b, b[1], b[2], b[3]);

    deallocate(b);

    // This buffer should represent 4 integers
    int* c = allocate(4 * sizeof(int));
    if (c == NULL) {
        printf("Failed to allocate block of size %d", 10 * 8);
        exit(1);
    }
    c[0] = 4;
    c[9] = 6;
    // Should print 4 0 0 6
    // printf("%d %d %d %d\n", *c, c[1], c[2], c[9]);

    deallocate(c);

    assert(b == c);
    b = NULL;
    c = NULL;

    puts("");
}

int main() {
    no_reuse_test();
    reuse_test();
    gc_test();
}
