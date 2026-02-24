#include "narsirabad.h"
#include "mem.h"
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#define NEW_BLOCK_THRESHOLD 128

// Exposed functions

/// Creates a new allocator by allocating headers on the "heap" and
Allocator new_allocator(uint32_t initial_size,
                        uint32_t initial_header_capacity) {
    Allocator alloc;
    alloc.headers = map_new(initial_header_capacity);
    if (alloc.headers == NULL) {
        printf("Failed to allocate allocator");
        exit(1);
    }

    alloc.headers->free = 0;
    alloc.headers->size = initial_size;

    alloc.headers->ptr = map_new(initial_size);
    if (alloc.headers->ptr == 0) {
        printf("Failed to allocate first block of allocator");
        exit(1);
    }

    alloc.header_capacity = initial_header_capacity;
    alloc.header_len = 1;

    return alloc;
}

/// Guarantees that the returned block will be zeroed
void* allocate(Allocator* alloc, uint32_t size) {
    Block* header = alloc->headers;
    while ((intptr_t)header < alloc->header_len) {
        if (!header->free || header->size > size)
            continue;

        header->free = false;
        try_split_block(alloc, header, size);

        // Clear the buffer
        for (int i = 0; i < size; i++) {
            ((char*)header->ptr)[i] = 0;
        }

        return header->ptr;
    }

    return NULL;
}

void deallocate(Allocator* alloc, void* ptr) {
    Block* header = alloc->headers;
    while ((intptr_t)header < alloc->header_len) {
        if (header->ptr != ptr)
            continue;

        header->free = true;
        try_merge_block(alloc, header);
        break;
    }
};

// Internal functions

void try_split_block(Allocator* alloc, Block* header, uint32_t new_size) {
    int remaining = header->size - new_size;
    if (remaining <= NEW_BLOCK_THRESHOLD) {
        return;
    }

    // Shrink old header
    header->size = new_size;

    // Put new header in allocator's block list
    int remaining_in_block_list = alloc->header_capacity - alloc->header_len;
    assert(remaining_in_block_list >= 0);

    if (remaining_in_block_list == 0) {
        expand_block_list(alloc);
    }

    // Create new header
    Block* next_header = alloc->headers + alloc->header_len;
    next_header->free = true;
    next_header->size = remaining;
    next_header->ptr = (void*)((intptr_t)header->ptr + new_size);
}

void expand_block_list(Allocator* alloc) {
    int old_mem_cap = alloc->header_capacity * sizeof(Block);

    void* in_place_mapping = map_fixed(alloc->headers, old_mem_cap * 2);
    if (in_place_mapping != NULL) {
        return;
    }

    Block* new_mapping = map_new(old_mem_cap * 2);

    memmove(new_mapping, alloc->headers, old_mem_cap);
    munmap(alloc->headers, old_mem_cap);
}

/// Attempts to merged a free block with adjacent freed memory
/// Significantly more complex and difficult than if the blocks were arranged in
/// a linked list
///
/// However, merging is necessary because otherwise we would just split forever
/// and have to allocate new blocks more often.
void try_merge_block(Allocator* alloc, Block* header) {}
