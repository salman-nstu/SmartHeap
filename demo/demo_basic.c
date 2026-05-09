/*
 * SmartHeap - Thread-Safe Custom Memory Allocator
 * demo_basic.c - Basic Usage Demonstration
 *
 * Shows fundamental operations: malloc, free, calloc, realloc
 * with visualization at each step.
 */

#include <stdio.h>
#include <string.h>

#include <windows.h>

#define SH_TRACK_LEAKS
#include "smartheap.h"

#define C_RESET   "\033[0m"
#define C_BOLD    "\033[1m"
#define C_CYAN    "\033[96m"
#define C_YELLOW  "\033[93m"
#define C_GREEN   "\033[92m"
#define C_WHITE   "\033[97m"

static void section(const char *title)
{
    printf("\n%s%s  ▸ %s%s\n", C_BOLD, C_CYAN, title, C_RESET);
    printf("  %s────────────────────────────────────────%s\n", C_YELLOW, C_RESET);
}

int main(void)
{
    SetConsoleOutputCP(65001);
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    GetConsoleMode(hOut, &mode);
    SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);

    printf("\n%s%s", C_YELLOW, C_BOLD);
    printf("  ╔══════════════════════════════════════════════╗\n");
    printf("  ║       SmartHeap Basic Demo                   ║\n");
    printf("  ╚══════════════════════════════════════════════╝\n");
    printf("%s", C_RESET);

    /* Initialize */
    sh_init();

    /* ── malloc ─────────────────────────────────────────── */
    section("Step 1: Allocate memory with sh_malloc()");

    printf("  Allocating 64, 128, and 256 bytes...\n");
    char *p1 = (char *)sh_malloc(64);
    char *p2 = (char *)sh_malloc(128);
    char *p3 = (char *)sh_malloc(256);

    strcpy(p1, "Hello");
    strcpy(p2, "SmartHeap");
    strcpy(p3, "Memory Allocator");

    printf("  p1 = \"%s\" (%p)\n", p1, (void *)p1);
    printf("  p2 = \"%s\" (%p)\n", p2, (void *)p2);
    printf("  p3 = \"%s\" (%p)\n", p3, (void *)p3);

    sh_visualize();

    /* ── free ───────────────────────────────────────────── */
    section("Step 2: Free p2 (creates a hole in the heap)");

    sh_free(p2);
    printf("  Freed 128-byte block. It's now marked as FREE.\n");

    sh_visualize();

    /* ── reuse ──────────────────────────────────────────── */
    section("Step 3: Allocate 100 bytes (reuses freed block)");

    char *p4 = (char *)sh_malloc(100);
    strcpy(p4, "Reused!");
    printf("  p4 = \"%s\" (%p)\n", p4, (void *)p4);
    printf("  %sNotice: p4 reused p2's block!%s\n", C_GREEN, C_RESET);

    sh_visualize();

    /* ── calloc ─────────────────────────────────────────── */
    section("Step 4: Allocate zeroed memory with sh_calloc()");

    int *arr = (int *)sh_calloc(10, sizeof(int));
    printf("  Allocated array of 10 ints (all zeroed):\n  ");
    for (int i = 0; i < 10; i++)
        printf("%d ", arr[i]);
    printf("\n");

    /* ── realloc ────────────────────────────────────────── */
    section("Step 5: Resize p1 with sh_realloc()");

    printf("  Original p1: \"%s\" (64 bytes)\n", p1);
    p1 = (char *)sh_realloc(p1, 512);
    strcat(p1, ", World! This is a resized block.");
    printf("  Resized p1:  \"%s\" (512 bytes)\n", p1);

    sh_visualize();

    /* ── stats ──────────────────────────────────────────── */
    section("Step 6: Print statistics");
    sh_print_stats();

    /* ── strategies ─────────────────────────────────────── */
    section("Step 7: Change allocation strategy");

    printf("  Current strategy: %s%s%s\n", C_WHITE, sh_get_strategy_name(), C_RESET);
    sh_set_strategy(SH_STRATEGY_BEST_FIT);
    printf("  Changed to:       %s%s%s\n", C_WHITE, sh_get_strategy_name(), C_RESET);

    /* ── leak check ─────────────────────────────────────── */
    section("Step 8: Check for memory leaks");

    /* Free some but not all */
    sh_free(p3);
    sh_free(p4);
    sh_free(arr);
    /* p1 intentionally not freed */

    printf("  Intentionally leaking p1 (512 bytes)...\n");
    sh_leak_check();

    /* Clean up */
    sh_free(p1);
    sh_destroy();

    printf("\n%s%s  ✓ Demo complete!%s\n\n", C_BOLD, C_GREEN, C_RESET);
    return 0;
}
