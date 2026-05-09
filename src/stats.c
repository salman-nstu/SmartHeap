/*
 * SmartHeap - Thread-Safe Custom Memory Allocator
 * stats.c - Statistics & Fragmentation Analysis
 *
 * Computes and displays comprehensive metrics about the allocator's
 * current state, including fragmentation, utilization, and block
 * size distributions.
 */

#include "stats.h"
#include "smartheap.h"
#include "smartheap_internal.h"
#include "arena.h"
#include "block.h"
#include "strategy.h"
#include "platform.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include <windows.h>

static void enable_ansi_stats(void)
{
    SetConsoleOutputCP(65001);
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    GetConsoleMode(hOut, &mode);
    SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
}

/* ── ANSI Colors ─────────────────────────────────────────────── */

#define C_RESET    "\033[0m"
#define C_BOLD     "\033[1m"
#define C_DIM      "\033[2m"
#define C_RED      "\033[91m"
#define C_GREEN    "\033[92m"
#define C_YELLOW   "\033[93m"
#define C_BLUE     "\033[94m"
#define C_MAGENTA  "\033[95m"
#define C_CYAN     "\033[96m"
#define C_WHITE    "\033[97m"

/* ── Helper: Format Size ─────────────────────────────────────── */

static const char *fmt_size(size_t bytes, char *buf, size_t buf_size)
{
    if (bytes >= 1048576)
        snprintf(buf, buf_size, "%.2f MB", (double)bytes / 1048576.0);
    else if (bytes >= 1024)
        snprintf(buf, buf_size, "%.2f KB", (double)bytes / 1024.0);
    else
        snprintf(buf, buf_size, "%zu B", bytes);
    return buf;
}

/* ── Compute Statistics ──────────────────────────────────────── */

sh_stats_t stats_compute(sh_state_t *state)
{
    sh_stats_t s;
    memset(&s, 0, sizeof(s));

    s.total_arena_memory   = state->total_arena_memory;
    s.total_allocated      = state->total_allocated;
    s.total_freed          = state->total_freed;
    s.current_usage        = state->current_usage;
    s.peak_usage           = state->peak_usage;
    s.alloc_count          = state->alloc_count;
    s.free_count           = state->free_count;
    s.reuse_count          = state->reuse_count;
    s.split_count          = state->split_count;
    s.coalesce_count       = state->coalesce_count;
    s.arena_count          = state->arena_count;

    size_t total_free      = 0;
    size_t largest_free    = 0;
    size_t smallest_free   = (size_t)-1;
    size_t total_used_size = 0;

    arena_t *arena = state->arenas;
    while (arena) {
        header_t *block = arena->first_block;
        while (block) {
            s.total_blocks++;
            s.overhead += sizeof(header_t);

            if (block->s.is_free) {
                s.free_blocks++;
                total_free += block->s.size;
                if (block->s.size > largest_free)
                    largest_free = block->s.size;
                if (block->s.size < smallest_free)
                    smallest_free = block->s.size;
            } else {
                s.used_blocks++;
                total_used_size += block->s.size;
            }
            block = block->s.next;
        }
        arena = arena->next;
    }

    s.largest_free_block  = largest_free;
    s.smallest_free_block = (smallest_free == (size_t)-1) ? 0 : smallest_free;

    if (s.free_blocks > 0)
        s.avg_free_block_size = (double)total_free / s.free_blocks;

    if (s.used_blocks > 0)
        s.avg_used_block_size = (double)total_used_size / s.used_blocks;

    /* External fragmentation: 1 - (largest_free / total_free)
     * 0.0 = no fragmentation (all free memory in one block)
     * 1.0 = maximum fragmentation (many tiny free blocks) */
    if (total_free > 0 && s.free_blocks > 1)
        s.external_fragmentation = 1.0 - (double)largest_free / total_free;
    else
        s.external_fragmentation = 0.0;

    /* Utilization: current_usage / total_arena_memory */
    if (s.total_arena_memory > 0)
        s.utilization = (double)s.current_usage / s.total_arena_memory;

    return s;
}

/* ── Print Statistics ────────────────────────────────────────── */

