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
// TODO Remove these from the header file
void try_split_block(Block* header, uint32_t size);

void expand_block_list();

void try_merge_block(Block* header);

// Exposed functions
void* allocate(uint32_t size);

void deallocate(void* ptr);

#endif
