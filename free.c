#include <assert.h>
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

__free_hook_t __free_hook = (__free_hook_t)initialize_free;

// if main arena not initialized, do it
void initialize_free(void *ptr, const void *caller)
{
    if (initialize_malloc()) {
        return;
    }

    __free_hook = NULL;
    free(ptr);
    return;
}

/*
 * retrieve memory block from the buddy system, or from mmapped regions;
 * for mmapped regions, unmap it; for buddy blocks, merge it with parent buddy;
 */
void __lib_free(void *mem_ptr)
{
    __free_hook_t lib_hook = __free_hook;
    if (lib_hook != NULL) {
        return (*lib_hook)(mem_ptr, __builtin_return_address(0));
    }
    // do something
}
void free(void *mem_ptr) __attribute__((weak, alias("__lib_free")));