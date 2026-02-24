#ifndef NARSIRABAD

#define NARSIRABAD

#include <stdbool.h>
#include <stdint.h>

typedef struct Block {
    bool free;
    uint32_t size;
    void* ptr;
} Block;
typedef struct Allocator {
    Block* headers;
    // These are counts of block, not amount of memory remaining
    uint8_t header_len;
    uint8_t header_capacity;
} Allocator;

// Internal functions
void try_split_block(Allocator* alloc, Block* header, uint32_t size);

void expand_block_list(Allocator* alloc);

void try_merge_block(Allocator* alloc, Block* header);

// Allocator functions
Allocator new_allocator(uint32_t initial_size,
                        uint32_t initial_header_capacity);

void* allocate(Allocator* alloc, uint32_t size);

void deallocate(Allocator* alloc, void* ptr);

#endif
