#include "gc.h"
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#define NEW_BLOCK_THRESHOLD 128

/// Creates a new allocator by allocating headers on the "heap" and
Allocator new_allocator(uint32_t initial_size,
                        uint32_t initial_header_capacity) {
    Allocator alloc;
    alloc.headers = mmap(NULL, initial_header_capacity * sizeof(Block),
                         PROT_READ | PROT_WRITE | PROT_EXEC,
                         MAP_SHARED | MAP_ANONYMOUS, 0, 0);
    if (alloc.headers == 0) {
        printf("Failed to allocate allocator");
        exit(1);
    }

    alloc.headers->free = 0;
    alloc.headers->size = initial_size;

    alloc.headers->ptr =
        mmap(NULL, initial_size, PROT_READ | PROT_WRITE | PROT_EXEC,
             MAP_SHARED | MAP_ANONYMOUS, 0, 0);
    if (alloc.headers->ptr == 0) {
        printf("Failed to allocate first block of allocator");
        exit(1);
    }

    alloc.header_capacity = initial_header_capacity;
    alloc.header_len = 1;

    return alloc;
}

void* allocate(Allocator* alloc, uint32_t size) {
    Block* header = alloc->headers;
    while ((intptr_t)header < alloc->header_len) {
        if (!header->free || header->size > size)
            continue;

        header->free = false;
        try_split_block(alloc, header, size);

        return header->ptr;
    }

    return NULL;
}

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

    void* in_place_mapping = mmap(alloc->headers, old_mem_cap * 2,
                                  PROT_READ | PROT_WRITE | PROT_EXEC,
                                  MAP_SHARED | MAP_ANONYMOUS | MAP_FIXED, 0, 0);

    if (in_place_mapping != NULL) {
        return;
    }

    Block* new_mapping =
        mmap(NULL, old_mem_cap * 2, PROT_READ | PROT_WRITE | PROT_EXEC,
             MAP_SHARED | MAP_ANONYMOUS, 0, 0);

    memmove(new_mapping, alloc->headers, old_mem_cap);
    munmap(alloc->headers, old_mem_cap);
}
