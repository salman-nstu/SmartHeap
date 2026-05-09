/*
 * SmartHeap - Thread-Safe Custom Memory Allocator
 * test_runner.c - Test Framework & Harness
 *
 * A minimal test framework with no external dependencies.
 * Runs all test suites and reports pass/fail with colors.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <windows.h>

/* ── ANSI Colors ─────────────────────────────────────────────── */

#define C_RESET    "\033[0m"
#define C_BOLD     "\033[1m"
#define C_RED      "\033[91m"
#define C_GREEN    "\033[92m"
#define C_YELLOW   "\033[93m"
#define C_CYAN     "\033[96m"
#define C_WHITE    "\033[97m"

/* ── Test Framework ──────────────────────────────────────────── */

static int g_tests_run    = 0;
static int g_tests_passed = 0;
static int g_tests_failed = 0;
static const char *g_current_suite = "";

#define TEST_SUITE(name) \
    do { g_current_suite = (name); \
    printf("\n%s%s  ▶ %s%s\n", C_BOLD, C_CYAN, name, C_RESET); } while(0)

#define TEST_CASE(name) \
    do { g_tests_run++; \
    printf("    %s%-50s%s", C_WHITE, name, C_RESET); } while(0)

#define TEST_PASS() \
    do { g_tests_passed++; \
    printf("%s[PASS]%s\n", C_GREEN, C_RESET); } while(0)

#define TEST_FAIL(msg) \
    do { g_tests_failed++; \
    printf("%s[FAIL]%s %s\n", C_RED, C_RESET, msg); } while(0)

#define ASSERT(cond, msg) \
    do { if (!(cond)) { TEST_FAIL(msg); return; } } while(0)

#define ASSERT_NOT_NULL(ptr) \
    ASSERT((ptr) != NULL, "Expected non-NULL pointer")

#define ASSERT_NULL(ptr) \
    ASSERT((ptr) == NULL, "Expected NULL pointer")

#define ASSERT_EQ(a, b) \
    ASSERT((a) == (b), "Values not equal")

/* ── Include SmartHeap ───────────────────────────────────────── */

#define SH_TRACK_LEAKS
#include "smartheap.h"

/* ══════════════════════════════════════════════════════════════
 *  TEST SUITE: malloc
 * ══════════════════════════════════════════════════════════════ */

static void test_malloc_basic(void)
{
    TEST_CASE("malloc: basic allocation");
    sh_init();

    void *ptr = sh_malloc(64);
    ASSERT_NOT_NULL(ptr);

    /* Write to the allocated memory to verify it's usable */
    memset(ptr, 0xAA, 64);

    sh_free(ptr);
    sh_destroy();
    TEST_PASS();
}

static void test_malloc_zero(void)
{
    TEST_CASE("malloc: zero-size returns NULL");
    sh_init();

    void *ptr = sh_malloc(0);
    ASSERT_NULL(ptr);

    sh_destroy();
    TEST_PASS();
}

static void test_malloc_multiple(void)
{
    TEST_CASE("malloc: multiple allocations");
    sh_init();

    void *p1 = sh_malloc(32);
    void *p2 = sh_malloc(64);
    void *p3 = sh_malloc(128);

    ASSERT_NOT_NULL(p1);
    ASSERT_NOT_NULL(p2);
    ASSERT_NOT_NULL(p3);

    /* Pointers should be different */
    ASSERT(p1 != p2, "p1 == p2");
    ASSERT(p2 != p3, "p2 == p3");
    ASSERT(p1 != p3, "p1 == p3");

    sh_free(p1);
    sh_free(p2);
    sh_free(p3);
    sh_destroy();
    TEST_PASS();
}

static void test_malloc_large(void)
{
    TEST_CASE("malloc: large allocation (2MB)");
    sh_init();

    void *ptr = sh_malloc(2 * 1024 * 1024);
    ASSERT_NOT_NULL(ptr);

    /* Write first and last byte */
    ((char *)ptr)[0] = 'A';
    ((char *)ptr)[2 * 1024 * 1024 - 1] = 'Z';

    sh_free(ptr);
    sh_destroy();
    TEST_PASS();
}

