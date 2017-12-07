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
int sys_core_count = SYS_CORE_COUNT;
int malloc_initialized = 0;

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
int size_to_class(size_t size)
{
    if (size > MAX_LRG_SIZE) return -1;
    if (size > MAX_SML_SIZE) return LRG_SIZE_CLASS(size);
    return SML_SIZE_CLASS(size);
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

    // confirm number of cores
    if ((sys_core_count = sysconf(_SC_NPROCESSORS_ONLN)) == -1)
        sys_core_count = SYS_CORE_COUNT;

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
    int class = size_to_class(132435);
    printf("current class is %d\n", class);
}

/*
 * assign hook; check for initialize state for current thread;
 * convert size to order, and call corresponding handler;
 */
void *__lib_malloc(size_t size)
{
    block_h_t *ret_addr = NULL;

    // hook & thread ini
    __malloc_hook_t lib_hook = __malloc_hook;
    if (lib_hook != NULL) {
        return (*lib_hook)(size, __builtin_return_address(0));
    }

    return (void *)ret_addr;
}

void *malloc(size_t size) __attribute__((weak, alias("__lib_malloc")));