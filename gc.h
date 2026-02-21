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

void expand_block_list(Allocator* alloc);

void try_split_block(Allocator* alloc, Block* header, uint32_t size);

void* allocate(Allocator* alloc, uint32_t size);
