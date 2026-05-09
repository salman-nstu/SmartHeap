/*
 * SmartHeap - Thread-Safe Custom Memory Allocator
 * smartheap.h - Public API
 *
 * A custom dynamic memory allocator that provides malloc(), free(),
 * calloc(), and realloc() replacements with:
 *   - Multiple allocation strategies (First Fit, Best Fit, Worst Fit)
 *   - Block splitting and coalescing
 *   - Thread safety via mutex locks
 *   - Terminal-based heap visualization
 *   - Fragmentation analysis
 *   - Memory leak detection
 *
 * Usage:
 *   #include "smartheap.h"
 *
 *   sh_init();
 *   void *ptr = sh_malloc(64);
 *   sh_free(ptr);
 *   sh_leak_check();
 *   sh_destroy();
 *
 * For leak detection with source tracking:
 *   #define SH_TRACK_LEAKS
 *   #include "smartheap.h"
 *   // sh_malloc() will now record __FILE__ and __LINE__
 */

#ifndef SMARTHEAP_H
#define SMARTHEAP_H

#include <stddef.h>

/* ── Allocation Strategies ───────────────────────────────────── */

typedef enum {
    SH_STRATEGY_FIRST_FIT = 0,   /* Fast: first block that fits */
    SH_STRATEGY_BEST_FIT  = 1,   /* Efficient: smallest fitting block */
    SH_STRATEGY_WORST_FIT = 2    /* Experimental: largest block */
} sh_strategy;

/* ── Initialization & Cleanup ────────────────────────────────── */

/*
 * Initialize the SmartHeap allocator.
 * Must be called before any allocation functions.
 * Default strategy: First Fit.
 *
 * @return  0 on success, -1 on failure
 */
int sh_init(void);

/*
 * Destroy the SmartHeap allocator and release all memory to the OS.
 * All previously allocated pointers become invalid after this call.
 */
void sh_destroy(void);

/* ── Core Allocation Functions ───────────────────────────────── */

/*
 * Allocate `size` bytes of memory.
 *
 * @param size  Number of bytes to allocate
 * @return      Pointer to allocated memory, or NULL on failure
 */
void *sh_malloc(size_t size);

/*
 * Free a previously allocated block of memory.
 * Passing NULL is a no-op.
 * Double-frees are detected and reported.
 *
 * @param ptr  Pointer to memory to free
 */
void sh_free(void *ptr);

/*
 * Allocate memory for an array of `num` elements of `nsize` bytes each.
 * The allocated memory is zeroed out.
 * Checks for multiplication overflow.
 *
 * @param num    Number of elements
 * @param nsize  Size of each element
 * @return       Pointer to zeroed allocated memory, or NULL on failure
 */
void *sh_calloc(size_t num, size_t nsize);

/*
 * Resize a previously allocated block to `size` bytes.
 * Contents are preserved up to the minimum of old and new sizes.
 * If `ptr` is NULL, behaves like sh_malloc(size).
 * If `size` is 0, behaves like sh_free(ptr) and returns NULL.
 *
 * @param ptr   Pointer to existing block (or NULL)
 * @param size  New size in bytes
 * @return      Pointer to resized block, or NULL on failure
 */
void *sh_realloc(void *ptr, size_t size);

/* ── Leak-Tracked Versions (Internal) ────────────────────────── */

void *sh_malloc_tracked(size_t size, const char *file, int line);
void *sh_calloc_tracked(size_t num, size_t nsize, const char *file, int line);
void *sh_realloc_tracked(void *ptr, size_t size, const char *file, int line);

/* ── Leak Tracking Macros ────────────────────────────────────── */

#ifdef SH_TRACK_LEAKS
#define sh_malloc(size)         sh_malloc_tracked((size), __FILE__, __LINE__)
#define sh_calloc(num, nsize)   sh_calloc_tracked((num), (nsize), __FILE__, __LINE__)
#define sh_realloc(ptr, size)   sh_realloc_tracked((ptr), (size), __FILE__, __LINE__)
#endif

/* ── Configuration ───────────────────────────────────────────── */

/*
 * Set the allocation strategy.
 *
 * @param strategy  One of SH_STRATEGY_FIRST_FIT, SH_STRATEGY_BEST_FIT,
 *                  SH_STRATEGY_WORST_FIT
 */
void sh_set_strategy(sh_strategy strategy);

/*
 * Get the current allocation strategy.
 *
 * @return  Current strategy enum value
 */
sh_strategy sh_get_strategy(void);

/*
 * Get the name of the current strategy as a string.
 *
 * @return  "First Fit", "Best Fit", or "Worst Fit"
 */
const char *sh_get_strategy_name(void);

/* ── Visualization ───────────────────────────────────────────── */

/*
 * Print a visual map of all heap arenas and blocks to stdout.
 * Uses ANSI colors for used (red) and free (green) blocks.
 */
void sh_visualize(void);

/*
 * Print a compact summary of heap usage.
 */
void sh_print_summary(void);

/* ── Statistics & Analysis ───────────────────────────────────── */

/*
 * Print a detailed statistics report including:
 *   - Memory usage (current, peak, total)
 *   - Block counts (used, free, total)
 *   - Fragmentation metrics
 *   - Operation counts
 */
void sh_print_stats(void);

/*
 * Run a benchmark comparing all three allocation strategies.
 *
 * @param num_ops  Number of alloc/free operations per strategy
 */
void sh_benchmark_strategies(size_t num_ops);

/* ── Leak Detection ──────────────────────────────────────────── */

/*
 * Check for memory leaks and print a report.
 *
 * @return  Number of leaked blocks found
 */
size_t sh_leak_check(void);

/* ── JSON State Export (for Dashboard) ───────────────────────── */

/*
 * Export the entire heap state as a JSON string.
 * Writes into the provided buffer. Returns the number of bytes written.
 *
 * @param buf      Buffer to write JSON into
 * @param bufsize  Size of the buffer
 * @return         Number of bytes written (excluding null terminator)
 */
int sh_state_json(char *buf, size_t bufsize);

#endif /* SMARTHEAP_H */
