#include "brl.h"
#include "alloc.h"
#include "mem.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

extern Allocator NARSIRABAD_ALLOCATOR;
#define NA NARSIRABAD_ALLOCATOR

#define SIZE sizeof(size_t)

BlockRefList BRL_new() {
    size_t page_size = getpagesize();
    void* mapping = map_new(page_size);
    if (mapping == NULL)
        exit(1);

    BlockRefList list;
    list.arr = mapping;
    list.len = 0;
    list.cap = page_size / SIZE;

    return list;
}

void BRL_realloc(BlockRefList* list) {
    void* new_mapping = map_new(list->cap * 2 * SIZE);
    if (new_mapping == NULL)
        exit(1);

    // NOTE
    // `list->len == list->cap` is probably true here
    // but it's still best to not copy more than we have to
    // in case there are other circumstances
    memmove(new_mapping, list->arr, list->len * SIZE);
    munmap(list->arr, list->cap * SIZE);

    list->cap *= 2;
    list->arr = new_mapping;
}

Block* BRL_idx(BlockRefList* list, size_t idx) {
    if (idx >= list->len) {
        return NULL;
    }

    return &NA.headers.arr[list->arr[idx]];
}

void BRL_remove(BlockRefList* list, size_t idx) {
    if (list->len <= idx) {
        return;
    }

    size_t* clear_address = list->arr + idx;
    size_t remaining_bytes = (list->len - idx - 1) * SIZE;

    memset(clear_address, 0, SIZE);
    memmove(clear_address, clear_address + 1, remaining_bytes);

    list->len--;
}

int BRL_find(BlockRefList* list, void* buf) {
    for (int idx = 0; idx < list->len; idx++)
        if (NA.headers.arr[list->arr[idx]].ptr == buf)
            return idx;

    return -1;
}

bool BRL_find_remove(BlockRefList* list, void* buf) {
    int idx = BRL_find(list, buf);
    if (idx == -1)
        return false;

    BRL_remove(list, idx);

    return true;
}

void BRL_push(BlockRefList* list, size_t bl_idx) {
    if (list->len == list->cap) {
        BRL_realloc(list);
    }

    list->arr[list->len++] = bl_idx;
}

void BRL_push_block(BlockRefList* list, Block* block) {
    size_t idx = BRL_find(list, block);
    BRL_push(list, idx);
}

void BRL_free(BlockRefList* list) {
    int8_t unmap_result = munmap(list->arr, list->cap * SIZE);
    if (unmap_result == -1) {
        sprintf(stderr, "Failed to unmap BlockRefList: %po", list);
        exit(1);
    }

    list->arr = NULL;
    list->len = 0;
    list->cap = 0;
}
