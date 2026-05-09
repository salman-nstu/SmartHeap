/*
 * SmartHeap - Custom Memory Allocator (Windows)
 * smartheap.c - Core Allocator Implementation
 *
 * Implements sh_malloc(), sh_free(), sh_calloc(), sh_realloc()
 * with thread safety via Windows CRITICAL_SECTION.
 *
 * Architecture:
 *   1. sh_malloc requests a block from the free list (via strategy)
 *   2. If found, splits if necessary and returns
 *   3. If not found, creates a new arena from the OS
 *   4. sh_free marks the block free and coalesces neighbors
 *   5. If an entire arena becomes free, releases it to the OS
 */

#include "smartheap.h"
#include "smartheap_internal.h"
#include "arena.h"
#include "block.h"
#include "strategy.h"
#include "visualizer.h"
#include "stats.h"
#include "leak_detector.h"
#include "platform.h"

#include <string.h>
#include <stdio.h>
#include <windows.h>

/* ── Global State ────────────────────────────────────────────── */

static sh_state_t g_state;
static CRITICAL_SECTION g_lock;
static volatile LONG g_lock_initialized = 0;

/* ── Internal Helpers ────────────────────────────────────────── */

static void ensure_lock_init(void)
{
    /* Thread-safe one-time initialization of the critical section */
    if (InterlockedCompareExchange(&g_lock_initialized, 1, 0) == 0) {
        InitializeCriticalSection(&g_lock);
    }
}

static void lock(void)
{
    ensure_lock_init();
    EnterCriticalSection(&g_lock);
}

static void unlock(void)
{
    LeaveCriticalSection(&g_lock);
}

/* ── Initialization & Cleanup ────────────────────────────────── */

int sh_init(void)
{
    lock();
    if (g_state.initialized) {
        unlock();
        return 0; /* Already initialized */
    }

    memset(&g_state, 0, sizeof(sh_state_t));
    g_state.strategy = SH_FIRST_FIT;
    g_state.next_alloc_id = 1;
    g_state.initialized = 1;

    unlock();
    return 0;
}

void sh_destroy(void)
{
    lock();

    /* Free all arenas back to the OS */
    arena_t *arena = g_state.arenas;
    while (arena) {
        arena_t *next = arena->next;
        platform_free(arena->base, arena->total_size);
        arena = next;
    }

    memset(&g_state, 0, sizeof(sh_state_t));
    unlock();
}

/* ── Core: malloc ────────────────────────────────────────────── */

static void *sh_malloc_internal(size_t size, const char *file, int line)
{
    header_t *header;
    arena_t *arena;
    size_t aligned_size;

    if (!size)
        return NULL;

    /* Ensure the allocator is initialized */
    if (!g_state.initialized)
        sh_init();

    /* Align the requested size up to SH_ALIGNMENT boundary */
    aligned_size = ALIGN_UP(size);

    lock();

    /* Step 1: Search existing free blocks using the current strategy */
    header = block_find(&g_state, aligned_size, g_state.strategy);

    if (header) {
        /* Found a reusable free block */
        header->s.is_free = 0;
        header->s.magic = SH_MAGIC;
        header->s.alloc_file = file;
        header->s.alloc_line = line;
        header->s.alloc_id = g_state.next_alloc_id++;

        /* Split if the block is significantly larger than needed */
        block_split(&g_state, header, aligned_size);

        /* Update statistics */
        g_state.alloc_count++;
        g_state.reuse_count++;
        g_state.current_usage += header->s.size;
        if (g_state.current_usage > g_state.peak_usage)
            g_state.peak_usage = g_state.current_usage;
        g_state.total_allocated += header->s.size;

        unlock();
        return HEADER_TO_PTR(header);
    }

    /* Step 2: No free block found — create a new arena */
    arena = arena_create(&g_state, aligned_size);
    if (!arena) {
        unlock();
        return NULL;
    }

    /* The arena starts with one large free block; use it */
    header = arena->first_block;
    header->s.is_free = 0;
    header->s.magic = SH_MAGIC;
    header->s.alloc_file = file;
    header->s.alloc_line = line;
    header->s.alloc_id = g_state.next_alloc_id++;

    /* Split the arena's initial block */
    block_split(&g_state, header, aligned_size);

    /* Update statistics */
    g_state.alloc_count++;
    g_state.current_usage += header->s.size;
    if (g_state.current_usage > g_state.peak_usage)
        g_state.peak_usage = g_state.current_usage;
    g_state.total_allocated += header->s.size;

    unlock();
    return HEADER_TO_PTR(header);
}

