/*
 * SmartHeap - Thread-Safe Custom Memory Allocator
 * stats.h - Statistics and Fragmentation Analysis Interface
 *
 * Tracks allocation statistics and computes fragmentation metrics
 * for performance analysis and strategy comparison.
 */

#ifndef SMARTHEAP_STATS_H
#define SMARTHEAP_STATS_H

#include "smartheap_internal.h"

/*
 * Comprehensive statistics snapshot.
 */
typedef struct {
    /* Memory usage */
    size_t total_arena_memory;     /* Total memory obtained from OS */
    size_t total_allocated;        /* Cumulative bytes ever allocated */
    size_t total_freed;            /* Cumulative bytes ever freed */
    size_t current_usage;          /* Current bytes in use by caller */
    size_t peak_usage;             /* Peak bytes in use */
    size_t overhead;               /* Memory used by headers */

    /* Block counts */
    size_t total_blocks;           /* Total blocks (used + free) */
    size_t used_blocks;            /* Currently allocated blocks */
    size_t free_blocks;            /* Currently free blocks */
    size_t arena_count;            /* Number of active arenas */

    /* Operation counts */
    size_t alloc_count;            /* Total malloc/calloc calls */
    size_t free_count;             /* Total free calls */
    size_t reuse_count;            /* Blocks reused from free list */
    size_t split_count;            /* Blocks split during allocation */
    size_t coalesce_count;         /* Blocks merged during free */

    /* Fragmentation metrics (0.0 to 1.0) */
    double external_fragmentation; /* 1 - (largest_free / total_free) */
    double internal_fragmentation; /* wasted space within allocated blocks */
    double utilization;            /* current_usage / total_arena_memory */

    /* Block size distribution */
    size_t largest_free_block;     /* Largest contiguous free block */
    size_t smallest_free_block;    /* Smallest free block */
    double avg_free_block_size;    /* Average free block size */
    double avg_used_block_size;    /* Average used block size */
} sh_stats_t;

/*
 * Compute a snapshot of the current allocator statistics.
 *
 * @param state  Global allocator state
 * @return       Statistics snapshot
 */
sh_stats_t stats_compute(sh_state_t *state);

/*
 * Print a formatted statistics report to stdout.
 *
 * @param stats  Statistics snapshot to print
 */
void stats_print(const sh_stats_t *stats);

/*
 * Print a comparison table of all three allocation strategies.
 * Runs a configurable benchmark and displays results.
 *
 * @param num_operations  Number of alloc/free operations per strategy
 */
void stats_compare_strategies(size_t num_operations);

#endif /* SMARTHEAP_STATS_H */
