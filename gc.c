#include "alloc.h"
#include "vec.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>

#define NA NARSIRABAD_ALLOCATOR

extern Allocator NARSIRABAD_ALLOCATOR;

extern uintptr_t top_of_stack;
extern uintptr_t bottom_of_stack;

extern uintptr_t start_of_bss;
extern uintptr_t end_of_bss;

/*
 * Finds the block corresponding with the given pointer (the pointer must
 * point to the beginning of the block)
 * Returns the index of the block in `NA.headers`, or `-1` if it could not
 * be found
 */
int8_t find_corresponding_block(void* ptr) {
    for (int i = 0; i < NA.used_headers.len; i++) {
        Block header = *BRL_idx(&NA.used_headers, i);

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
void mark_used_blocks_by_ptrs_in_buffer(bool used_blocks[NA.used_headers.len],
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
        if (block_idx == -1)
            continue;

        if (used_blocks[block_idx])
            continue;

        Block header = *BRL_idx(&NA.used_headers, block_idx);
        mark_used_blocks_by_ptrs_in_buffer(used_blocks, header.ptr,
                                           header.size);

        used_blocks[block_idx] = true;
    }
}

void mark_stack(bool used_blocks[NA.used_headers.len]) {
    // Align `top_of_stack` to `8`
    uintptr_t diff = (uintptr_t)top_of_stack % 8;
    if (diff != 0) {
        top_of_stack += 8 - diff;
    }

    // Yes, this is correct
    assert(top_of_stack < bottom_of_stack);

    /// The number of pointers that can exist in the current buffer
    /// Rounded down because the top the the stack might not be aligned to
    /// `8`, but the bottom will be
    size_t stack_size = ((uintptr_t)bottom_of_stack - (uintptr_t)top_of_stack) /
                        sizeof(uintptr_t);
    mark_used_blocks_by_ptrs_in_buffer(used_blocks, (uintptr_t*)top_of_stack,
                                       stack_size);
}

// TODO
// Implement searching and marking through other sections
void mark_bss(bool used_blocks[NA.used_headers.len]) {
    // NOTE
    // No need to align the bottom of the bss
    // I think?

    // TODO Verify that this is correct
    assert(start_of_bss > end_of_bss);

    // printf("Start of .BSS: %po\n  End of .BSS: %po\n\n", (void*)start_of_bss,
    //        (void*)end_of_bss);

    size_t stack_size = start_of_bss - end_of_bss;
    mark_used_blocks_by_ptrs_in_buffer(used_blocks, (uintptr_t*)end_of_bss,
                                       stack_size);
}

/*
 * Loops through the registers the system will likely be using, marking all the
 pointers found in both those registers and the `NARSIRABAD_ALLOCATOR`
 *
 * List of registers:
 * https://cs.brown.edu/courses/cs033/docs/guides/x64_cheatsheet.pdf
 *
 *
 * We are not checking the following registers:
 * - `rsp`: Stack Pointer
 * - `rbp`: Stack Frame Base Pointer
 *
 * This has to be a macro, because the assembly code has to be generated at
 * compile time.
 */
void mark_registers(bool used_blocks[NA.used_headers.len]) {

// TODO
// Maybe move this to the top of the file
#define CHECK_REG(r)                                                           \
    {                                                                          \
        register register_t v asm(#r);                                         \
        int8_t block_number = find_corresponding_block((void*)v);              \
        if (block_number != -1) {                                              \
            used_blocks[block_number] = true;                                  \
        }                                                                      \
    }

    CHECK_REG(rax)
    CHECK_REG(rcx)
    CHECK_REG(rsi)
    CHECK_REG(rdi)
    CHECK_REG(r8)
    CHECK_REG(r9)
    CHECK_REG(r10)
    CHECK_REG(r11)
    CHECK_REG(r12)
    CHECK_REG(r13)
    CHECK_REG(r14)
    CHECK_REG(r15)
}

void sweep(bool used_blocks[NA.used_headers.len]) {
    for (int i = 0; i < NA.used_headers.len; i++) {
        if (used_blocks[i])
            continue;

        // Equivalent to `free_block(header)`
        Block* header = BRL_idx(&NA.used_headers, i);

        header->size += header->offset;
        header->offset = 0;

        BRL_find_remove(&NA.used_headers, header);
        BRL_push(&NA.free_headers, header);
    }
}

// If they happen to have the same number that they don't mean as a pointer,
// then we have a false positive, which is fine
//
// Also, they could modify their pointer with the intention of obfuscating it
// from us, we're not going to worry about this case
void garbage_collect() {
    bool used_blocks[NA.used_headers.len];
    memset(used_blocks, 0, NA.used_headers.len);

    mark_stack(used_blocks);
    mark_bss(used_blocks);
    mark_registers(used_blocks);

    sweep(used_blocks);
}
