/*
 * SmartHeap - Custom Memory Allocator (Windows)
 * platform.h - OS Memory Interface
 *
 * Uses Windows VirtualAlloc/VirtualFree for requesting and releasing
 * memory from the operating system.
 */

#ifndef SMARTHEAP_PLATFORM_H
#define SMARTHEAP_PLATFORM_H

#include <stddef.h>

/*
 * Request a chunk of memory from the OS via VirtualAlloc.
 * The returned memory is page-aligned and zero-initialized.
 *
 * @param size  Number of bytes to allocate (rounded up to page size)
 * @return      Pointer to allocated memory, or NULL on failure
 */
void *platform_alloc(size_t size);

/*
 * Release a chunk of memory back to the OS via VirtualFree.
 *
 * @param ptr   Pointer to the memory to release
 * @param size  Size of the memory region
 */
void platform_free(void *ptr, size_t size);

/*
 * Get the system page size (typically 4096 bytes).
 *
 * @return  Page size in bytes
 */
size_t platform_page_size(void);

/*
 * Round a size up to the nearest multiple of the system page size.
 *
 * @param size  Size to round up
 * @return      Rounded-up size
 */
size_t platform_round_to_page(size_t size);

#endif /* SMARTHEAP_PLATFORM_H */
