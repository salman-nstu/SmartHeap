/*
 * SmartHeap - Thread-Safe Custom Memory Allocator
 * visualizer.c - Terminal Heap Visualizer
 *
 * Renders a visual map of all arenas and blocks using
 * ANSI escape codes and Unicode box-drawing characters.
 *
 * Color scheme:
 *   Red/Magenta  — Used (allocated) blocks
 *   Green/Cyan   — Free blocks
 *   Yellow       — Headers and borders
 */

#include "visualizer.h"
#include "platform.h"
#include <stdio.h>
#include <string.h>

#include <windows.h>

/* Enable ANSI escape codes + UTF-8 output on Windows Terminal */
static void enable_ansi(void)
{
    SetConsoleOutputCP(65001);
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    GetConsoleMode(hOut, &mode);
    SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
}

/* ── ANSI Color Codes ────────────────────────────────────────── */

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

#define C_BG_RED   "\033[41m"
#define C_BG_GREEN "\033[42m"
#define C_BG_BLUE  "\033[44m"
#define C_BG_GRAY  "\033[100m"

/* ── Unicode Box-Drawing ─────────────────────────────────────── */

#define BOX_TL     "\xe2\x95\x94"  /* ╔ */
#define BOX_TR     "\xe2\x95\x97"  /* ╗ */
#define BOX_BL     "\xe2\x95\x9a"  /* ╚ */
#define BOX_BR     "\xe2\x95\x9d"  /* ╝ */
#define BOX_H      "\xe2\x95\x90"  /* ═ */
#define BOX_V      "\xe2\x95\x91"  /* ║ */
#define BOX_MID_L  "\xe2\x95\xa0"  /* ╠ */
#define BOX_MID_R  "\xe2\x95\xa3"  /* ╣ */
#define BOX_THIN_H "\xe2\x94\x80"  /* ─ */
#define BOX_THIN_V "\xe2\x94\x82"  /* │ */

#define BLOCK_USED "\xe2\x96\x93"  /* ▓ */
#define BLOCK_FREE "\xe2\x96\x91"  /* ░ */

/* Forward declaration */
static const char *strategy_name_vis(sh_strategy_t strategy);

/* ── Helper: Format Size ─────────────────────────────────────── */

static const char *format_size(size_t bytes, char *buf, size_t buf_size)
{
    if (bytes >= 1048576)
        snprintf(buf, buf_size, "%.1f MB", (double)bytes / 1048576.0);
    else if (bytes >= 1024)
        snprintf(buf, buf_size, "%.1f KB", (double)bytes / 1024.0);
    else
        snprintf(buf, buf_size, "%zu B", bytes);
    return buf;
}

/* ── Helper: Print Horizontal Line ───────────────────────────── */

static void print_hline(const char *left, const char *fill, const char *right, int width)
{
    printf("%s%s", C_YELLOW, left);
    for (int i = 0; i < width; i++)
        printf("%s", fill);
    printf("%s%s\n", right, C_RESET);
}

/* ── Print Memory Map ────────────────────────────────────────── */

