#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include "common.h"

// ini globals
long sys_page_size = SYS_PAGE_SIZE;
int sys_page_shift = 16;
int sys_core_count = SYS_CORE_COUNT;
int malloc_initialized = 0;
char class_array_[FLAT_CLASS_NO];
size_t class_to_size_[MAX_BINS];
size_t class_to_pages_[MAX_BINS];

// per thread global
__thread int restartable;

// hook
__malloc_hook_t __malloc_hook = (__malloc_hook_t)initialize_lib;

// Sizes <= 1024 have an alignment >= 8.  So for such sizes we have an
// array indexed by ceil(size/8).  Sizes > 1024 have an alignment >= 128.
// So for these larger sizes we have an array indexed by ceil(size/128).
//
// We flatten both logical arrays into one physical array and use
// arithmetic to compute an appropriate index.
//
// Examples:
//   Size       Expression                      Index
//   -------------------------------------------------------
//   0          (0 + 7) / 8                     0
//   1          (1 + 7) / 8                     1
//   ...
//   1024       (1024 + 7) / 8                  128
//   1025       (1025 + 127 + (120<<7)) / 128   129
//   ...
//   32768      (32768 + 127 + (120<<7)) / 128  376
int class_index(size_t size)
{
    if (size > MAX_LRG_SIZE) return -1;
    if (size > MAX_SML_SIZE) return LRG_SIZE_CLASS_IDX(size);
    return SML_SIZE_CLASS_IDX(size);
}

// only for size < 32 bits
int lg_floor(size_t s)
{
    int i, log = 0;
    for (i = 4; i >= 0; --i) {
        int shift = (1 << i);
        size_t x = s >> shift;
        if (x != 0) {
            s = x;
            log += shift;
        }
    }
    return log;
}

int size_to_no_blocks(size_t size)
{
    if (size == 0) return 0;
    // Use approx 64k transfers between thread and central caches.
    int num = (int)(64.0 * 1024.0 / size);
    if (num < 2) num = 2;
    if (num > 32) num = 32;
    return num;
}

// convert size class sizes to next level alignments
int size_to_alignment(size_t size)
{
    int alignment = SML_ALIGN;
    if (size > MAX_LRG_SIZE) {
        // Cap alignment at page size for large sizes.
        alignment = sys_page_size;
    } else if (size >= 128) {
        // Space wasted due to alignment is at most 1/8, i.e., 12.5%.
        alignment = (1 << lg_floor(size)) / 8;
    } else if (size >= REAL_SML_ALIGN) {
        // We need an alignment of at least 16 bytes to satisfy
        // requirements for some SSE types.
        alignment = REAL_SML_ALIGN;
    }
    // Maximum alignment allowed is page size alignment.
    if (alignment > sys_page_size) {
        alignment = sys_page_size;
    }
    return alignment;
}

// initialize class_array_, class_to_size_, and class_to_pages_
int initialize_size_classes()
{
    // Compute the size classes we want to use
    int sc = 1;  // Next size class to assign
    int alignment = REAL_SML_ALIGN;
    size_t size;
    for (size = REAL_SML_ALIGN; size <= MAX_LRG_SIZE; size += alignment) {
        alignment = size_to_alignment(size);

        int blocks_to_move = size_to_no_blocks(size) / 4;
        size_t psize = 0;
        do {
            psize += sys_page_size;
            // Allocate enough pages so leftover is less than 1/8 of total.
            // This bounds wasted space to at most 12.5%.
            while ((psize % size) > (psize >> 3)) {
                psize += sys_page_size;
            }
            // Continue to add pages until there are at least as many objects in
            // the span as are needed when moving objects from the central
            // freelists and spans to the thread caches.
        } while ((psize / size) < (blocks_to_move));
        size_t my_pages = psize >> sys_page_shift;

        if (sc > 1 && my_pages == class_to_pages_[sc - 1]) {
            // See if we can merge this into the previous class without
            // increasing the fragmentation of the previous class.
            size_t my_objects = (my_pages << sys_page_shift) / size;
            size_t prev_objects = (class_to_pages_[sc - 1] << sys_page_shift) /
                                  class_to_size_[sc - 1];
            if (my_objects == prev_objects) {
                // Adjust last class to include this size
                class_to_size_[sc - 1] = size;
                continue;
            }
        }

        // Add new class
        class_to_pages_[sc] = my_pages;
        class_to_size_[sc] = size;
        sc++;
    }

    // mapping arrays
    int next_size = 0;
    int num_size_classes = sc;
    int c;
    for (c = 1; c < num_size_classes; c++) {
        int max_size_in_class = class_to_size_[c];
        int s;
        for (s = next_size; s <= max_size_in_class; s += SML_ALIGN) {
            class_array_[class_index(s)] = c;
        }
        next_size = max_size_in_class + SML_ALIGN;
    }

    printf(
        "Below is a list of (size class idx, max bytes allowed in class) "
        "pairs\n");
    int i;
    for (i = 0; i < num_size_classes; i++) {
        printf("%d:%d ", i, (int)class_to_size_[i]);
    }
    printf(
        "\nBelow is a list of (size class idx, number of pages needed) "
        "pairs\n");
    for (i = 0; i < num_size_classes; i++) {
        printf("%d:%d ", i, (int)class_to_pages_[i]);
    }
    printf("\n");
    return SUCCESS;
}

/*
 * initialize libmalloc; takes place on the first malloc call;
 * create heap for each CPU core;
 */
void *initialize_lib(size_t size, const void *caller)
{
    if (initialize_malloc()) {
        return NULL;
    }
    __malloc_hook = NULL;
    return malloc(size);
}

/*
 * confirm SYS_PAGE_SIZE and SYS_CORE_COUNT;
 * initialize HEAP per CPU core, plus global HEAP;
 */
int initialize_malloc()
{
    int out = SUCCESS;

    if (malloc_initialized) return out;

    // confirm global variables
    if ((sys_page_size = sysconf(_SC_PAGESIZE)) == -1)
        sys_page_size = SYS_PAGE_SIZE;
    sys_page_shift = (int)(log(sys_page_size) / log(2));

    // confirm number of cores
    if ((sys_core_count = sysconf(_SC_NPROCESSORS_ONLN)) == -1)
        sys_core_count = SYS_CORE_COUNT;

    // ini size class mappings
    if ((out = initialize_size_classes()) == FAILURE) {
        errno = ENOMEM;
        return out;
    }

    // ini arena meta data
    if ((out = initialize_heaps()) == FAILURE) {
        errno = ENOMEM;
        return out;
    }

    // raise flag & set stats
    malloc_initialized = 1;
    return out;
}

/*
 * create a global heap, one heap per core;
 * add a super block per (heap, size class);
 * break super block to free blocks;
 */
int initialize_heaps()
{
    int cpu = sched_getcpu();
    printf("currently running on %d\n", cpu);
}

/*
 * assign hook; check for initialize state for current thread;
 * convert size to order, and call corresponding handler;
 */
void *__lib_malloc(size_t size)
{
    block_h_t *ret_addr = NULL;
    void *mmapped;

    // hook & thread ini
    __malloc_hook_t lib_hook = __malloc_hook;
    if (lib_hook != NULL) {
        return (*lib_hook)(size, __builtin_return_address(0));
    }

    if ((mmapped = (mmap(NULL, size, PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS, -1, 0))) == MAP_FAILED) {
        errno = ENOMEM;
        return NULL;
    }

    ret_addr = (block_h_t *)mmapped;
    return mmapped;
}

void *malloc(size_t size) __attribute__((weak, alias("__lib_malloc")));