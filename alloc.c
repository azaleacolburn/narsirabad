#include "alloc.h"
#include "gc.h"
#include "mem.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#define NEW_BLOCK_THRESHOLD 8
#define INITIAL_ALLOCATOR_SIZE 128 * sizeof(int)
#define INITIAL_HEADER_BUFFER_CAPACITY 8
#define NA NARSIRABAD_ALLOCATOR

Allocator NARSIRABAD_ALLOCATOR;

extern char __bss_start;
extern char __data_start;

uintptr_t end_of_bss;
uintptr_t start_of_bss;

uintptr_t bottom_of_stack;
uintptr_t top_of_stack;

// Internal functions

void print_headers() {
    for (int i = 0; i < NA.header_len; i++) {
        Block header = NA.headers[i];
        printf("{ ptr: %po; size: %lu; offset: %d; free: %d }\n", header.ptr,
               header.size, header.offset, header.free);
    }
    puts("");
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

    NA.header_capacity = old_mem_cap * 2;
}

/// Attempts to split a freshly allocated block
///
/// header - The header of the block you wish to split.
/// new_size - The new size of the allocation, does not include the offset
///     If you wish to include the offset, it should be set in `header->offset`
///     before calling this function.
void try_split_block(Block* header, uint32_t new_size) {
    int remaining = header->size - new_size - header->offset;
    if (remaining <= NEW_BLOCK_THRESHOLD) {
        return;
    }

    // Shrink old header
    header->size = new_size;

    // Put new header in NA's block list
    int remaining_in_block_list = NA.header_capacity - NA.header_len;
    assert(remaining_in_block_list >= 0);

    if (remaining_in_block_list == 0) {
        expand_block_list();
    }

    // Create new header
    Block* next_header = NA.headers + NA.header_len;
    next_header->free = true;
    next_header->size = remaining;
    next_header->ptr = (void*)((uintptr_t)header->ptr + new_size);
    next_header->offset = 0;

    NA.header_len++;
}

void merge_blocks(uint16_t first_idx, uint16_t second_idx) {
    Block* first = NA.headers + first_idx;
    Block* second = NA.headers + second_idx;

    // Clear the block
    first->size += second->size + second->offset;
    memset(second->ptr, 0, second->size);

    // Shift the headers over
    memmove(second, second + 1, sizeof(Block) * (NA.header_len - second_idx));
    NA.header_len--;
}

/// Attempts to merged a free block with adjacent freed memory
/// Significantly more complex and difficult than if the blocks were arranged in
/// a linked list
///
/// However, merging is necessary because otherwise we would just split forever
/// and have to allocate new blocks more often.
void try_merge_block(uint16_t header_idx) {
    Block header = NA.headers[header_idx];
    uintptr_t start = (uintptr_t)header.ptr;
    uintptr_t end = start + header.size;

    bool below = false;
    bool above = false;

    for (int i = 0; i < NA.header_len; i++) {
        Block other_header = NA.headers[i];
        if (!other_header.free)
            continue;

        uintptr_t other_start = (uintptr_t)other_header.ptr;
        uintptr_t other_end = other_start + other_header.size;

        // TODO
        // We might also want to recurse whenever we find a new block
        // To make this more efficient, we should split checking above and below
        // into two different functions.
        if (other_start == end) {
            merge_blocks(header_idx, i);

            if (header_idx > i)
                header_idx--;
            below = true;
            i--;
        } else if (other_end == start) {
            merge_blocks(i, header_idx);

            if (header_idx > i)
                header_idx--;
            above = true;
            i--;
        }

        if (below && above)
            break;
    }
}

void try_merge_all_blocks() {
    for (int i = 0; i < NA.header_len; i++) {
        try_merge_block(i);
    }
}

/// Calculates the offset necessary to align `header->ptr` to
/// `alignof(max_align_t)`. Puts the calculated value in `header->offset`
///
///
/// This is the same alignment that `malloc` ensures:
/// http://kernel.org/doc/man-pages/online/pages/man3/malloc.3.html
///
/// `header` - The header representing the allocation to be aligned
void align_block(Block* header) {
    uint8_t diff = (uintptr_t)header->ptr % 8;
    if (diff != 0) {
        header->offset = 8 - diff;
    }

    header->offset = 0;
}