static void test_malloc_alignment(void)
{
    TEST_CASE("malloc: 16-byte alignment");
    sh_init();

    for (int i = 1; i <= 100; i++) {
        void *ptr = sh_malloc(i);
        ASSERT_NOT_NULL(ptr);
        ASSERT(((size_t)ptr % 16) == 0, "Pointer not 16-byte aligned");
        sh_free(ptr);
    }

    sh_destroy();
    TEST_PASS();
}

/* ══════════════════════════════════════════════════════════════
 *  TEST SUITE: free
 * ══════════════════════════════════════════════════════════════ */

static void test_free_null(void)
{
    TEST_CASE("free: NULL is a no-op");
    sh_init();
    sh_free(NULL); /* Should not crash */
    sh_destroy();
    TEST_PASS();
}

static void test_free_and_reuse(void)
{
    TEST_CASE("free: freed memory is reused");
    sh_init();

    void *p1 = sh_malloc(64);
    ASSERT_NOT_NULL(p1);
    sh_free(p1);

    void *p2 = sh_malloc(64);
    ASSERT_NOT_NULL(p2);

    /* p2 should reuse p1's block (same address) */
    ASSERT(p1 == p2, "Expected block reuse");

    sh_free(p2);
    sh_destroy();
    TEST_PASS();
}

static void test_free_coalesce(void)
{
    TEST_CASE("free: adjacent blocks coalesce");
    sh_init();

    void *p1 = sh_malloc(32);
    void *p2 = sh_malloc(32);
    void *p3 = sh_malloc(32);
    ASSERT_NOT_NULL(p1);
    ASSERT_NOT_NULL(p2);
    ASSERT_NOT_NULL(p3);

    /* Free middle, then neighbors — should coalesce */
    sh_free(p2);
    sh_free(p1);
    sh_free(p3);

    /* Now a single allocation of 96 should fit in the coalesced block */
    void *p4 = sh_malloc(96);
    ASSERT_NOT_NULL(p4);

    sh_free(p4);
    sh_destroy();
    TEST_PASS();
}

/* ══════════════════════════════════════════════════════════════
 *  TEST SUITE: calloc
 * ══════════════════════════════════════════════════════════════ */

static void test_calloc_basic(void)
{
    TEST_CASE("calloc: zero-initialized allocation");
    sh_init();

    int *arr = (int *)sh_calloc(10, sizeof(int));
    ASSERT_NOT_NULL(arr);

    for (int i = 0; i < 10; i++)
        ASSERT(arr[i] == 0, "calloc memory not zeroed");

    sh_free(arr);
    sh_destroy();
    TEST_PASS();
}

static void test_calloc_zero_params(void)
{
    TEST_CASE("calloc: zero num or size returns NULL");
    sh_init();

    ASSERT_NULL(sh_calloc(0, 10));
    ASSERT_NULL(sh_calloc(10, 0));

    sh_destroy();
    TEST_PASS();
}

static void test_calloc_overflow(void)
{
    TEST_CASE("calloc: overflow detection");
    sh_init();

    /* This should overflow and return NULL */
    void *ptr = sh_calloc((size_t)-1, 2);
    ASSERT_NULL(ptr);

    sh_destroy();
    TEST_PASS();
}

/* ══════════════════════════════════════════════════════════════
 *  TEST SUITE: realloc
 * ══════════════════════════════════════════════════════════════ */

static void test_realloc_grow(void)
{
    TEST_CASE("realloc: grow preserves data");
    sh_init();

    char *ptr = (char *)sh_malloc(32);
    ASSERT_NOT_NULL(ptr);
    memcpy(ptr, "Hello, SmartHeap!", 18);

    ptr = (char *)sh_realloc(ptr, 128);
    ASSERT_NOT_NULL(ptr);
    ASSERT(memcmp(ptr, "Hello, SmartHeap!", 18) == 0, "Data not preserved");

    sh_free(ptr);
    sh_destroy();
    TEST_PASS();
}

