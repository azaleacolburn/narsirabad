#ifndef NARSIRABAD_VEC
#define NARSIRABAD_VEC

#include "alloc.h"
#include <stddef.h>

/*
 * `BlockList` functions
 */

BlockList BL_new();

void BL_push(BlockList* list, Block block);

Block* BL_idx(BlockList* list, size_t idx);

bool BL_find_remove(BlockList* list, Block* value);

void BL_remove(BlockList* list, size_t idx);

void BL_free(BlockList* list);

/*
 * `BlockRefList` functions
 */

BlockRefList BRL_new();

Block* BL_new_header(BlockList* list, size_t size, void* ptr);

void BRL_push(BlockRefList* list, Block* block);

Block* BRL_idx(BlockRefList* list, size_t idx);

int BRL_find(BlockRefList* list, Block* block);

void BRL_remove(BlockRefList* list, size_t idx);

bool BRL_find_remove(BlockRefList* list, Block* value);

void BRL_free(BlockRefList* list);

#endif
