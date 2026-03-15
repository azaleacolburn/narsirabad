#include "bl.h"
#include "alloc.h"
#include "mem.h"

#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

BlockList BL_new() {
    size_t page_size = getpagesize();
    void* mapping = map_new(page_size);
    if (mapping == NULL)
        exit(1);

    BlockList list;
    list.arr = mapping;
    list.len = 0;
    list.cap = page_size / sizeof(Block);

    return list;
}

void BL_realloc(BlockList* list) {
    void* new_mapping = map_new(list->cap * 2 * sizeof(Block));
    if (new_mapping == NULL)
        exit(1);

    memmove(new_mapping, list->arr, list->len * sizeof(Block));
    munmap(list->arr, list->cap * sizeof(Block));

    list->cap *= 2;
    list->arr = new_mapping;
}

Block* BL_idx(BlockList* list, size_t idx) {
    if (list->len <= idx) {
        return NULL;
    }

    return &list->arr[idx];
}

void BL_remove(BlockList* list, size_t idx) {
    if (list->len <= idx) {
        return;
    }

    memset(list->arr + idx, 0, sizeof(Block));
    memmove(list->arr + idx, list->arr + idx + 1,
            (list->len - idx - 1) * sizeof(Block));

    list->len--;
}

void BL_free(BlockList* list) {
    int8_t unmap_result = munmap(list->arr, list->cap * sizeof(Block));
    if (unmap_result == -1) {
        exit(1);
    }

    list->arr = NULL;
    list->len = 0;
    list->cap = 0;
}

// TODO
// This function exists because otherwise we'd have to create a `Block`
// then pass it into the push function, which is slow
Block* BL_new_header(BlockList* list, size_t size, void* ptr) {
    if (list->len == list->cap) {
        BL_realloc(list);
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
        BL_realloc(list);
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

bool BL_find_remove(BlockList* list, Block* block) {
    int idx = BL_find(list, block);
    if (idx == -1)
        return false;

    BL_remove(list, idx);

    return true;
}