void *sh_malloc(size_t size)
{
    return sh_malloc_internal(size, NULL, 0);
}

void *sh_malloc_tracked(size_t size, const char *file, int line)
{
    return sh_malloc_internal(size, file, line);
}

/* ── Core: free ──────────────────────────────────────────────── */

void sh_free(void *ptr)
{
    header_t *header;
    arena_t *arena;

    if (!ptr)
        return;

    lock();

    header = PTR_TO_HEADER(ptr);

    /* Validate the block header */
    if (!block_validate(header)) {
        fprintf(stderr, "[SmartHeap] ERROR: Invalid free — corrupted header at %p\n", ptr);
        unlock();
        return;
    }

    /* Detect double-free */
    if (header->s.magic == SH_FREED_MAGIC || header->s.is_free) {
        fprintf(stderr, "[SmartHeap] ERROR: Double free detected at %p", ptr);
        if (header->s.alloc_file)
            fprintf(stderr, " (originally allocated at %s:%d)", header->s.alloc_file, header->s.alloc_line);
        fprintf(stderr, "\n");
        unlock();
        return;
    }

    /* Mark as free */
    g_state.current_usage -= header->s.size;
    g_state.total_freed += header->s.size;
    g_state.free_count++;

    header->s.is_free = 1;
    header->s.magic = SH_FREED_MAGIC;

    /* Coalesce with adjacent free blocks */
    header = block_coalesce(&g_state, header);

    /*
     * If the entire arena is now free, release it back to the OS.
     * This prevents indefinite memory growth.
     */
    arena = arena_find(&g_state, header);
    if (arena && arena_is_empty(arena)) {
        /* Don't destroy the last arena — keep at least one for reuse */
        if (g_state.arena_count > 1) {
            arena_destroy(&g_state, arena);
        }
    }

    unlock();
}

/* ── Core: calloc ────────────────────────────────────────────── */

static void *sh_calloc_internal(size_t num, size_t nsize, const char *file, int line)
{
    size_t size;
    void *block;

    if (!num || !nsize)
        return NULL;

    size = num * nsize;

    /* Check for multiplication overflow */
    if (nsize != size / num)
        return NULL;

    block = sh_malloc_internal(size, file, line);
    if (!block)
        return NULL;

    memset(block, 0, size);
    return block;
}

void *sh_calloc(size_t num, size_t nsize)
{
    return sh_calloc_internal(num, nsize, NULL, 0);
}

void *sh_calloc_tracked(size_t num, size_t nsize, const char *file, int line)
{
    return sh_calloc_internal(num, nsize, file, line);
}

/* ── Core: realloc ───────────────────────────────────────────── */

static void *sh_realloc_internal(void *ptr, size_t size, const char *file, int line)
{
    header_t *header;
    void *new_block;

    if (!ptr)
        return sh_malloc_internal(size, file, line);

    if (!size) {
        sh_free(ptr);
        return NULL;
    }

    header = PTR_TO_HEADER(ptr);

    if (!block_validate(header)) {
        fprintf(stderr, "[SmartHeap] ERROR: Invalid realloc — corrupted header at %p\n", ptr);
        return NULL;
    }

    size_t aligned_size = ALIGN_UP(size);

    /* Current block is already large enough */
    if (header->s.size >= aligned_size)
        return ptr;

    /*
     * Try to expand in-place by absorbing the next block if it's free
     * and large enough. This avoids a copy.
     */
    lock();
    if (header->s.next && header->s.next->s.is_free) {
        size_t combined = header->s.size + sizeof(header_t) + header->s.next->s.size;
        if (combined >= aligned_size) {
            /* Absorb the next block */
            header_t *absorbed = header->s.next;
            header->s.size = combined;
            header->s.next = absorbed->s.next;
            if (absorbed->s.next)
                absorbed->s.next->s.prev = header;

            g_state.coalesce_count++;

            /* Split if there's excess */
            block_split(&g_state, header, aligned_size);

            unlock();
            return ptr;
        }
    }
    unlock();

    /* Fall back: allocate new block, copy, free old */
    new_block = sh_malloc_internal(size, file, line);
    if (new_block) {
        memcpy(new_block, ptr, header->s.size);
        sh_free(ptr);
    }
    return new_block;
}

void *sh_realloc(void *ptr, size_t size)
{
    return sh_realloc_internal(ptr, size, NULL, 0);
}

void *sh_realloc_tracked(void *ptr, size_t size, const char *file, int line)
{
    return sh_realloc_internal(ptr, size, file, line);
}

