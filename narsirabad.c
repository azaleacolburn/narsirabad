#include "narsirabad.h"
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
    NA.headers->offset = 0;

    NA.headers->ptr = map_new(INITIAL_ALLOCATOR_SIZE);
    if (NA.headers->ptr == 0) {
        printf("Failed to NAate first block of allocator\n");
        exit(1);
    }

    NA.header_capacity = INITIAL_HEADER_BUFFER_CAPACITY;
    NA.header_len = 1;
}

/// This destructor will fail if not all blocks have be deallocated
__attribute__((destructor)) void destroy_allocator() {
    for (int i = 0; i < NA.header_len; i++) {
        Block header = NA.headers[i];
        if (header.ptr == NULL)
            continue;

        int unmap_result = munmap(header.ptr, header.size);
        if (unmap_result == -1) {
            printf("Failed to unnmap block");
            exit(1);
        }
    }
    munmap(NA.headers, NA.header_capacity);
}

// Internal functions

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
    next_header->ptr = (void*)((intptr_t)header->ptr + new_size);
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
    NA.header_len -= 1;
}

/// Attempts to merged a free block with adjacent freed memory
/// Significantly more complex and difficult than if the blocks were arranged in
/// a linked list
///
/// However, merging is necessary because otherwise we would just split forever
/// and have to allocate new blocks more often.
void try_merge_block(uint16_t header_idx) {
    Block header = NA.headers[header_idx];
    intptr_t start = (intptr_t)header.ptr;
    intptr_t end = start + header.size;

    for (int i = 0; i < NA.header_len; i++) {
        Block other_header = NA.headers[i];
        if (!other_header.free)
            continue;

        intptr_t other_start = (intptr_t)other_header.ptr;
        intptr_t other_end = other_start + other_header.size;

        if (other_start == end) {
            merge_blocks(header_idx, i);

            if (header_idx > i)
                header_idx--;
            i--;
        } else if (other_end == start) {
            merge_blocks(i, header_idx);

            if (header_idx > i)
                header_idx--;
            i--;
        }
    }
}

/// Calculates the offset necessary to align `header->ptr` to
/// `alignof(max_align_t)`. Puts the calculated value in `header->offset`
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

/// Finds the block corresponding with the given pointer (the pointer must point
/// to the beginning of the block)
///
/// Returns the index of the block in `NA.headers`, or `-1` if it could not be
/// found
int8_t find_corresponding_block(void* ptr) {
    for (int i = 0; i < NA.header_len; i++) {
        Block header = NA.headers[i];

        if (header.ptr + header.offset == ptr) {
            return i;
        }
    }

    return -1;
}

/// Marks every block that holds a pointer to an allocation owned by `NA` as
/// used, if that pointer is found in the current buffer.
///
/// For every pointer found this way, the function recurses on the buffer that
/// pointer points to, taking the `size` of this sub-buffer from the header
/// corresponding to the parent pointer (`header->size`).
///
/// The marking algorithm over a single buffer has time complexity:
/// O(`size` * `NA.header_len`)
///
/// This function has the potential to recure indefinitely at the moment if a
/// cyclical reference is encountered.
///
/// `used_blocks` - The array in which to store whether a block is alive.
///     The indicies of the array should correspond to the indicies of the
///     headers in `NA`
/// `buf` - The buffer in which to search for pointers
/// `size` - The number of potential pointers in `buf`
void mark_used_blocks_by_ptrs_in_buffer(bool used_blocks[NA.header_len],
                                        uintptr_t* buf, size_t size) {
    for (int i = 0; i < size; i++) {
        int8_t block_idx = find_corresponding_block((void*)buf[i]);
        if (block_idx == -1) {
            continue;
        }

        // TODO
        // Figure out a way to handle cyclical references

        Block header = NA.headers[block_idx];
        mark_used_blocks_by_ptrs_in_buffer(used_blocks, header.ptr,
                                           header.size);

        used_blocks[block_idx] = true;
    }
}

void garbage_collect() {
    // WARNING
    // Horrible hack to get the bounds on the stack
    uintptr_t volatile m = 0;
    uintptr_t volatile* top_of_stack = &m;
    uintptr_t* volatile bottom_of_stack = (uintptr_t*)(&NA + 1);
    uintptr_t* head = bottom_of_stack;

    bool used_blocks[NA.header_len];
    memset(used_blocks, 0, NA.header_len);

    mark_used_blocks_by_ptrs_in_buffer(used_blocks, top_of_stack,
                                       (bottom_of_stack - top_of_stack)/sizeof(intptr_t); // This shouldn't round because it's aligned, I think
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

        align_block(header);
        header->free = false;

        try_split_block(header, size);

        memset(header->ptr, 0, header->size);

        return header->ptr + header->offset;
    }

    // TODO Garbage Collect
    return NULL;
}

void deallocate(void* ptr) {
    for (int i = 0; i < NA.header_len; i++) {
        Block* header = NA.headers + i;
        if (header->ptr != ptr + header->offset)
            continue;

        header->free = true;
        try_merge_block(i);

        break;
    }
}
