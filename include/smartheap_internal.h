/*
 * SmartHeap - Thread-Safe Custom Memory Allocator
 * smartheap_internal.h - Internal Types, Constants, and Macros
 *
 * This header defines the core data structures used throughout
 * SmartHeap. It is NOT part of the public API.
 */

#ifndef SMARTHEAP_INTERNAL_H
#define SMARTHEAP_INTERNAL_H

#include <stdint.h>
#include <stddef.h>

/* ── Configuration Constants ─────────────────────────────────── */

/* Magic number for corruption detection.
 * Every valid block header starts with this value. If it's wrong,
 * the block has been corrupted (buffer overflow, use-after-free, etc.) */
#define SH_MAGIC           0xDEADBEEFU

/* Magic number for freed blocks (detects double-free) */
#define SH_FREED_MAGIC     0xFEEDFACEU

/* Default arena size: 1 MB.
 * We request large chunks from the OS and sub-allocate from them. */
#define SH_ARENA_SIZE      (1 << 20)

/* Memory alignment: 16 bytes.
 * Ensures returned pointers are suitable for any C data type,
 * including SSE/AVX aligned types. */
#define SH_ALIGNMENT       16

/* Minimum block size: prevents creating tiny unusable fragments
 * when splitting blocks. Must be >= sizeof(block_header_t). */
#define SH_MIN_BLOCK_SIZE  32

/* ── Allocation Strategy ─────────────────────────────────────── */

typedef enum {
    SH_FIRST_FIT = 0,   /* Return the first block that fits (fast) */
    SH_BEST_FIT  = 1,   /* Return the smallest block that fits (less waste) */
    SH_WORST_FIT = 2    /* Return the largest block that fits (large remainders) */
} sh_strategy_t;

/* ── Alignment Type ──────────────────────────────────────────── */

/* Used to force struct alignment to SH_ALIGNMENT bytes */
typedef char SH_ALIGN[SH_ALIGNMENT];

/* ── Block Header ────────────────────────────────────────────── */

/*
 * Every allocated block has a header immediately before the user data.
 * The header is hidden from the caller — they receive a pointer to
 * the byte right after the header.
 *
 * Memory layout:
 *   [header_t][...... user data ......]
 *   ^         ^
 *   |         |__ pointer returned to caller
 *   |__ actual allocation start
 */
/* Forward declaration so block_header can point to header_t */
union header_aligned;

typedef struct block_header {
    uint32_t       magic;        /* SH_MAGIC for allocated, SH_FREED_MAGIC for freed */
    size_t         size;         /* Usable size in bytes (excluding header) */
    unsigned       is_free;      /* 1 = free, 0 = allocated */
    union header_aligned *next;  /* Next block in the arena's block list */
    union header_aligned *prev;  /* Previous block (for O(1) coalescing) */
    const char    *alloc_file;   /* Source file of allocation (leak tracking) */
    int            alloc_line;   /* Source line of allocation (leak tracking) */
    uint32_t       alloc_id;     /* Unique allocation ID (for interactive CLI) */
} block_header_t;

/*
 * Union wrapper to force the header to be aligned to SH_ALIGNMENT bytes.
 * The end of the header is where user data begins, so this ensures
 * the returned pointer is always properly aligned.
 *
 * sizeof(header_t) will be max(sizeof(block_header_t), SH_ALIGNMENT),
 * rounded up to the nearest multiple of SH_ALIGNMENT.
 */
union header_aligned {
    block_header_t s;
    SH_ALIGN       stub;
};
typedef union header_aligned header_t;

/* ── Arena ───────────────────────────────────────────────────── */

/*
 * An arena is a large chunk of memory obtained from the OS.
 * We sub-allocate blocks from arenas. When an arena is fully freed,
 * we can return it to the OS.
 *
 * Each arena contains a linked list of blocks (both used and free).
 */
typedef struct arena {
    void          *base;         /* Start of the arena memory (from platform_alloc) */
    size_t         total_size;   /* Total size of the arena in bytes */
    size_t         used_size;    /* Currently used bytes (headers + user data) */
    header_t      *first_block;  /* First block in this arena */
    struct arena  *next;         /* Next arena in the global arena list */
} arena_t;

/* ── Global Allocator State ──────────────────────────────────── */

/*
 * The global state of the SmartHeap allocator.
 * Protected by a mutex for thread safety.
 */
typedef struct {
    arena_t       *arenas;       /* Linked list of all arenas */
    sh_strategy_t  strategy;     /* Current allocation strategy */
    uint32_t       next_alloc_id;/* Counter for unique allocation IDs */
    int            initialized;  /* 1 if init has been called */

    /* Statistics */
    size_t         total_allocated;   /* Cumulative bytes allocated */
    size_t         total_freed;       /* Cumulative bytes freed */
    size_t         current_usage;     /* Current bytes in use */
    size_t         peak_usage;        /* Peak bytes in use */
    size_t         alloc_count;       /* Number of malloc calls */
    size_t         free_count;        /* Number of free calls */
    size_t         reuse_count;       /* Number of times a free block was reused */
    size_t         arena_count;       /* Number of active arenas */
    size_t         total_arena_memory;/* Total memory obtained from OS */
    size_t         split_count;       /* Number of block splits */
    size_t         coalesce_count;    /* Number of block coalesces */
} sh_state_t;

/* ── Utility Macros ──────────────────────────────────────────── */

/* Get user data pointer from header */
#define HEADER_TO_PTR(h)   ((void *)((header_t *)(h) + 1))

/* Get header pointer from user data pointer */
#define PTR_TO_HEADER(p)   ((header_t *)((header_t *)(p) - 1))

/* Align a size up to SH_ALIGNMENT boundary */
#define ALIGN_UP(size)     (((size) + (SH_ALIGNMENT - 1)) & ~(SH_ALIGNMENT - 1))

/* Check if a pointer falls within an arena's memory range */
#define IN_ARENA(arena, ptr) \
    ((char *)(ptr) >= (char *)(arena)->base && \
     (char *)(ptr) < (char *)(arena)->base + (arena)->total_size)

#endif /* SMARTHEAP_INTERNAL_H */