void visualizer_print_map(sh_state_t *state)
{
    int arena_num = 0;
    int width = 62;
    char size_buf[32];

    enable_ansi();

    printf("\n");
    print_hline(BOX_TL, BOX_H, BOX_TR, width);

    /* Title */
    printf("%s" BOX_V "%s", C_YELLOW, C_RESET);
    printf("%s%s         SmartHeap Memory Map         %s", C_BOLD, C_CYAN, C_RESET);
    /* Pad to width */
    for (int i = 0; i < width - 38; i++) printf(" ");
    printf("%s" BOX_V "%s\n", C_YELLOW, C_RESET);

    if (!state->arenas) {
        print_hline(BOX_MID_L, BOX_H, BOX_MID_R, width);
        printf("%s" BOX_V "%s", C_YELLOW, C_RESET);
        printf("  %s(empty - no arenas allocated)%s", C_DIM, C_RESET);
        for (int i = 0; i < width - 33; i++) printf(" ");
        printf("%s" BOX_V "%s\n", C_YELLOW, C_RESET);
        print_hline(BOX_BL, BOX_H, BOX_BR, width);
        printf("\n");
        return;
    }

    arena_t *arena = state->arenas;
    while (arena) {
        print_hline(BOX_MID_L, BOX_H, BOX_MID_R, width);

        /* Arena header */
        printf("%s" BOX_V "%s", C_YELLOW, C_RESET);
        printf(" %s%sArena %d%s ", C_BOLD, C_BLUE, arena_num, C_RESET);
        printf("(%s)", format_size(arena->total_size, size_buf, sizeof(size_buf)));
        int header_len = 11 + (int)strlen(size_buf) + 3;
        for (int i = header_len; i < width; i++) printf(" ");
        printf("%s" BOX_V "%s\n", C_YELLOW, C_RESET);

        /* Block visualization line */
        printf("%s" BOX_V "%s ", C_YELLOW, C_RESET);

        header_t *block = arena->first_block;
        int col = 1;
        int max_col = width - 1;

        while (block && col < max_col) {
            /* Calculate visual width: proportional to block size, min 3 chars */
            int total_data = (int)(arena->total_size - sizeof(arena_t));
            int visual_width = (int)((double)(block->s.size + sizeof(header_t)) / total_data * (max_col - 2));
            if (visual_width < 3) visual_width = 3;
            if (col + visual_width >= max_col) visual_width = max_col - col;

            if (block->s.is_free) {
                /* Free block: green with ░ fill */
                printf("%s%s", C_GREEN, C_BG_GREEN);
                for (int i = 0; i < visual_width; i++)
                    printf("%s", BLOCK_FREE);
                printf("%s", C_RESET);
            } else {
                /* Used block: red with ▓ fill */
                printf("%s%s", C_RED, C_BG_RED);
                for (int i = 0; i < visual_width; i++)
                    printf("%s", BLOCK_USED);
                printf("%s", C_RESET);
            }

            col += visual_width;
            block = block->s.next;
        }

        /* Pad remaining space */
        for (; col < max_col; col++) printf(" ");
        printf("%s" BOX_V "%s\n", C_YELLOW, C_RESET);

        /* Block detail line */
        printf("%s" BOX_V "%s ", C_YELLOW, C_RESET);
        block = arena->first_block;
        col = 1;
        int block_idx = 0;

        while (block && col < max_col) {
            int total_data = (int)(arena->total_size - sizeof(arena_t));
            int visual_width = (int)((double)(block->s.size + sizeof(header_t)) / total_data * (max_col - 2));
            if (visual_width < 3) visual_width = 3;
            if (col + visual_width >= max_col) visual_width = max_col - col;

            /* Format: size label centered in the visual width */
            char label[32];
            format_size(block->s.size, size_buf, sizeof(size_buf));
            snprintf(label, sizeof(label), "%s", size_buf);
            int label_len = (int)strlen(label);

            if (block->s.is_free)
                printf("%s", C_GREEN);
            else
                printf("%s", C_RED);

            if (label_len >= visual_width) {
                /* Truncate label */
                for (int i = 0; i < visual_width; i++)
                    printf("%c", label[i]);
            } else {
                int pad_left = (visual_width - label_len) / 2;
                int pad_right = visual_width - label_len - pad_left;
                for (int i = 0; i < pad_left; i++) printf(" ");
                printf("%s", label);
                for (int i = 0; i < pad_right; i++) printf(" ");
            }
            printf("%s", C_RESET);

            col += visual_width;
            block = block->s.next;
            block_idx++;
        }

        for (; col < max_col; col++) printf(" ");
        printf("%s" BOX_V "%s\n", C_YELLOW, C_RESET);

        arena = arena->next;
        arena_num++;
    }

    /* Legend */
    print_hline(BOX_MID_L, BOX_H, BOX_MID_R, width);
    printf("%s" BOX_V "%s", C_YELLOW, C_RESET);
    printf(" %s%s" BLOCK_USED BLOCK_USED "%s = Used  ", C_RED, C_BG_RED, C_RESET);
    printf("%s%s" BLOCK_FREE BLOCK_FREE "%s = Free  ", C_GREEN, C_BG_GREEN, C_RESET);
    printf("%sStrategy: %s%s", C_DIM, strategy_name_vis(state->strategy), C_RESET);
    int legend_len = 38 + (int)strlen(strategy_name_vis(state->strategy));
    for (int i = legend_len; i < width; i++) printf(" ");
    printf("%s" BOX_V "%s\n", C_YELLOW, C_RESET);

    /* Summary stats */
    size_t used_bytes = state->current_usage;
    size_t total_bytes = state->total_arena_memory;
    double util = total_bytes > 0 ? (double)used_bytes / total_bytes * 100.0 : 0.0;

    char used_buf[32], total_buf[32];
    printf("%s" BOX_V "%s", C_YELLOW, C_RESET);
    printf(" %sUsed: %s%s%s | Total: %s%s%s | Util: %s%.1f%%%s",
           C_WHITE,
           C_RED, format_size(used_bytes, used_buf, sizeof(used_buf)), C_RESET,
           C_BLUE, format_size(total_bytes, total_buf, sizeof(total_buf)), C_RESET,
           C_CYAN, util, C_RESET);
    /* Rough padding */
    int stats_len = 30 + (int)strlen(used_buf) + (int)strlen(total_buf);
    for (int i = stats_len; i < width; i++) printf(" ");
    printf("%s" BOX_V "%s\n", C_YELLOW, C_RESET);

    print_hline(BOX_BL, BOX_H, BOX_BR, width);
    printf("\n");
}

