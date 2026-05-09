/*
 * SmartHeap - Thread-Safe Custom Memory Allocator
 * visualizer.h - Heap Visualization Interface
 *
 * Provides terminal-based visualization of the heap state
 * using ANSI colors and Unicode box-drawing characters.
 */

#ifndef SMARTHEAP_VISUALIZER_H
#define SMARTHEAP_VISUALIZER_H

#include "smartheap_internal.h"

/*
 * Print a visual map of all arenas and their blocks to stdout.
 * Uses ANSI colors: green for free blocks, red for used blocks.
 *
 * @param state  Global allocator state
 */
void visualizer_print_map(sh_state_t *state);

/*
 * Print a compact one-line summary of each arena.
 *
 * @param state  Global allocator state
 */
void visualizer_print_summary(sh_state_t *state);

/*
 * Print detailed information about a specific block.
 *
 * @param header  The block header to inspect
 */
void visualizer_print_block(header_t *header);

/*
 * Print the full linked list of blocks (debug view).
 *
 * @param state  Global allocator state
 */
void visualizer_print_list(sh_state_t *state);

#endif /* SMARTHEAP_VISUALIZER_H */
