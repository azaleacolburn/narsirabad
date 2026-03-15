#include "brl.h"
#include "alloc.h"
#include "mem.h"

#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

BlockRefList BRL_new() {
    size_t page_size = getpagesize();
    void* mapping = map_new(page_size);
    if (mapping == NULL)
        exit(1);

    BlockRefList list;
    list.arr = mapping;
    list.len = 0;
    list.cap = page_size / sizeof(Block);

    return list;
}

void BRL_realloc(BlockRefList* list) {
    void* new_mapping = map_new(list->cap * 2 * sizeof(Block*));
    if (new_mapping == NULL)
        exit(1);

    memmove(new_mapping, list->arr, list->len * sizeof(Block*));
    munmap(list->arr, list->cap * sizeof(Block*));

    list->cap *= 2;
    list->arr = new_mapping;
}

Block* BRL_idx(BlockRefList* list, size_t idx) {
    if (list->len <= idx) {
        return NULL;
    }

    return list->arr[idx];
}

void BRL_remove(BlockRefList* list, size_t idx) {
    if (list->len <= idx) {
        return;
    }

    memset(list->arr + idx, 0, sizeof(Block*));
    memmove(list->arr + idx, list->arr + idx + 1,
            (list->len - idx - 1) * sizeof(Block*));

    list->len--;
}

void BRL_free(BlockRefList* list) {
    int8_t unmap_result = munmap(list->arr, list->cap * sizeof(Block*));
    if (unmap_result == -1) {
        exit(1);
    }

    list->arr = NULL;
    list->len = 0;
    list->cap = 0;
}

void BRL_push(BlockRefList* list, Block* block) {
    if (list->len == list->cap) {
        BRL_realloc(list);
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

bool BRL_find_remove(BlockRefList* list, Block* block) {
    int idx = BRL_find(list, block);
    if (idx == -1)
        return false;

    BRL_remove(list, idx);

    return true;
}
