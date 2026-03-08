#ifndef NARSIRABAD_GC
#define NARSIRABAD_GC
#include "alloc.h"
#include <stdint.h>

/// If they happen to have the same number that they don't mean as a pointer,
/// then we have a false positive, which is fine
///
/// Also, they could modify their pointer with the intention of obfuscating it
/// from us, we're not going to worry about this case
void garbage_collect();
#endif
