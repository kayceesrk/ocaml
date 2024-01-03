#ifndef GC_H_
#define GC_H_

#include "internal/alias.h"

// This takes in the first and last ptr of free list(because we want starting
// and stopping point of free list while sweeping)
void mark_and_sweep(uint64_t heap_start, uint64_t heap_end);

#endif // GC_H_
