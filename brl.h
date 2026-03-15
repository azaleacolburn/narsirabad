#include "alloc.h"

BlockRefList BRL_new();

Block* BRL_idx(BlockRefList* list, size_t idx);

void BRL_push(BlockRefList* list, Block* block);

void BRL_remove(BlockRefList* list, size_t idx);

int BRL_find(BlockRefList* list, Block* block);

bool BRL_find_remove(BlockRefList* list, Block* block);

void BRL_realloc(BlockRefList* list);

void BRL_free(BlockRefList* list);
