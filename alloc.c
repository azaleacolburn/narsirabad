#include "alloc.h"
#include "gc.h"
#include "mem.h"
#include "vec.h"

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
    printf("Free Headers:\n");
    for (int i = 0; i < NA.free_headers.len; i++) {
        Block* header = BRL_idx(&NA.free_headers, i);

        printf("\t{ ptr: %po; size: %lu; offset: %d }\n", header->ptr,
               header->size, header->offset);
    }

    printf("Used Headers:\n");
    for (int i = 0; i < NA.used_headers.len; i++) {
        Block* header = BRL_idx(&NA.used_headers, i);

        printf("\t{ ptr: %po; size: %lu; offset: %d }\n", header->ptr,
               header->size, header->offset);
    }

    puts("");
}

void new_free_header(void* ptr, size_t size) {
    // These will automatically expand the lists
    Block* header = BL_new_header(&NA.headers, size, ptr);
    BRL_push(&NA.free_headers, header);
}

/*
 * Attempts to split a freshly allocated block.
 *
 * Returns the address of the first part of the block. If the block list is
 * not reallocated, it will be equal to `header`.
 *
 * `header` - The header of the block you wish to split.
 * `new_size` - The new size of the allocation, does not include the offset
 *     If you wish to include the offset, it should be set in
 * `header->offset` before calling this function.
 */
Block* try_split_block(uint32_t block_idx, uint32_t new_size) {
    Block* header = BL_idx(&NA.headers, block_idx);
    // We have to save the pointer
    // because `header` will become invalid if we expand the block list
    uintptr_t ptr = (uintptr_t)header->ptr;

    uint32_t remaining = header->size - new_size - header->offset;
    if (remaining <= NEW_BLOCK_THRESHOLD) {
        return header;
    }

    // Shrink old header
    header->size = new_size;

    // uint16_t remaining_in_block_list = NA.headers.cap - NA.headers.cap;
    // assert(remaining_in_block_list >= 0);

    // if (remaining_in_block_list == 0) {
    //     expand_block_list();
    // }

    new_free_header((void*)(ptr + new_size), remaining);

    return BL_idx(&NA.headers, block_idx);
}

void merge_blocks(uint16_t first_idx, uint16_t second_idx) {
    Block* first = BL_idx(&NA.headers, first_idx);
    Block* second = BL_idx(&NA.headers, second_idx);

    // Clear the block
    first->size += second->size + second->offset;

    BL_remove(&NA.headers, first_idx);

    BRL_find_remove(&NA.free_headers, first);
}

bool is_free(Block* header) { return BRL_find(&NA.free_headers, header) != -1; }

/// Attempts to merged a free block with adjacent freed memory
/// Significantly more complex and difficult than if the blocks were arranged in
/// a linked list
///
/// However, merging is necessary because otherwise we would just split forever
/// and have to allocate new blocks more often.
void try_merge_block(uint16_t header_idx) {
    Block header = *BL_idx(&NA.headers, header_idx);
    uintptr_t start = (uintptr_t)header.ptr;
    uintptr_t end = start + header.size;

    bool below = false;
    bool above = false;

    for (int i = 0; i < NA.headers.len; i++) {
        Block* other_header = BL_idx(&NA.headers, i);
        if (is_free(other_header))
            continue;

        uintptr_t other_start = (uintptr_t)other_header->ptr;
        uintptr_t other_end = other_start + other_header->size;

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
    for (int i = 0; i < NA.headers.len; i++) {
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

    Block* new_header = BL_new_header(&NA.headers, size, ptr);
    BRL_push(&NA.used_headers, new_header);

    return true;
}

/// Attempts to perform an allocation
/// If it fails, it will not garbage collect nor alloate more memory
void* try_allocate(uint32_t size) {
    printf("Trying to allocate: %d\n", size);
    print_headers();
    for (int i = 0; i < NA.headers.len; i++) {
        Block* header = BL_idx(&NA.headers, i);

        if (!is_free(header) || header->size < size)
            continue;

        align_block(header);

        BRL_find_remove(&NA.used_headers, header);
        BRL_push(&NA.free_headers, header);

        // We have to assign here, because our pointer could become invalid if
        // `NA.headers` is reallocated
        header = try_split_block(i, size);

        memset(header->ptr, 0, header->size);

        return (uintptr_t*)header->ptr + header->offset;
    }

    return NULL;
}

__attribute__((constructor)) void new_allocator() {
    NA.headers = BL_new(INITIAL_HEADER_BUFFER_CAPACITY);

    void* ptr = map_new(INITIAL_ALLOCATOR_SIZE);
    if (ptr == NULL) {
        printf("Failed to allocate first block of allocator\n");
        exit(1);
    }

    BL_new_header(&NA.headers, INITIAL_ALLOCATOR_SIZE, ptr);

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
        Block* header = &NA.headers[i];
        assert(header->ptr != NULL);

        header->free = true;
        header->size += header->offset;
        header->offset = 0;
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
    printf("here\n");
    try_merge_all_blocks();

    print_headers();

    ptr = try_allocate(size);
    if (ptr != NULL) {
        return ptr;
    }

    bool expand_success = expand_memory(size);
    if (!expand_success)
        return NULL;

    printf("Post expansion\n");

    // After expanding, we know that the only
    // valid header will be the last in the buffer
    Block* block = NA.headers + NA.header_len - 1;
    block->free = false;
    try_split_block(NA.header_len - 1, size);

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
