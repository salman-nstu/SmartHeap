/*
 * SmartHeap - Thread-Safe Custom Memory Allocator
 * leak_detector.h - Memory Leak Detection Interface
 *
 * Tracks allocation source locations using __FILE__ and __LINE__.
 * At program shutdown, reports all unfreed memory blocks with
 * their allocation origins.
 */

#ifndef SMARTHEAP_LEAK_DETECTOR_H
#define SMARTHEAP_LEAK_DETECTOR_H

#include "smartheap_internal.h"

/*
 * Perform a leak check: walk all blocks and report any that
 * are still marked as allocated.
 *
 * @param state  Global allocator state
 * @return       Number of leaked blocks found
 */
size_t leak_check(sh_state_t *state);

/*
 * Print a detailed leak report to stderr.
 *
 * @param state  Global allocator state
 */
void leak_report(sh_state_t *state);

#endif /* SMARTHEAP_LEAK_DETECTOR_H */
