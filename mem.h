// Azalea Colburn, 2026
// Memory mapping wrapper library
// Provides a wrapper around mmap and munmap
#include <stdint.h>

void* map_fixed(void* ptr, intptr_t size);
void* map_new(intptr_t size);