static void test_realloc_shrink(void)
{
    TEST_CASE("realloc: shrink (returns same block)");
    sh_init();

    char *ptr = (char *)sh_malloc(128);
    ASSERT_NOT_NULL(ptr);
    memcpy(ptr, "Test", 5);

    char *ptr2 = (char *)sh_realloc(ptr, 32);
    ASSERT_NOT_NULL(ptr2);
    /* Should be same pointer (block already large enough) */
    ASSERT(ptr == ptr2, "Expected same pointer for shrink");
    ASSERT(memcmp(ptr2, "Test", 5) == 0, "Data not preserved");

    sh_free(ptr2);
    sh_destroy();
    TEST_PASS();
}

static void test_realloc_null(void)
{
    TEST_CASE("realloc: NULL ptr acts like malloc");
    sh_init();

    void *ptr = sh_realloc(NULL, 64);
    ASSERT_NOT_NULL(ptr);

    sh_free(ptr);
    sh_destroy();
    TEST_PASS();
}

static void test_realloc_zero_size(void)
{
    TEST_CASE("realloc: zero size acts like free");
    sh_init();

    void *ptr = sh_malloc(64);
    ASSERT_NOT_NULL(ptr);

    void *result = sh_realloc(ptr, 0);
    ASSERT_NULL(result);

    sh_destroy();
    TEST_PASS();
}

/* ══════════════════════════════════════════════════════════════
 *  TEST SUITE: strategies
 * ══════════════════════════════════════════════════════════════ */

static void test_strategy_first_fit(void)
{
    TEST_CASE("strategy: first fit finds first block");
    sh_init();
    sh_set_strategy(SH_STRATEGY_FIRST_FIT);

    void *p1 = sh_malloc(100);
    void *p2 = sh_malloc(200);
    void *p3 = sh_malloc(100);

    sh_free(p1);  /* Create 100-byte hole */
    sh_free(p3);  /* Create another 100-byte hole */

    /* First fit should reuse p1's block (first hole found) */
    void *p4 = sh_malloc(50);
    ASSERT(p4 == p1, "First fit should reuse first free block");

    sh_free(p2);
    sh_free(p4);
    sh_destroy();
    TEST_PASS();
}

static void test_strategy_best_fit(void)
{
    TEST_CASE("strategy: best fit finds smallest block");
    sh_init();
    sh_set_strategy(SH_STRATEGY_FIRST_FIT);

    /* Allocate and free to create blocks of known sizes */
    void *p1 = sh_malloc(200);  /* 200-byte block */
    void *p2 = sh_malloc(50);   /* spacer */
    void *p3 = sh_malloc(100);  /* 100-byte block */
    void *p4 = sh_malloc(50);   /* spacer */

    sh_free(p1);  /* Free 200-byte block */
    sh_free(p3);  /* Free 100-byte block */

    sh_set_strategy(SH_STRATEGY_BEST_FIT);

    /* Best fit should pick the 100-byte block for a 80-byte request */
    void *p5 = sh_malloc(80);
    ASSERT_NOT_NULL(p5);
    /* p5 should be at p3's address (smaller fitting block) */
    ASSERT(p5 == p3, "Best fit should choose smallest fitting block");

    sh_free(p2);
    sh_free(p4);
    sh_free(p5);
    sh_destroy();
    TEST_PASS();
}

static void test_strategy_worst_fit(void)
{
    TEST_CASE("strategy: worst fit finds largest block");
    sh_init();
    sh_set_strategy(SH_STRATEGY_FIRST_FIT);

    /* Create two free blocks of different sizes separated by spacers */
    void *p1 = sh_malloc(100);   /* Will become small free block */
    void *s1 = sh_malloc(50);    /* spacer */
    void *p2 = sh_malloc(300);   /* Will become large free block */
    void *s2 = sh_malloc(50);    /* spacer to prevent coalescing with tail */
    (void)s2; /* keep it allocated */

    sh_free(p1);  /* small free: ~112 bytes */
    sh_free(p2);  /* large free: ~304 bytes */

    /* First fit would pick p1 (first found). Worst fit should NOT pick p1. */
    sh_set_strategy(SH_STRATEGY_WORST_FIT);
    void *p3 = sh_malloc(80);
    ASSERT_NOT_NULL(p3);
    /* Worst fit should NOT pick the first (smaller) block */
    ASSERT(p3 != p1, "Worst fit should not pick the smaller first block");

    sh_free(s1);
    sh_free(s2);
    sh_free(p3);
    sh_destroy();
    TEST_PASS();
}

