/*
 * SmartHeap - Thread-Safe Custom Memory Allocator
 * arena.h - Arena Manager Interface
 *
 * Arenas are large chunks of memory obtained from the OS.
 * The allocator carves individual blocks from arenas, amortizing
 * the cost of OS system calls.
 */

#ifndef SMARTHEAP_ARENA_H
#define SMARTHEAP_ARENA_H

#include "smartheap_internal.h"

/*
 * Create a new arena with at least `min_size` usable bytes.
 * The actual arena may be larger (rounded up to page boundary).
 *
 * @param state     Global allocator state
 * @param min_size  Minimum usable size needed
 * @return          Pointer to the new arena, or NULL on failure
 */
arena_t *arena_create(sh_state_t *state, size_t min_size);

/*
 * Destroy an arena and return its memory to the OS.
 * The arena must be fully freed (no allocated blocks).
 *
 * @param state  Global allocator state
 * @param arena  The arena to destroy
 */
void arena_destroy(sh_state_t *state, arena_t *arena);

/*
 * Find the arena that contains the given pointer.
 *
 * @param state  Global allocator state
 * @param ptr    A pointer to user data (or header)
 * @return       The arena containing ptr, or NULL if not found
 */
arena_t *arena_find(sh_state_t *state, void *ptr);

/*
 * Check if an arena is completely free (all blocks marked free).
 *
 * @param arena  The arena to check
 * @return       1 if entirely free, 0 otherwise
 */
int arena_is_empty(arena_t *arena);

#endif /* SMARTHEAP_ARENA_H */
