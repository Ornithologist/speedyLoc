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

__calloc_hook_t __calloc_hook = (__calloc_hook_t)initialize_calloc;

void *initialize_calloc(size_t nmemb, size_t size, const void *caller)
{
    // something to be done before
    if (initialize_malloc()) {
        return NULL;
    }
    __calloc_hook = NULL;
    return calloc(nmemb, size);
}

/*
 * allocate a region of memory of size (nmemb * size);
 * fill it with integer 0;
 */
void *__lib_calloc(size_t nmemb, size_t size)
{
    __calloc_hook_t lib_hook = __calloc_hook;
    if (lib_hook != NULL) {
        return (*lib_hook)(nmemb, size, __builtin_return_address(0));
    }

    if (nmemb == 0 || size == 0) return NULL;

    void *ret_ptr = __lib_malloc(nmemb * size);
    if (ret_ptr == NULL) {
        errno = ENOMEM;
        return NULL;
    }

    block_h_t *block_ptr = (block_h_t *)(ret_ptr - sizeof(block_h_t));
    size_t memset_size = size;  // FIXME: wrong!
    memset(ret_ptr, 0, memset_size);
    return ret_ptr;
}
void *calloc(size_t nmemb, size_t size)
    __attribute__((weak, alias("__lib_calloc")));