/* ══════════════════════════════════════════════════════════════
 *  TEST SUITE: stress
 * ══════════════════════════════════════════════════════════════ */

static void test_stress_random(void)
{
    TEST_CASE("stress: 10000 random alloc/free cycles");
    sh_init();

    #define STRESS_SIZE 10000
    void *ptrs[1024];
    int count = 0;

    srand(12345);

    for (int i = 0; i < STRESS_SIZE; i++) {
        if (count > 0 && (rand() % 3 == 0)) {
            int idx = rand() % count;
            sh_free(ptrs[idx]);
            ptrs[idx] = ptrs[--count];
        } else if (count < 1024) {
            size_t sz = 1 + (rand() % 4096);
            void *p = sh_malloc(sz);
            ASSERT_NOT_NULL(p);
            /* Write to verify usability */
            memset(p, 0xBB, sz);
            ptrs[count++] = p;
        }
    }

    /* Free remaining */
    for (int i = 0; i < count; i++)
        sh_free(ptrs[i]);

    sh_destroy();
    TEST_PASS();
}

static void test_stress_realloc(void)
{
    TEST_CASE("stress: 1000 realloc cycles");
    sh_init();

    char *ptr = (char *)sh_malloc(16);
    ASSERT_NOT_NULL(ptr);
    memset(ptr, 'A', 16);

    for (int i = 0; i < 1000; i++) {
        size_t new_size = 16 + (rand() % 2048);
        char *new_ptr = (char *)sh_realloc(ptr, new_size);
        ASSERT_NOT_NULL(new_ptr);
        ptr = new_ptr;
    }

    sh_free(ptr);
    sh_destroy();
    TEST_PASS();
}

/* ══════════════════════════════════════════════════════════════
 *  TEST SUITE: leak detection
 * ══════════════════════════════════════════════════════════════ */

static void test_leak_none(void)
{
    TEST_CASE("leak: no leaks when all freed");
    sh_init();

    void *p1 = sh_malloc(64);
    void *p2 = sh_malloc(128);
    sh_free(p1);
    sh_free(p2);

    size_t leaks = sh_leak_check();
    ASSERT(leaks == 0, "Expected zero leaks");

    sh_destroy();
    TEST_PASS();
}

static void test_leak_detected(void)
{
    TEST_CASE("leak: intentional leaks detected");
    sh_init();

    void *p1 = sh_malloc(64);   /* intentional leak */
    void *p2 = sh_malloc(128);  /* intentional leak */
    void *p3 = sh_malloc(256);
    sh_free(p3);                 /* only free one */

    size_t leaks = sh_leak_check();
    ASSERT(leaks == 2, "Expected 2 leaks");

    /* Clean up for test framework */
    sh_free(p1);
    sh_free(p2);
    sh_destroy();
    TEST_PASS();
}

/* ══════════════════════════════════════════════════════════════
 *  TEST SUITE: visualization
 * ══════════════════════════════════════════════════════════════ */

static void test_visualize(void)
{
    TEST_CASE("visualize: print map (visual check)");
    sh_init();

    void *p1 = sh_malloc(64);
    void *p2 = sh_malloc(128);
    void *p3 = sh_malloc(256);
    sh_free(p2); /* Create a hole */

    printf("\n");
    sh_visualize();

    sh_free(p1);
    sh_free(p3);
    sh_destroy();
    TEST_PASS();
}

static void test_stats(void)
{
    TEST_CASE("stats: print statistics (visual check)");
    sh_init();

    void *p1 = sh_malloc(100);
    void *p2 = sh_malloc(200);
    void *p3 = sh_malloc(300);
    sh_free(p2);

    printf("\n");
    sh_print_stats();

    sh_free(p1);
    sh_free(p3);
    sh_destroy();
    TEST_PASS();
}

/* ══════════════════════════════════════════════════════════════
 *  TEST SUITE: thread safety
 * ══════════════════════════════════════════════════════════════ */