/* ── Configuration ───────────────────────────────────────────── */

void sh_set_strategy(sh_strategy strategy)
{
    lock();
    g_state.strategy = (sh_strategy_t)strategy;
    unlock();
}

sh_strategy sh_get_strategy(void)
{
    return (sh_strategy)g_state.strategy;
}

const char *sh_get_strategy_name(void)
{
    return strategy_name(g_state.strategy);
}

/* ── Visualization ───────────────────────────────────────────── */

void sh_visualize(void)
{
    lock();
    visualizer_print_map(&g_state);
    unlock();
}

void sh_print_summary(void)
{
    lock();
    visualizer_print_summary(&g_state);
    unlock();
}

/* ── Statistics ──────────────────────────────────────────────── */

void sh_print_stats(void)
{
    lock();
    sh_stats_t s = stats_compute(&g_state);
    stats_print(&s);
    unlock();
}

void sh_benchmark_strategies(size_t num_ops)
{
    stats_compare_strategies(num_ops);
}

/* ── Leak Detection ──────────────────────────────────────────── */

size_t sh_leak_check(void)
{
    size_t count;
    lock();
    count = leak_check(&g_state);
    if (count > 0)
        leak_report(&g_state);
    else
        fprintf(stderr, "\n[SmartHeap] No memory leaks detected. All clean!\n\n");
    unlock();
    return count;
}

/* ── JSON State Export ───────────────────────────────────────── */

int sh_state_json(char *buf, size_t bufsize)
{
    int pos = 0;
    lock();

    /* Strategy name */
    const char *strat = strategy_name(g_state.strategy);

    /* Compute stats */
    sh_stats_t st = stats_compute(&g_state);

    /* Start JSON */
    pos += snprintf(buf + pos, bufsize - pos,
        "{\"strategy\":\"%s\","
        "\"stats\":{"
        "\"current_usage\":%zu,"
        "\"peak_usage\":%zu,"
        "\"total_allocated\":%zu,"
        "\"total_freed\":%zu,"
        "\"arena_memory\":%zu,"
        "\"alloc_count\":%zu,"
        "\"free_count\":%zu,"
        "\"reuse_count\":%zu,"
        "\"split_count\":%zu,"
        "\"coalesce_count\":%zu,"
        "\"arena_count\":%zu,"
        "\"total_blocks\":%zu,"
        "\"used_blocks\":%zu,"
        "\"free_blocks\":%zu,"
        "\"external_frag\":%.1f,"
        "\"utilization\":%.1f,"
        "\"largest_free\":%zu,"
        "\"smallest_free\":%zu"
        "},",
        strat,
        g_state.current_usage,
        g_state.peak_usage,
        g_state.total_allocated,
        g_state.total_freed,
        g_state.total_arena_memory,
        g_state.alloc_count,
        g_state.free_count,
        g_state.reuse_count,
        g_state.split_count,
        g_state.coalesce_count,
        g_state.arena_count,
        st.total_blocks,
        st.used_blocks,
        st.free_blocks,
        st.external_fragmentation * 100.0,
        st.utilization * 100.0,
        st.largest_free_block,
        st.smallest_free_block
    );

    /* Arenas */
    pos += snprintf(buf + pos, bufsize - pos, "\"arenas\":[");
    arena_t *arena = g_state.arenas;
    int arena_id = 0;
    while (arena) {
        if (arena_id > 0)
            pos += snprintf(buf + pos, bufsize - pos, ",");

        pos += snprintf(buf + pos, bufsize - pos,
            "{\"id\":%d,\"total_size\":%zu,\"blocks\":[",
            arena_id, arena->total_size);

        header_t *block = arena->first_block;
        int first_block = 1;
        while (block) {
            if (!first_block)
                pos += snprintf(buf + pos, bufsize - pos, ",");

            pos += snprintf(buf + pos, bufsize - pos,
                "{\"size\":%zu,\"is_free\":%s,\"alloc_id\":%u}",
                block->s.size,
                block->s.is_free ? "true" : "false",
                block->s.alloc_id);

            first_block = 0;
            block = block->s.next;

            /* Safety: avoid overflow */
            if ((size_t)pos >= bufsize - 100) break;
        }

        pos += snprintf(buf + pos, bufsize - pos, "]}");

        arena = arena->next;
        arena_id++;

        if ((size_t)pos >= bufsize - 100) break;
    }

    pos += snprintf(buf + pos, bufsize - pos, "]}");

    unlock();
    return pos;
}
