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
 * validate that block is within heap's terrain, returns null if invalid
 * return the pointer to the superblock where this block was given birth
 */
superblock_h_t *retrieve_mamablock(block_h_t *bptr) {}

/*
 * retrieve memory block from the buddy system, or from mmapped regions;
 * for mmapped regions, unmap it; for buddy blocks, merge it with parent
 * buddy;
 */
void __lib_free(void *mem_ptr)
{
    if (initialize_malloc() != SUCCESS) {
        errno = ENOMEM;
        return;
    }

    superblock_h_t *mama_s;
    block_h_t *block_ptr = (block_h_t *)((char *)mem_ptr - sizeof(block_h_t));
    uint8_t sc = block_ptr->size_class;

    // validate & retrieve superblock
    if ((mama_s = retrieve_mamablock(block_ptr)) == NULL) {
        return;
    }

    // destory immediately if large
    if (sc > MAX_BINS) {
        // TODO: destory the block, unmmap it
        return;
    }

    //
    return;
}
void free(void *mem_ptr) __attribute__((weak, alias("__lib_free")));