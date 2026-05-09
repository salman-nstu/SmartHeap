/*
 * SmartHeap - Thread-Safe Custom Memory Allocator
 * arena.c - Arena Manager Implementation
 *
 * Manages large memory chunks obtained from the OS.
 * Each arena is a contiguous block of memory from which
 * individual allocations are carved out.
 */

#include "arena.h"
#include "block.h"
#include "platform.h"
#include <string.h>

arena_t *arena_create(sh_state_t *state, size_t min_size)
{
    size_t arena_size;
    size_t usable_size;
    arena_t *arena;
    header_t *first_block;

    /*
     * Calculate arena size: we need space for the arena struct itself
     * (stored at the beginning of the allocation) plus at least min_size
     * bytes for user data plus a header for the first block.
     * Use the default SH_ARENA_SIZE if it's large enough.
     */
    size_t needed = sizeof(arena_t) + sizeof(header_t) + min_size;
    arena_size = (needed > SH_ARENA_SIZE) ? needed : SH_ARENA_SIZE;

    /* Round up to page boundary and allocate from OS */
    arena_size = platform_round_to_page(arena_size);
    void *raw = platform_alloc(arena_size);
    if (!raw)
        return NULL;

    /*
     * Layout within the arena:
     *   [arena_t struct][header_t + free block spanning rest of arena]
     *
     * The arena_t struct is stored at the very beginning.
     * The first block header follows immediately after.
     */
    arena = (arena_t *)raw;
    memset(arena, 0, sizeof(arena_t));
    arena->base = raw;
    arena->total_size = arena_size;
    arena->used_size = sizeof(arena_t);
    arena->next = NULL;

    /* Calculate usable space after the arena struct */
    usable_size = arena_size - sizeof(arena_t) - sizeof(header_t);

    /* Initialize the first block: a single large free block */
    first_block = (header_t *)((char *)raw + sizeof(arena_t));
    block_init(first_block, usable_size, 1, NULL, NULL);
    arena->first_block = first_block;

    /* Add to the global arena list (prepend) */
    arena->next = state->arenas;
    state->arenas = arena;
    state->arena_count++;
    state->total_arena_memory += arena_size;

    return arena;
}

void arena_destroy(sh_state_t *state, arena_t *arena)
{
    arena_t *prev = NULL;
    arena_t *curr = state->arenas;

    /* Find and remove from the linked list */
    while (curr) {
        if (curr == arena) {
            if (prev)
                prev->next = curr->next;
            else
                state->arenas = curr->next;

            state->arena_count--;
            state->total_arena_memory -= arena->total_size;

            /* Return memory to OS */
            platform_free(arena->base, arena->total_size);
            return;
        }
        prev = curr;
        curr = curr->next;
    }
}

arena_t *arena_find(sh_state_t *state, void *ptr)
{
    arena_t *arena = state->arenas;
    while (arena) {
        if (IN_ARENA(arena, ptr))
            return arena;
        arena = arena->next;
    }
    return NULL;
}

int arena_is_empty(arena_t *arena)
{
    header_t *block = arena->first_block;
    while (block) {
        if (!block->s.is_free)
            return 0;
        block = block->s.next;
    }
    return 1;
}
