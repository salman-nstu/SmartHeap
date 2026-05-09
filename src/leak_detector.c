/*
 * SmartHeap - Thread-Safe Custom Memory Allocator
 * leak_detector.c - Memory Leak Detection
 *
 * Walks all blocks in all arenas and reports any blocks
 * still marked as allocated (not freed).
 * When SH_TRACK_LEAKS is defined, reports include the
 * source file and line number of the allocation.
 */

#include "leak_detector.h"
#include <stdio.h>

#include <windows.h>

static void enable_ansi_leak(void)
{
    SetConsoleOutputCP(65001);
    HANDLE hOut = GetStdHandle(STD_ERROR_HANDLE);
    DWORD mode = 0;
    GetConsoleMode(hOut, &mode);
    SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
}

/* ANSI colors for stderr output */
#define C_RESET    "\033[0m"
#define C_BOLD     "\033[1m"
#define C_RED      "\033[91m"
#define C_GREEN    "\033[92m"
#define C_YELLOW   "\033[93m"
#define C_CYAN     "\033[96m"
#define C_WHITE    "\033[97m"

size_t leak_check(sh_state_t *state)
{
    size_t count = 0;
    arena_t *arena = state->arenas;

    while (arena) {
        header_t *block = arena->first_block;
        while (block) {
            if (!block->s.is_free)
                count++;
            block = block->s.next;
        }
        arena = arena->next;
    }
    return count;
}

void leak_report(sh_state_t *state)
{
    enable_ansi_leak();

    size_t total_leaked_bytes = 0;
    size_t leak_count = 0;

    fprintf(stderr, "\n");
    fprintf(stderr, "%s%s", C_YELLOW, C_BOLD);
    fprintf(stderr, "  ╔═══════════════════════════════════════════════════╗\n");
    fprintf(stderr, "  ║           SmartHeap Leak Report                  ║\n");
    fprintf(stderr, "  ╠═══════════════════════════════════════════════════╣\n");
    fprintf(stderr, "%s", C_RESET);

    arena_t *arena = state->arenas;
    int arena_num = 0;

    while (arena) {
        header_t *block = arena->first_block;
        while (block) {
            if (!block->s.is_free) {
                leak_count++;
                total_leaked_bytes += block->s.size;

                fprintf(stderr, "%s  ║%s", C_YELLOW, C_RESET);
                fprintf(stderr, "  %s⚠ LEAK:%s %s%zu bytes%s at %p",
                        C_RED, C_RESET,
                        C_WHITE, block->s.size, C_RESET,
                        (void *)block);

                if (block->s.alloc_file) {
                    fprintf(stderr, "\n%s  ║%s", C_YELLOW, C_RESET);
                    fprintf(stderr, "         %sallocated at %s:%d%s",
                            C_CYAN, block->s.alloc_file,
                            block->s.alloc_line, C_RESET);
                }

                if (block->s.alloc_id > 0) {
                    fprintf(stderr, " (ID #%u)", block->s.alloc_id);
                }

                fprintf(stderr, "\n");
            }
            block = block->s.next;
        }
        arena = arena->next;
        arena_num++;
    }

    fprintf(stderr, "%s%s", C_YELLOW, C_BOLD);
    fprintf(stderr, "  ╠═══════════════════════════════════════════════════╣\n");
    fprintf(stderr, "%s", C_RESET);

    if (total_leaked_bytes >= 1048576) {
        fprintf(stderr, "%s  ║%s  %sTotal leaked: %.2f MB in %zu block(s)%s\n",
                C_YELLOW, C_RESET, C_RED,
                (double)total_leaked_bytes / 1048576.0, leak_count, C_RESET);
    } else if (total_leaked_bytes >= 1024) {
        fprintf(stderr, "%s  ║%s  %sTotal leaked: %.2f KB in %zu block(s)%s\n",
                C_YELLOW, C_RESET, C_RED,
                (double)total_leaked_bytes / 1024.0, leak_count, C_RESET);
    } else {
        fprintf(stderr, "%s  ║%s  %sTotal leaked: %zu bytes in %zu block(s)%s\n",
                C_YELLOW, C_RESET, C_RED,
                total_leaked_bytes, leak_count, C_RESET);
    }

    fprintf(stderr, "%s%s", C_YELLOW, C_BOLD);
    fprintf(stderr, "  ╚═══════════════════════════════════════════════════╝\n");
    fprintf(stderr, "%s\n", C_RESET);
}
