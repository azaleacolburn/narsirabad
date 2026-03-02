#ifndef NARSIRABAD

#define NARSIRABAD

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
    size_t header_len;
    size_t header_capacity;
} Allocator;

void* allocate(uint32_t size);

void deallocate(void* ptr);

#endif
