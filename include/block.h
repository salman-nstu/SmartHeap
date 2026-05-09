/*
 * SmartHeap - Thread-Safe Custom Memory Allocator
 * block.h - Block Operations Interface
 *
 * Manages the doubly-linked list of memory blocks within arenas.
 * Provides splitting, coalescing, and search operations.
 */

#ifndef SMARTHEAP_BLOCK_H
#define SMARTHEAP_BLOCK_H

#include "smartheap_internal.h"

/*
 * Split a free block into two: one of the requested size (marked used),
 * and a remainder block (marked free).
 *
 * Only splits if the remainder would be large enough to be useful
 * (at least SH_MIN_BLOCK_SIZE + sizeof(header_t)).
 *
 * @param state   Global allocator state
 * @param header  The free block to split
 * @param size    The requested allocation size (aligned)
 */
void block_split(sh_state_t *state, header_t *header, size_t size);

/*
 * Coalesce a free block with adjacent free neighbors.
 * Merges with the next block if free, then with the previous block if free.
 * Reduces external fragmentation.
 *
 * @param state   Global allocator state
 * @param header  The free block to coalesce around
 * @return        The merged block header (may differ from input if merged with prev)
 */
header_t *block_coalesce(sh_state_t *state, header_t *header);

/*
 * Find a free block that can accommodate the given size,
 * using the specified allocation strategy.
 *
 * @param state     Global allocator state
 * @param size      Required size in bytes (already aligned)
 * @param strategy  The search strategy to use
 * @return          A suitable free block header, or NULL if none found
 */
header_t *block_find(sh_state_t *state, size_t size, sh_strategy_t strategy);

/*
 * Initialize a new block header at the given location.
 *
 * @param header    Pointer to the header location
 * @param size      Usable size of the block (excluding header)
 * @param is_free   Whether the block starts as free
 * @param prev      Previous block in the list (or NULL)
 * @param next      Next block in the list (or NULL)
 */
void block_init(header_t *header, size_t size, unsigned is_free,
                header_t *prev, header_t *next);

/*
 * Validate a block header's magic number.
 *
 * @param header  The header to validate
 * @return        1 if valid, 0 if corrupted
 */
int block_validate(header_t *header);

#endif /* SMARTHEAP_BLOCK_H */
