/*
 * SmartHeap - Custom Memory Allocator (Windows)
 * platform_win32.c - Windows VirtualAlloc Implementation
 *
 * Uses VirtualAlloc/VirtualFree for direct OS memory management.
 * Memory is committed and reserved in a single call and is
 * automatically zero-initialized by the OS.
 */

#include "platform.h"
#include <windows.h>

void *platform_alloc(size_t size)
{
    size_t alloc_size = platform_round_to_page(size);
    void *ptr = VirtualAlloc(
        NULL,                          /* Let OS choose the address */
        alloc_size,
        MEM_COMMIT | MEM_RESERVE,      /* Reserve and commit in one step */
        PAGE_READWRITE                 /* Read/write access */
    );
    return ptr; /* Returns NULL on failure */
}

void platform_free(void *ptr, size_t size)
{
    (void)size; /* VirtualFree with MEM_RELEASE ignores size */
    if (ptr) {
        VirtualFree(ptr, 0, MEM_RELEASE);
    }
}

size_t platform_page_size(void)
{
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return (size_t)si.dwPageSize;
}

size_t platform_round_to_page(size_t size)
{
    size_t page = platform_page_size();
    return (size + page - 1) & ~(page - 1);
}