/* Local helper to get strategy name without including strategy.h */
static const char *strategy_name_vis(sh_strategy_t strategy)
{
    switch (strategy) {
    case SH_FIRST_FIT: return "First Fit";
    case SH_BEST_FIT:  return "Best Fit";
    case SH_WORST_FIT: return "Worst Fit";
    default:           return "Unknown";
    }
}

/* ── Print Compact Summary ───────────────────────────────────── */

void visualizer_print_summary(sh_state_t *state)
{
    char buf[32];
    enable_ansi();

    printf("\n%s%s=== SmartHeap Summary ===%s\n", C_BOLD, C_CYAN, C_RESET);

    int arena_num = 0;
    arena_t *arena = state->arenas;
    while (arena) {
        int used_blocks = 0, free_blocks = 0;
        size_t used_bytes = 0, free_bytes = 0;

        header_t *block = arena->first_block;
        while (block) {
            if (block->s.is_free) {
                free_blocks++;
                free_bytes += block->s.size;
            } else {
                used_blocks++;
                used_bytes += block->s.size;
            }
            block = block->s.next;
        }

        printf("  Arena %d: %s total | %s%s used%s (%d blocks) | %s%s free%s (%d blocks)\n",
               arena_num,
               format_size(arena->total_size, buf, sizeof(buf)),
               C_RED, format_size(used_bytes, buf, sizeof(buf)), C_RESET,
               used_blocks,
               C_GREEN, format_size(free_bytes, buf, sizeof(buf)), C_RESET,
               free_blocks);

        arena = arena->next;
        arena_num++;
    }

    if (arena_num == 0)
        printf("  %s(no arenas allocated)%s\n", C_DIM, C_RESET);

    printf("\n");
}

/* ── Print Block Detail ──────────────────────────────────────── */

void visualizer_print_block(header_t *header)
{
    char buf[32];
    enable_ansi();

    if (!header) {
        printf("  (null block)\n");
        return;
    }

    printf("  %sBlock #%u%s at %p\n", C_BOLD, header->s.alloc_id, C_RESET, (void *)header);
    printf("    Size:    %s\n", format_size(header->s.size, buf, sizeof(buf)));
    printf("    Status:  %s%s%s\n",
           header->s.is_free ? C_GREEN : C_RED,
           header->s.is_free ? "FREE" : "USED",
           C_RESET);
    printf("    Magic:   0x%08X %s\n", header->s.magic,
           header->s.magic == SH_MAGIC ? "(valid)" :
           header->s.magic == SH_FREED_MAGIC ? "(freed)" : "(CORRUPTED!)");

    if (header->s.alloc_file)
        printf("    Source:  %s:%d\n", header->s.alloc_file, header->s.alloc_line);

    printf("    Prev:    %p\n", (void *)header->s.prev);
    printf("    Next:    %p\n", (void *)header->s.next);
}

/* ── Print Full Block List (Debug) ───────────────────────────── */

void visualizer_print_list(sh_state_t *state)
{
    enable_ansi();

    printf("\n%s%s=== Block List ===%s\n", C_BOLD, C_CYAN, C_RESET);

    int arena_num = 0;
    arena_t *arena = state->arenas;
    while (arena) {
        printf("\n%sArena %d:%s\n", C_BLUE, arena_num, C_RESET);

        header_t *block = arena->first_block;
        int idx = 0;
        while (block) {
            printf("  [%d] ", idx);
            printf("%s%-5s%s ",
                   block->s.is_free ? C_GREEN : C_RED,
                   block->s.is_free ? "FREE" : "USED",
                   C_RESET);

            char buf[32];
            printf("size=%-8s ", format_size(block->s.size, buf, sizeof(buf)));
            printf("id=%-4u ", block->s.alloc_id);

            if (block->s.alloc_file)
                printf("(%s:%d)", block->s.alloc_file, block->s.alloc_line);

            printf("\n");

            block = block->s.next;
            idx++;
        }

        arena = arena->next;
        arena_num++;
    }
    printf("\n");
}
