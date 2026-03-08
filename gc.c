#include "alloc.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#define NA NARSIRABAD_ALLOCATOR

extern Allocator NARSIRABAD_ALLOCATOR;

extern uintptr_t top_of_stack;
extern uintptr_t bottom_of_stack;

extern uintptr_t bottom_of_bss;
extern uintptr_t top_of_bss;

/*
 * Finds the block corresponding with the given pointer (the pointer must
 * point to the beginning of the block)
 * Returns the index of the block in `NA.headers`, or `-1` if it could not
 * be found
 */
int8_t find_corresponding_block(void* ptr) {
    for (int i = 0; i < NA.header_len; i++) {
        Block header = NA.headers[i];

        if ((uint8_t*)header.ptr + header.offset == ptr) {
            return i;
        }
    }

    return -1;
}

/*
 * Marks every block that holds a pointer to an allocation owned by `NA` as
 * used, if that pointer is found in the current buffer.
 *
 * For every pointer found this way, the function recurses on the buffer
 * that pointer points to, taking the `size` of this sub-buffer from the
 * header corresponding to the parent pointer (`header->size`).
 *
 * The marking algorithm over a single buffer has time complexity:
 * O(`size` * `NA.header_len`)
 *
 * This function has the potential to recure indefinitely at the moment if
 * a cyclical reference is encountered. It probably shouldn't though.
 *
 * `used_blocks` - The array in which to store whether a block is alive.
 *     The indicies of the array should correspond to the indicies of the
 *     headers in `NA`
 * `buf` - The buffer in which to search for pointers
 * `size` - The number of potential pointers in `buf`
 */
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

void mark_stack(bool used_blocks[NA.header_len]) {
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
    // printf("stack size: %d\ntos: %po\n", stack_size, (void*)top_of_stack);
    mark_used_blocks_by_ptrs_in_buffer(used_blocks, (uintptr_t*)top_of_stack,
                                       stack_size);
}

// TODO
// Implement searching and marking through other sections
void mark_bss(bool used_blocks[NA.header_len]) {}

void mark_registers(bool used_blocks[NA.header_len]) {}

void sweep(bool used_blocks[NA.header_len]) {
    for (int i = 0; i < NA.header_len; i++) {
        if (used_blocks[i])
            continue;

        // TODO
        // Check if this modified `NA->headers` (it should)
        Block* header = NA.headers + i;
        header->free = true;
        header->size += header->offset;
        header->offset = 0;
        // TODO
        // Merge everything at the end instead
        // try_merge_block(i);
    }
}

// If they happen to have the same number that they don't mean as a pointer,
// then we have a false positive, which is fine
//
// Also, they could modify their pointer with the intention of obfuscating it
// from us, we're not going to worry about this case
void garbage_collect() {
    bool used_blocks[NA.header_len];
    memset(used_blocks, 0, NA.header_len);

    mark_stack(used_blocks);

    sweep(used_blocks);
}