/// Allocates a new block of `size`
/// Expands the header buffer if necessary
///
/// Returns the success value of the new allocation
/// `true` for success, `false` for failure
bool expand_memory(uint32_t size) {
    void* ptr = map_new(size);
    if (ptr == NULL) {
        return false;
    }

    if (NA.header_len == NA.header_capacity) {
        expand_block_list();
    }

    Block* new_header = (NA.headers + NA.header_len);
    new_header->ptr = ptr;
    new_header->size = size;
    new_header->free = false;
    new_header->offset = 0;

    NA.header_len++;

    return true;
}

/// Attempts to perform an allocation
/// If it fails, it will not garbage collect nor alloate more memory
void* try_allocate(uint32_t size) {
    for (int i = 0; i < NA.header_len; i++) {
        Block* header = NA.headers + i;

        if (!header->free || header->size < size)
            continue;

        align_block(header);
        header->free = false;

        try_split_block(header, size);

        memset(header->ptr, 0, header->size);

        return (uintptr_t*)header->ptr + header->offset;
    }

    return NULL;
}

__attribute__((constructor)) void new_allocator() {
    NA.headers = map_new(INITIAL_HEADER_BUFFER_CAPACITY * sizeof(Block));
    if (NA.headers == NULL) {
        printf("Failed to allocate the NARSIRABAD_ALLOCATOR allocator\n");
        exit(1);
    }

    NA.headers->free = true;
    NA.headers->size = INITIAL_ALLOCATOR_SIZE;
    NA.headers->offset = 0;

    NA.headers->ptr = map_new(INITIAL_ALLOCATOR_SIZE);
    if (NA.headers->ptr == NULL) {
        printf("Failed to allocate first block of allocator\n");
        exit(1);
    }

    NA.header_capacity = INITIAL_HEADER_BUFFER_CAPACITY;
    NA.header_len = 1;

    bottom_of_stack = (uintptr_t)__builtin_stack_address();

    // TODO Verify that they're contiguous in memory
    start_of_bss = (uintptr_t)&__bss_start;
    end_of_bss = (uintptr_t)&__data_start;
}

/// This destructor will fail if not all blocks have be deallocated
__attribute__((destructor)) void destroy_allocator() {
    // We must first deallocate and merge blocks
    // because otherwise we would unmap blocks that were split from mappings
    // (which wouldn't be legal)
    //
    // But we can't just unmmap the first block, because there might be multiple
    // non-contiguous mappingr across all the headers
    for (int i = 0; i < NA.header_len; i++) {
        Block header = NA.headers[i];
        assert(header.ptr != NULL);

        deallocate(header.ptr);
    }
    try_merge_all_blocks();

    for (int i = 0; i < NA.header_len; i++) {
        Block header = NA.headers[i];
        assert(header.ptr != NULL);

        int unmap_result = munmap(header.ptr, header.size + header.offset);
        if (unmap_result == -1) {
            printf("Failed to unnmap block\n");
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
    // We need to get it here because otherwise we'd be looking the allocator's
    // stack frames if we end up garbage collecting, which would obviously lead
    // to not freeing blocks that could be
    top_of_stack = (uintptr_t)__builtin_stack_address();

    void* ptr = try_allocate(size);
    if (ptr != NULL)
        return ptr;

    // WARNING
    // We need to merge all blocks because we might be trying to allocate a size
    // larger than any individual blocks, but can fit in the aggregate size of
    // collected blocks
    //
    // However, we're going to go ahead and split them again anyway, which might
    // end up being really inefficient
    garbage_collect();
    try_merge_all_blocks();

    ptr = try_allocate(size);
    if (ptr != NULL) {
        return ptr;
    }

    bool expand_success = expand_memory(size);
    if (!expand_success)
        return NULL;

    // After expanding, we know that the only
    // valid header will be the last in the buffer
    Block* block = NA.headers + NA.header_len - 1;
    block->free = false;
    try_split_block(block, size);

    return block->ptr;
}

void deallocate(void* ptr) {
    for (int i = 0; i < NA.header_len; i++) {
        Block* header = NA.headers + i;
        if (header->ptr != (uint8_t*)ptr + header->offset)
            continue;

        header->free = true;
        try_merge_block(i);

        break;
    }
}
