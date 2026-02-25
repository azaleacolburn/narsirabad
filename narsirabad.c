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
#define INITIAL_ALLOCATOR_SIZE 128
#define INITIAL_HEADER_BUFFER_CAPACITY 8
#define NARSIRABAD_ALLOCATOR NA

// CONSTANTS
Allocator NARSIRABAD_ALLOCATOR;

__attribute__((constructor)) void new_allocator() {
    NA.headers = map_new(INITIAL_HEADER_BUFFER_CAPACITY);
    if (NA.headers == NULL) {
        printf("Failed to allocate the NARSIRABAD_ALLOCATOR allocator\n");
        exit(1);
    }

    NA.headers->free = 1;
    NA.headers->size = INITIAL_ALLOCATOR_SIZE;

    NA.headers->ptr = map_new(INITIAL_ALLOCATOR_SIZE);
    if (NA.headers->ptr == 0) {
        printf("Failed to NAate first block of allocator\n");
        exit(1);
    }

    NA.header_capacity = INITIAL_HEADER_BUFFER_CAPACITY;
    NA.header_len = 1;
}

__attribute__((destructor)) void destroy_allocator() {
    for (int i = 0; i < NA.header_len; i++) {
        Block* header = NA.headers + i;
        if (header->ptr == NULL)
            continue;

        int unmap_result = munmap(header->ptr, header->size);
        if (unmap_result == -1) {
            printf("Failed to unnmap block");
            exit(1);
        }
    }
    munmap(NA.headers, NA.header_capacity);
}

// EXPOSED FUNCTIONS

/// Guarantees that the returned block will be zeroed
// There's an issue where you can just write into another allocation if a larger
// block is split. I don't exactly know how to make the write fail, not sure if
// that's what it should do.
void* allocate(uint32_t size) {
    for (int i = 0; i < NA.header_len; i++) {
        Block* header = NA.headers + i;

        if (!header->free || header->size < size)
            continue;

        header->free = false;
        try_split_block(header, size);

        // Clear the buffer
        for (int i = 0; i < size; i++) {
            ((char*)header->ptr)[i] = 0;
        }

        return header->ptr;
    }

    // TODO Garbage Collect
    return NULL;
}

void deallocate(void* ptr) {
    Block* header = NA.headers;
    while ((intptr_t)header < NA.header_len) {
        if (header->ptr != ptr)
            continue;

        header->free = true;
        try_merge_block(header);
        break;
    }
}

// Internal functions

void try_split_block(Block* header, uint32_t new_size) {
    int remaining = header->size - new_size;
    if (remaining <= NEW_BLOCK_THRESHOLD) {
        return;
    }

    // Shrink old header
    header->size = new_size;

    // Put new header in NAator's block list
    int remaining_in_block_list = NA.header_capacity - NA.header_len;
    assert(remaining_in_block_list >= 0);

    if (remaining_in_block_list == 0) {
        expand_block_list();
    }

    // Create new header
    Block* next_header = NA.headers + NA.header_len;
    next_header->free = true;
    next_header->size = remaining;
    next_header->ptr = (void*)((intptr_t)header->ptr + new_size);
}

void expand_block_list() {
    int old_mem_cap = NA.header_capacity * sizeof(Block);

    void* in_place_mapping = map_fixed(NA.headers, old_mem_cap * 2);
    if (in_place_mapping != NULL) {
        return;
    }

    Block* new_mapping = map_new(old_mem_cap * 2);

    memmove(new_mapping, NA.headers, old_mem_cap);
    munmap(NA.headers, old_mem_cap);
}

/// Attempts to merged a free block with adjacent freed memory
/// Significantly more complex and difficult than if the blocks were arranged in
/// a linked list
///
/// However, merging is necessary because otherwise we would just split forever
/// and have to allocate new blocks more often.
void try_merge_block(Block* header) {}
