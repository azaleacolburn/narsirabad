#ifndef NARSIRABAD

#define NARSIRABAD

#include <stdbool.h>
#include <stdint.h>

typedef struct Block {
    bool free;
    uint8_t offset;
    uint32_t size;
    void* ptr;
} Block;

typedef struct Allocator {
    Block* headers;
    // These are counts of block, not amount of memory remaining
    uint8_t header_len;
    uint8_t header_capacity;
} Allocator;

void* allocate(uint32_t size);

void deallocate(void* ptr);

#endif
