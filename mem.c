#include "mem.h"
#include <stdio.h>
#include <sys/mman.h>

#define PROT PROT_READ | PROT_WRITE | PROT_EXEC
#define MAP MAP_SHARED | MAP_ANONYMOUS

void* map_fixed(void* ptr, intptr_t size) {
    return mmap(ptr, size, PROT, MAP | MAP_FIXED, 0, 0);
}

void* map_new(intptr_t size) { return mmap(NULL, size, PROT, MAP, 0, 0); }
