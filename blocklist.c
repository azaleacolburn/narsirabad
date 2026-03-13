#include "blocklist.h"
#include "alloc.h"
#include "mem.h"
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

// TODO
// This is super ugly, just split things into two files and accept that we'll
// re-implement stuff I really wish I had a generics system here
//
// The reason I don't just use a vector that holds a `void*` is because I would
// like an obvious differentiation between lists of owned blocks and lists of
// references to blocks
#define VEC_GEN_new(name, outer_type, inner_type)                              \
    outer_type name(size_t initial_capacity) {                                 \
        void* mapping = map_new(initial_capacity * sizeof(inner_type));        \
        if (mapping == NULL)                                                   \
            exit(1);                                                           \
                                                                               \
        outer_type list;                                                       \
        list.arr = mapping;                                                    \
        list.len = 0;                                                          \
        list.cap = initial_capacity;                                           \
                                                                               \
        return list;                                                           \
    }

#define VEC_GEN_realloc(name, outer_type, inner_type)                          \
    void name(outer_type* list, size_t new_cap) {                              \
        void* new_mapping = map_new(new_cap * sizeof(inner_type));             \
        if (new_mapping == NULL)                                               \
            exit(1);                                                           \
                                                                               \
        memmove(new_mapping, list->arr, list->len * sizeof(inner_type));       \
        munmap(list->arr, list->cap * sizeof(inner_type));                     \
                                                                               \
        list->cap = new_cap;                                                   \
        list->arr = new_mapping;                                               \
    }

#define VEC_GEN_idx(name, outer_type, ref_symbol)                              \
    Block* name(outer_type* list, size_t idx) {                                \
        if (list->len <= idx) {                                                \
            return NULL;                                                       \
        }                                                                      \
                                                                               \
        return ref_symbol list->arr[idx];                                      \
    }

#define VEC_GEN_remove(name, outer_type, inner_type)                           \
    void name(outer_type* list, size_t idx) {                                  \
        if (list->len <= idx) {                                                \
            return;                                                            \
        }                                                                      \
                                                                               \
        memset(list->arr + idx, 0, sizeof(inner_type));                        \
        memmove(list->arr + idx, list->arr + idx + 1,                          \
                (list->len - idx - 1) * sizeof(inner_type));                   \
                                                                               \
        list->len--;                                                           \
    }

#define VEC_GEN_free(name, outer_type, inner_type)                             \
    void name(outer_type* list) {                                              \
        int8_t unmap_result =                                                  \
            munmap(list->arr, list->cap * sizeof(inner_type));                 \
        if (unmap_result == -1) {                                              \
            exit(1);                                                           \
        }                                                                      \
                                                                               \
        list->arr = NULL;                                                      \
        list->len = 0;                                                         \
        list->cap = 0;                                                         \
    }

// TODO
// Figure out what the fuck is going on with the formatting here
VEC_GEN_new(BL_new, BlockList, Block)
    VEC_GEN_realloc(BL_realloc, BlockList, Block)
        VEC_GEN_idx(BL_idx, BlockList, &)
            VEC_GEN_remove(BL_remove, BlockList, Block)
                VEC_GEN_free(BL_free, BlockList, Block)

                    VEC_GEN_new(BRL_new, BlockRefList, Block*)
                        VEC_GEN_realloc(BRL_realloc, BlockRefList, Block*)
                            VEC_GEN_idx(BRL_idx, BlockRefList, )
                                VEC_GEN_remove(BRL_remove, BlockRefList, Block*)
                                    VEC_GEN_free(BRL_free, BlockRefList, Block*)

    // NOTE
    // This function exists because otherwise we'd have to create a `Block`
    // then pass it into the push function, which is slow
    Block* BL_new_header(BlockList* list, size_t size, void* ptr) {
    if (list->len == list->cap) {
        BL_realloc(list, list->cap * 2);
    }

    Block* next_header = list->arr + list->len;
    next_header->size = size;
    next_header->ptr = ptr;
    next_header->offset = 0;

    list->len++;

    return next_header;
}

/*
 * NOTE
 * Putting this logic in a macro is annoying because we'd have to make
 * conditional function calls.
 */
void BL_push(BlockList* list, Block block) {
    if (list->len == list->cap) {
        BL_realloc(list, list->cap * 2);
    }

    list->arr[list->len++] = block;
}

int BL_find(BlockList* list, Block* block) {
    int idx = 0;
    while (idx < list->len) {
        if (list->arr + idx == block)
            break;

        idx++;
    }

    return idx;
}

void BL_find_remove(BlockList* list, Block* block) {
    int idx = BL_find(list, block);
    if (idx == -1)
        return;

    BL_remove(list, idx);
}

void BRL_push(BlockRefList* list, Block* block) {
    if (list->len == list->cap) {
        BRL_realloc(list, list->cap * 2);
    }

    list->arr[list->len++] = block;
}

int BRL_find(BlockRefList* list, Block* block) {
    int idx = 0;
    while (idx < list->len) {
        if (list->arr[idx] == block)
            break;

        idx++;
    }

    return idx;
}

void BRL_find_remove(BlockRefList* list, Block* block) {
    int idx = BRL_find(list, block);
    if (idx == -1)
        return;

    BRL_remove(list, idx);
}
