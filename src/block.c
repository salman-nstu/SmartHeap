/*
 * SmartHeap - Thread-Safe Custom Memory Allocator
 * block.c - Block Operations Implementation
 *
 * Manages the doubly-linked list of memory blocks.
 * Implements splitting (to reduce internal fragmentation)
 * and coalescing (to reduce external fragmentation).
 */

#include "block.h"
#include "strategy.h"
#include <string.h>

void block_init(header_t *header, size_t size, unsigned is_free,
                header_t *prev, header_t *next)
{
    memset(header, 0, sizeof(header_t));
    header->s.magic     = SH_MAGIC;
    header->s.size      = size;
    header->s.is_free   = is_free;
    header->s.prev      = prev;
    header->s.next      = next;
    header->s.alloc_file = NULL;
    header->s.alloc_line = 0;
    header->s.alloc_id   = 0;
}

int block_validate(header_t *header)
{
    if (!header)
        return 0;
    if (header->s.magic != SH_MAGIC && header->s.magic != SH_FREED_MAGIC)
        return 0;
    return 1;
}

void block_split(sh_state_t *state, header_t *header, size_t size)
{
    size_t remaining;
    header_t *new_block;

    /*
     * Only split if the remainder is large enough to form a useful block.
     * We need space for a new header plus at least SH_MIN_BLOCK_SIZE bytes.
     */
    if (header->s.size < size + sizeof(header_t) + SH_MIN_BLOCK_SIZE)
        return;

    remaining = header->s.size - size - sizeof(header_t);

    /*
     * Create the new free block right after the allocated portion:
     *   [header][allocated size bytes][new_header][remaining bytes]
     */
    new_block = (header_t *)((char *)HEADER_TO_PTR(header) + size);
    block_init(new_block, remaining, 1, header, header->s.next);

    /* Update the next block's prev pointer */
    if (header->s.next)
        header->s.next->s.prev = new_block;

    /* Update the current block */
    header->s.next = new_block;
    header->s.size = size;

    if (state)
        state->split_count++;
}

header_t *block_coalesce(sh_state_t *state, header_t *header)
{
    header_t *result = header;

    if (!header || !header->s.is_free)
        return header;

    /*
     * Merge with next block if it's free.
     * Absorb its size + header overhead into the current block.
     */
    if (header->s.next && header->s.next->s.is_free) {
        header->s.size += sizeof(header_t) + header->s.next->s.size;

        /* Skip over the merged block in the linked list */
        header_t *after = header->s.next->s.next;
        header->s.next = after;
        if (after)
            after->s.prev = header;

        if (state)
            state->coalesce_count++;
    }

    /*
     * Merge with previous block if it's free.
     * The previous block absorbs the current block.
     */
    if (header->s.prev && header->s.prev->s.is_free) {
        header->s.prev->s.size += sizeof(header_t) + header->s.size;

        /* Skip over the current block in the linked list */
        header->s.prev->s.next = header->s.next;
        if (header->s.next)
            header->s.next->s.prev = header->s.prev;

        result = header->s.prev;

        if (state)
            state->coalesce_count++;
    }

    return result;
}

header_t *block_find(sh_state_t *state, size_t size, sh_strategy_t strategy)
{
    switch (strategy) {
    case SH_FIRST_FIT:
        return strategy_first_fit(state, size);
    case SH_BEST_FIT:
        return strategy_best_fit(state, size);
    case SH_WORST_FIT:
        return strategy_worst_fit(state, size);
    default:
        return strategy_first_fit(state, size);
    }
}
