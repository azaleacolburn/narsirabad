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

    Block* clear_address = list->arr + idx;
    size_t remaining_bytes = (list->len - idx - 1) * sizeof(Block);

    memset(clear_address, 0, sizeof(Block));
    memmove(clear_address, clear_address + 1, remaining_bytes);

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
size_t BL_new_header(BlockList* list, size_t size, void* ptr) {
    if (list->len == list->cap) {
        BL_realloc(list);
    }

    Block* next_header = list->arr + list->len;
    next_header->size = size;
    next_header->ptr = ptr;
    next_header->offset = 0;

    return list->len++;
}

void BL_push(BlockList* list, Block block) {
    if (list->len == list->cap) {
        BL_realloc(list);
    }

    list->arr[list->len++] = block;
}

size_t BL_find(BlockList* list, Block* block) {
    for (size_t idx = 0; idx < list->len; idx++)
        if (list->arr[idx].ptr == block)
            return idx;

    return -1;
}

bool BL_find_remove(BlockList* list, Block* block) {
    int idx = BL_find(list, block);
    if (idx == -1)
        return false;

    BL_remove(list, idx);

    return true;
}
