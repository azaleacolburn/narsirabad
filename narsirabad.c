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
#define INITIAL_ALLOCATOR_SIZE 128 * sizeof(int)
#define INITIAL_HEADER_BUFFER_CAPACITY 8
#define NARSIRABAD_ALLOCATOR NA

// CONSTANTS
Allocator NARSIRABAD_ALLOCATOR;

uintptr_t bottom_of_stack;
uintptr_t top_of_stack;

__attribute__((constructor)) void new_allocator() {
    NA.headers = map_new(INITIAL_HEADER_BUFFER_CAPACITY);
    if (NA.headers == NULL) {
        printf("Failed to allocate the NARSIRABAD_ALLOCATOR allocator\n");
        exit(1);
    }

    NA.headers->free = true;
    NA.headers->size = INITIAL_ALLOCATOR_SIZE;
    NA.headers->offset = 0;

    NA.headers->ptr = map_new(INITIAL_ALLOCATOR_SIZE);
    if (NA.headers->ptr == 0) {
        printf("Failed to allocate first block of allocator\n");
        exit(1);
    }

    NA.header_capacity = INITIAL_HEADER_BUFFER_CAPACITY;
    NA.header_len = 1;

    bottom_of_stack = (uintptr_t)__builtin_stack_address();
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

void print_headers() {
    for (int i = 0; i < NA.header_len; i++) {
        Block header = NA.headers[i];
        printf("{ ptr: %po; size: %lo; offset: %d; free: %b };", header.ptr,
               header.size, header.offset, header.free);
    }
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

/// Finds the block corresponding with the given pointer (the pointer must
/// point to the beginning of the block)
///
/// Returns the index of the block in `NA.headers`, or `-1` if it could not
/// be found
int8_t find_corresponding_block(void* ptr) {
    for (int i = 0; i < NA.header_len; i++) {
        Block header = NA.headers[i];

        if ((uint8_t*)header.ptr + header.offset == ptr) {
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
/// cyclical reference is encountered. It probably shouldn't though.
///
/// `used_blocks` - The array in which to store whether a block is alive.
///     The indicies of the array should correspond to the indicies of the
///     headers in `NA`
/// `buf` - The buffer in which to search for pointers
/// `size` - The number of potential pointers in `buf`
void mark_used_blocks_by_ptrs_in_buffer(bool used_blocks[NA.header_len],
                                        uintptr_t* buf, size_t size) {
    // TODO
    // One problem we have here is that we don't know whether the buffer grows
    // up or down
    //
    // If the caller knows, they can give us the correct side of the buffer
    // knowing that we will increment by one to traverse it
    //
    // We could also just have them pass the top and bottom of the buffer, that
    // could work too
    //
    // What if we encounted dangling pointers on old stack frames?
    // We might accidently have false negatives
    for (int i = 0; i < size; i++) {
        int8_t block_idx = find_corresponding_block((void*)buf[i]);
        if (block_idx == -1 && !NA.headers[block_idx].free) {
            continue;
        }

        printf("FOUND ALLOCATED POINTER\n");

        if (used_blocks[block_idx]) {
            continue;
        }

        Block header = NA.headers[block_idx];
        mark_used_blocks_by_ptrs_in_buffer(used_blocks, header.ptr,
                                           header.size);

        used_blocks[block_idx] = true;
    }
}

/// If they happen to have the same number that they don't mean as a pointer,
/// then we have a false positive, which is fine
///
/// Also, they could modify their pointer with the intention of obfuscating it
/// from us, we're not going to worry about this case
///
void garbage_collect() {
    bool used_blocks[NA.header_len];
    memset(used_blocks, 0, NA.header_len);

    // WARNING
    // Horrible hack to get the bounds on the stack
    // This relies on programs pre-allocating the stack
    // We might have to worry about the dead zone here

    // Align `top_of_stack` to 8
    uintptr_t diff = (uintptr_t)top_of_stack % 8;
    if (diff != 0) {
        top_of_stack += 8 - diff;
    }

    // Yes, this is correct
    assert(top_of_stack < bottom_of_stack);

    /// The number of pointers that can exist in the current buffer
    /// Rounded down because the top the the stack might not be aligned to
    /// `8`, but the bottom will be
    uint8_t stack_size =
        ((uintptr_t)bottom_of_stack - (uintptr_t)top_of_stack) /
        sizeof(uintptr_t);
    printf("stack size: %d\ntos: %po\n", stack_size, (void*)top_of_stack);
    mark_used_blocks_by_ptrs_in_buffer(used_blocks, (uintptr_t*)top_of_stack,
                                       stack_size);

    for (int i = 0; i < NA.header_len; i++) {
        printf("used: %b\n", used_blocks[i]);
        if (used_blocks[i])
            continue;
        printf("here\n");

        // TODO
        // Check if this modified `NA->headers` (it should)
        Block* header = NA.headers + i;
        header->free = true;
        header->size += header->offset;
        header->offset = 0;
        // TODO
        // Merge everything at the end instead
        try_merge_block(i);
    }
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

// EXPOSED FUNCTIONS

/// Guarantees that the returned block will be zeroed
// There's an issue where you can just write into another allocation if a larger
// block is split. I don't exactly know how to make the write fail, not sure if
// that's what it should do.
void* allocate(uint32_t size) {
    top_of_stack = (uintptr_t)__builtin_stack_address();

    void* ptr = try_allocate(size);
    if (ptr != NULL)
        return ptr;

    printf("NULL ALLOCATION VALUE");

    garbage_collect();
    print_headers();
    printf("\n");
    ptr = try_allocate(size);
    if (ptr != NULL) {
        return ptr;
    }
    printf("NULL ALLOCATION VALUE");

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