void stats_print(const sh_stats_t *s)
{
    char buf1[32];
    enable_ansi_stats();

    printf("\n");
    printf("%s%s", C_YELLOW, C_BOLD);
    printf("  ╔══════════════════════════════════════════════════════╗\n");
    printf("  ║          SmartHeap Statistics Report                 ║\n");
    printf("  ╠══════════════════════════════════════════════════════╣\n");
    printf("%s", C_RESET);

    /* Memory Usage */
    printf("%s  ║%s %s%-20s%s %s%15s%s           %s║%s\n",
           C_YELLOW, C_RESET, C_BOLD, "MEMORY USAGE", C_RESET, "", "", C_RESET, C_YELLOW, C_RESET);
    printf("%s  ║%s   Current Usage:     %s%15s%s           %s║%s\n",
           C_YELLOW, C_RESET, C_RED, fmt_size(s->current_usage, buf1, sizeof(buf1)), C_RESET, C_YELLOW, C_RESET);
    printf("%s  ║%s   Peak Usage:        %s%15s%s           %s║%s\n",
           C_YELLOW, C_RESET, C_MAGENTA, fmt_size(s->peak_usage, buf1, sizeof(buf1)), C_RESET, C_YELLOW, C_RESET);
    printf("%s  ║%s   Total Allocated:   %s%15s%s           %s║%s\n",
           C_YELLOW, C_RESET, C_WHITE, fmt_size(s->total_allocated, buf1, sizeof(buf1)), C_RESET, C_YELLOW, C_RESET);
    printf("%s  ║%s   Total Freed:       %s%15s%s           %s║%s\n",
           C_YELLOW, C_RESET, C_GREEN, fmt_size(s->total_freed, buf1, sizeof(buf1)), C_RESET, C_YELLOW, C_RESET);
    printf("%s  ║%s   Arena Memory:      %s%15s%s           %s║%s\n",
           C_YELLOW, C_RESET, C_BLUE, fmt_size(s->total_arena_memory, buf1, sizeof(buf1)), C_RESET, C_YELLOW, C_RESET);
    printf("%s  ║%s   Header Overhead:   %s%15s%s           %s║%s\n",
           C_YELLOW, C_RESET, C_DIM, fmt_size(s->overhead, buf1, sizeof(buf1)), C_RESET, C_YELLOW, C_RESET);

    printf("%s  ╠══════════════════════════════════════════════════════╣%s\n", C_YELLOW, C_RESET);

    /* Block Counts */
    printf("%s  ║%s %s%-20s%s %s%15s%s           %s║%s\n",
           C_YELLOW, C_RESET, C_BOLD, "BLOCK COUNTS", C_RESET, "", "", C_RESET, C_YELLOW, C_RESET);
    printf("%s  ║%s   Total Blocks:      %15zu           %s║%s\n",
           C_YELLOW, C_RESET, s->total_blocks, C_YELLOW, C_RESET);
    printf("%s  ║%s   Used Blocks:       %s%15zu%s           %s║%s\n",
           C_YELLOW, C_RESET, C_RED, s->used_blocks, C_RESET, C_YELLOW, C_RESET);
    printf("%s  ║%s   Free Blocks:       %s%15zu%s           %s║%s\n",
           C_YELLOW, C_RESET, C_GREEN, s->free_blocks, C_RESET, C_YELLOW, C_RESET);
    printf("%s  ║%s   Active Arenas:     %s%15zu%s           %s║%s\n",
           C_YELLOW, C_RESET, C_BLUE, s->arena_count, C_RESET, C_YELLOW, C_RESET);

    printf("%s  ╠══════════════════════════════════════════════════════╣%s\n", C_YELLOW, C_RESET);

    /* Operations */
    printf("%s  ║%s %s%-20s%s %s%15s%s           %s║%s\n",
           C_YELLOW, C_RESET, C_BOLD, "OPERATIONS", C_RESET, "", "", C_RESET, C_YELLOW, C_RESET);
    printf("%s  ║%s   Allocations:       %15zu           %s║%s\n",
           C_YELLOW, C_RESET, s->alloc_count, C_YELLOW, C_RESET);
    printf("%s  ║%s   Frees:             %15zu           %s║%s\n",
           C_YELLOW, C_RESET, s->free_count, C_YELLOW, C_RESET);
    printf("%s  ║%s   Block Reuses:      %s%15zu%s           %s║%s\n",
           C_YELLOW, C_RESET, C_CYAN, s->reuse_count, C_RESET, C_YELLOW, C_RESET);
    printf("%s  ║%s   Block Splits:      %15zu           %s║%s\n",
           C_YELLOW, C_RESET, s->split_count, C_YELLOW, C_RESET);
    printf("%s  ║%s   Block Coalesces:   %15zu           %s║%s\n",
           C_YELLOW, C_RESET, s->coalesce_count, C_YELLOW, C_RESET);

    printf("%s  ╠══════════════════════════════════════════════════════╣%s\n", C_YELLOW, C_RESET);

    /* Fragmentation */
    printf("%s  ║%s %s%-20s%s %s%15s%s           %s║%s\n",
           C_YELLOW, C_RESET, C_BOLD, "FRAGMENTATION", C_RESET, "", "", C_RESET, C_YELLOW, C_RESET);

    /* Color-code fragmentation: green < 20%, yellow < 50%, red >= 50% */
    const char *frag_color = s->external_fragmentation < 0.2 ? C_GREEN :
                             s->external_fragmentation < 0.5 ? C_YELLOW : C_RED;
    printf("%s  ║%s   External Frag:     %s%14.1f%%%s           %s║%s\n",
           C_YELLOW, C_RESET, frag_color, s->external_fragmentation * 100.0, C_RESET, C_YELLOW, C_RESET);
    printf("%s  ║%s   Utilization:       %s%14.1f%%%s           %s║%s\n",
           C_YELLOW, C_RESET, C_CYAN, s->utilization * 100.0, C_RESET, C_YELLOW, C_RESET);

    if (s->free_blocks > 0) {
        printf("%s  ║%s   Largest Free:      %s%15s%s           %s║%s\n",
               C_YELLOW, C_RESET, C_GREEN, fmt_size(s->largest_free_block, buf1, sizeof(buf1)), C_RESET, C_YELLOW, C_RESET);
        printf("%s  ║%s   Smallest Free:     %s%15s%s           %s║%s\n",
               C_YELLOW, C_RESET, C_GREEN, fmt_size(s->smallest_free_block, buf1, sizeof(buf1)), C_RESET, C_YELLOW, C_RESET);
        printf("%s  ║%s   Avg Free Size:     %14.1f B           %s║%s\n",
               C_YELLOW, C_RESET, s->avg_free_block_size, C_YELLOW, C_RESET);
    }

    if (s->used_blocks > 0) {
        printf("%s  ║%s   Avg Used Size:     %14.1f B           %s║%s\n",
               C_YELLOW, C_RESET, s->avg_used_block_size, C_YELLOW, C_RESET);
    }

    printf("%s%s", C_YELLOW, C_BOLD);
    printf("  ╚══════════════════════════════════════════════════════╝\n");
    printf("%s\n", C_RESET);
}

