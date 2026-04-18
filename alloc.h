#ifndef NARSIRABAD_ALLOC

#define NARSIRABAD_ALLOC

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint8_t offset;
    size_t size;
    void* ptr;
} Block;

// A list of all headers `List<Block>`
typedef struct {
    Block* arr;
    // These are counts of block, not amount of memory remaining
    uint32_t len;
    uint32_t cap;
} BlockList;

// A list containing indicies of headers in `NA.headers`
//
// This structure should only have two instances: `NA.free_headers` and
// `NA.used_headers` `List<Block*>`
typedef struct {
    size_t* arr;
    // These are counts of block, not amount of memory remaining
    uint32_t len;
    uint32_t cap;
} BlockRefList;

typedef struct Allocator {
    BlockList headers;

    BlockRefList free_headers;
    BlockRefList used_headers;
} Allocator;

bool is_free(Block* header);

void* allocate(uint32_t size);

void deallocate(void* ptr);

#endif
