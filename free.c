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

/*
 * retrieve memory block from the buddy system, or from mmapped regions;
 * for mmapped regions, unmap it; for buddy blocks, merge it with parent buddy;
 */
void __lib_free(void *mem_ptr)
{
    if (initialize_malloc() != SUCCESS) {
        errno = ENOMEM;
        return NULL;
    }

    block_h_t *block_ptr = NULL;
    uint8_t sc;

    // validate that block is within heap's terrain
    // if ((block_ptr = validate_addr(mem_ptr)) == NULL) {
    //     return;
    // }

    // destory immediately if large
    if (sc > MAX_BINS) {
        // TODO: destory the block, unmmap it
        return;
    }

    // TODO: release small block
    return;
}
void free(void *mem_ptr) __attribute__((weak, alias("__lib_free")));