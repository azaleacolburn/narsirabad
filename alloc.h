#ifndef NARSIRABAD_ALLOC

#define NARSIRABAD_ALLOC

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct Block {
    bool free;
    uint8_t offset;
    size_t size;
    void* ptr;
} Block;

typedef struct Allocator {
    Block* headers;
    // These are counts of block, not amount of memory remaining
    uint32_t header_len;
    uint32_t header_capacity;
} Allocator;

void* allocate(uint32_t size);

void deallocate(void* ptr);

#endif
