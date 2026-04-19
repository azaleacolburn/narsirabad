#ifndef NARSIRABAD_VEC
#define NARSIRABAD_VEC

#include "alloc.h"
#include <stddef.h>

/*
 * `BlockList` functions
 */

BlockList BL_new();

size_t BL_new_header(BlockList* list, size_t size, void* ptr);

void BL_push(BlockList* list, Block block);

Block* BL_idx(BlockList* list, size_t idx);

size_t BL_find(BlockList* list, Block* block);

bool BL_find_remove(BlockList* list, Block* value);

void BL_remove(BlockList* list, size_t idx);

void BL_free(BlockList* list);

#endif