#define THREAD_COUNT 4
#define OPS_PER_THREAD 2500

static DWORD WINAPI thread_worker(LPVOID arg)
{
    (void)arg;

    void *ptrs[64];
    int count = 0;

    for (int i = 0; i < OPS_PER_THREAD; i++) {
        if (count > 0 && (rand() % 2 == 0)) {
            int idx = rand() % count;
            sh_free(ptrs[idx]);
            ptrs[idx] = ptrs[--count];
        } else if (count < 64) {
            size_t sz = 1 + (rand() % 512);
            void *p = sh_malloc(sz);
            if (p) {
                memset(p, 0xCC, sz);
                ptrs[count++] = p;
            }
        }
    }

    for (int i = 0; i < count; i++)
        sh_free(ptrs[i]);

    return 0;
}

static void test_thread_safety(void)
{
    TEST_CASE("thread: concurrent alloc/free (4 threads)");
    sh_init();

    HANDLE threads[THREAD_COUNT];

    for (int i = 0; i < THREAD_COUNT; i++)
        threads[i] = CreateThread(NULL, 0, thread_worker, NULL, 0, NULL);

    WaitForMultipleObjects(THREAD_COUNT, threads, TRUE, INFINITE);

    for (int i = 0; i < THREAD_COUNT; i++)
        CloseHandle(threads[i]);

    size_t leaks = sh_leak_check();
    ASSERT(leaks == 0, "Leaks detected after threaded test");

    sh_destroy();
    TEST_PASS();
}

/* ══════════════════════════════════════════════════════════════
 *  MAIN — Run All Tests
 * ══════════════════════════════════════════════════════════════ */

int main(void)
{
    /* Enable ANSI escape codes + UTF-8 in Windows Terminal */
    SetConsoleOutputCP(65001);
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    GetConsoleMode(hOut, &mode);
    SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);

    printf("\n");
    printf("%s%s", C_YELLOW, C_BOLD);
    printf("  ╔══════════════════════════════════════════════════╗\n");
    printf("  ║         SmartHeap Test Suite                     ║\n");
    printf("  ╚══════════════════════════════════════════════════╝\n");
    printf("%s\n", C_RESET);

    /* malloc tests */
    TEST_SUITE("malloc");
    test_malloc_basic();
    test_malloc_zero();
    test_malloc_multiple();
    test_malloc_large();
    test_malloc_alignment();

    /* free tests */
    TEST_SUITE("free");
    test_free_null();
    test_free_and_reuse();
    test_free_coalesce();

    /* calloc tests */
    TEST_SUITE("calloc");
    test_calloc_basic();
    test_calloc_zero_params();
    test_calloc_overflow();

    /* realloc tests */
    TEST_SUITE("realloc");
    test_realloc_grow();
    test_realloc_shrink();
    test_realloc_null();
    test_realloc_zero_size();

    /* strategy tests */
    TEST_SUITE("strategies");
    test_strategy_first_fit();
    test_strategy_best_fit();
    test_strategy_worst_fit();

    /* stress tests */
    TEST_SUITE("stress");
    test_stress_random();
    test_stress_realloc();

    /* leak detection tests */
    TEST_SUITE("leak detection");
    test_leak_none();
    test_leak_detected();

    /* visualization tests */
    TEST_SUITE("visualization");
    test_visualize();
    test_stats();

    /* thread safety tests */
    TEST_SUITE("thread safety");
    test_thread_safety();

    /* Summary */
    printf("\n");
    printf("%s%s", C_YELLOW, C_BOLD);
    printf("  ════════════════════════════════════════════════════\n");
    printf("%s", C_RESET);
    printf("  %sTotal: %d tests%s | ", C_WHITE, g_tests_run, C_RESET);
    printf("%s%d passed%s | ", C_GREEN, g_tests_passed, C_RESET);
    printf("%s%d failed%s\n", g_tests_failed > 0 ? C_RED : C_GREEN, g_tests_failed, C_RESET);
    printf("%s%s", C_YELLOW, C_BOLD);
    printf("  ════════════════════════════════════════════════════\n");
    printf("%s\n", C_RESET);

    return g_tests_failed > 0 ? 1 : 0;
}
