/*
 * SmartHeap - Thread-Safe Custom Memory Allocator
 * strategy.c - Allocation Strategy Implementations
 *
 * Three classic allocation strategies for finding free blocks:
 *   1. First Fit  - Fast, returns the first block that fits
 *   2. Best Fit   - Efficient, returns the smallest fitting block
 *   3. Worst Fit  - Returns the largest block (experimental)
 */

#include "strategy.h"

header_t *strategy_first_fit(sh_state_t *state, size_t size)
{
    arena_t *arena = state->arenas;

    while (arena) {
        header_t *curr = arena->first_block;
        while (curr) {
            if (curr->s.is_free && curr->s.size >= size)
                return curr;
            curr = curr->s.next;
        }
        arena = arena->next;
    }
    return NULL;
}

header_t *strategy_best_fit(sh_state_t *state, size_t size)
{
    header_t *best = NULL;
    arena_t *arena = state->arenas;

    while (arena) {
        header_t *curr = arena->first_block;
        while (curr) {
            if (curr->s.is_free && curr->s.size >= size) {
                /* Perfect fit — can't do better than this */
                if (curr->s.size == size)
                    return curr;
                /* Better than current best? */
                if (!best || curr->s.size < best->s.size)
                    best = curr;
            }
            curr = curr->s.next;
        }
        arena = arena->next;
    }
    return best;
}

header_t *strategy_worst_fit(sh_state_t *state, size_t size)
{
    header_t *worst = NULL;
    arena_t *arena = state->arenas;

    while (arena) {
        header_t *curr = arena->first_block;
        while (curr) {
            if (curr->s.is_free && curr->s.size >= size) {
                if (!worst || curr->s.size > worst->s.size)
                    worst = curr;
            }
            curr = curr->s.next;
        }
        arena = arena->next;
    }
    return worst;
}

const char *strategy_name(sh_strategy_t strategy)
{
    switch (strategy) {
    case SH_FIRST_FIT: return "First Fit";
    case SH_BEST_FIT:  return "Best Fit";
    case SH_WORST_FIT: return "Worst Fit";
    default:           return "Unknown";
    }
}
