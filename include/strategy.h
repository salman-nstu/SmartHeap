/*
 * SmartHeap - Thread-Safe Custom Memory Allocator
 * strategy.h - Allocation Strategy Interface
 *
 * Implements First Fit, Best Fit, and Worst Fit search algorithms
 * for finding free blocks in the heap.
 */

#ifndef SMARTHEAP_STRATEGY_H
#define SMARTHEAP_STRATEGY_H

#include "smartheap_internal.h"

/*
 * Search for a free block using First Fit strategy.
 * Returns the first block that is large enough.
 * Fast but may cause more fragmentation.
 *
 * @param state  Global allocator state
 * @param size   Required size in bytes
 * @return       First suitable free block, or NULL
 */
header_t *strategy_first_fit(sh_state_t *state, size_t size);

/*
 * Search for a free block using Best Fit strategy.
 * Returns the smallest block that is large enough.
 * Less internal waste but requires full scan.
 *
 * @param state  Global allocator state
 * @param size   Required size in bytes
 * @return       Best fitting free block, or NULL
 */
header_t *strategy_best_fit(sh_state_t *state, size_t size);

/*
 * Search for a free block using Worst Fit strategy.
 * Returns the largest free block available.
 * Leaves larger remainders but causes most fragmentation.
 *
 * @param state  Global allocator state
 * @param size   Required size in bytes
 * @return       Largest suitable free block, or NULL
 */
header_t *strategy_worst_fit(sh_state_t *state, size_t size);

/*
 * Get the name of a strategy as a string.
 *
 * @param strategy  The strategy enum value
 * @return          Human-readable name string
 */
const char *strategy_name(sh_strategy_t strategy);

#endif /* SMARTHEAP_STRATEGY_H */