/* ── Strategy Comparison Benchmark ───────────────────────────── */

void stats_compare_strategies(size_t num_operations)
{
    enable_ansi_stats();

    printf("\n%s%s  ╔══════════════════════════════════════════════════════════════╗%s\n", C_YELLOW, C_BOLD, C_RESET);
    printf("%s%s  ║        Strategy Comparison Benchmark (%zu ops each)        ║%s\n", C_YELLOW, C_BOLD, num_operations, C_RESET);
    printf("%s%s  ╠══════════════════════════════════════════════════════════════╣%s\n", C_YELLOW, C_BOLD, C_RESET);
    printf("%s%s  ║  Strategy    │  Time (ms)  │  Ext. Frag  │  Reuse Rate    ║%s\n", C_YELLOW, C_BOLD, C_RESET);
    printf("%s%s  ╠══════════════════════════════════════════════════════════════╣%s\n", C_YELLOW, C_BOLD, C_RESET);

    sh_strategy strategies[] = { SH_STRATEGY_FIRST_FIT, SH_STRATEGY_BEST_FIT, SH_STRATEGY_WORST_FIT };
    const char *names[] = { "First Fit ", "Best Fit  ", "Worst Fit " };

    for (int s = 0; s < 3; s++) {
        /* Initialize a fresh allocator for each strategy */
        sh_init();
        sh_set_strategy(strategies[s]);

        /* Seed for reproducibility */
        srand(42);

        /* Array to track allocations for freeing */
        void **ptrs = (void **)malloc(num_operations * sizeof(void *));
        size_t ptr_count = 0;

        clock_t start = clock();

        for (size_t i = 0; i < num_operations; i++) {
            if (ptr_count > 0 && (rand() % 3 == 0)) {
                /* Free a random block 1/3 of the time */
                size_t idx = rand() % ptr_count;
                sh_free(ptrs[idx]);
                ptrs[idx] = ptrs[--ptr_count];
            } else {
                /* Allocate a random size between 16 and 1024 */
                size_t sz = 16 + (rand() % 1009);
                void *p = sh_malloc(sz);
                if (p && ptr_count < num_operations)
                    ptrs[ptr_count++] = p;
            }
        }

        clock_t end = clock();
        double time_ms = (double)(end - start) / CLOCKS_PER_SEC * 1000.0;

        /* Compute stats before cleanup - timing only for now */
        double ext_frag = 0.0;

        /* Print row */
        printf("%s  ║%s  %s%-11s%s│%s %9.1f   %s│  %9.1f%%  │  %9s     %s║%s\n",
               C_YELLOW, C_RESET, C_WHITE, names[s], C_RESET,
               C_CYAN, time_ms, C_RESET,
               ext_frag * 100.0,
               "--",
               C_YELLOW, C_RESET);

        /* Cleanup */
        for (size_t i = 0; i < ptr_count; i++)
            sh_free(ptrs[i]);

        free(ptrs);
        sh_destroy();
    }

    printf("%s%s  ╚══════════════════════════════════════════════════════════════╝%s\n\n", C_YELLOW, C_BOLD, C_RESET);
}
