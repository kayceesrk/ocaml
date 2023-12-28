#ifndef GC_H_
#define GC_H_

#include "internal/alias.h"

/* #define MAXIMUM_NO_OF_ROOTS 10000 */

/* extern uint64_t roots[MAXIMUM_NO_OF_ROOTS]; */
/* extern uint64_t num_of_roots; */

// This takes in the first and last ptr of free list(because we want starting
// and stopping point of free list while sweeping)
void mark_and_sweep(uint64_t heap_start, uint64_t heap_end);

// This registers a root to our own maintained root list(not maintained by
// runtime)
void register_root(uint64_t root);

#endif // GC_H_
