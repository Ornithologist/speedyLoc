#define _GNU_SOURCE

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
superblock_h_t *retrieve_mamablock(block_h_t *bptr)
{
    size_t sc = bptr->size_class;
    size_t pages = class_to_pages_[sc];
    int psize = (int)pages * sys_page_size, i;
    superblock_h_t *itr = NULL;
    for (i = 0; i <= sys_core_count; i++) {
        itr = cpu_heaps[i].bins[sc];
        superblock_h_t *prev_itr = itr;
        while (itr != NULL && ((char *)itr >= (char *)bptr ||
                               (char *)itr + psize <= (char *)bptr)) {
            prev_itr = itr;
            itr = itr->next;
        }
        if (itr != NULL) {
            break;
        }
    }
    return itr;
}

/*
 * returns 0 if slow path is taken;
 * returns 1 if fast path is taken;
 */
int restartable_critical_section_free(superblock_h_t *mama_s, block_h_t *bptr)
{
    restartable = 1;
    int path = 0;
    size_t sc = bptr->size_class;

    // get current CPU id
    my_cpu = sched_getcpu();
    if (my_cpu < 0) {
        restartable = 0;
        return path;
    }

    // check if mama_s (further implies bptr's locality) is local
    heap_h_t hp = cpu_heaps[my_cpu];
    superblock_h_t *local_sbptr = hp.bins[sc];
    if (local_sbptr == NULL || ((char *)local_sbptr - (char *)mama_s) != 0) {
        restartable = 0;
        return path;
    }

    // bptr is local, link it to local_head
    bptr->next = (block_h_t *)local_sbptr->local_head;
    local_sbptr->local_head = (void *)bptr;

    // update flag and stats
    local_sbptr->in_use_count--;
    restartable = 0;
    path = 1;
    return path;
}

/*
 * lock mama superblock;
 * push to-be-freed block to the remote free list of its mama superblock
 */
void add_block_to_remote(superblock_h_t *mama_s, block_h_t *bptr)
{
    pthread_mutex_lock(&mama_s->lock);
    bptr->next = (block_h_t *)mama_s->remote_head;
    mama_s->remote_head = (void *)bptr;
    pthread_mutex_unlock(&mama_s->lock);
}

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

    if (mem_ptr == NULL) return;
    superblock_h_t *mama_s;
    block_h_t *bptr = (block_h_t *)((char *)mem_ptr - sizeof(block_h_t));
    uint8_t sc = bptr->size_class;

    // destory and return immediately if large
    if (sc > MAX_BINS) {
        // destory the block, unmmap it
        size_t size = class_to_size_[sc];
        int res = munmap((void *)bptr, size);
        assert(res == 0);
        return;
    }

    // validate & retrieve superblock
    if ((mama_s = retrieve_mamablock(bptr)) == NULL) {
        return;
    }

    // FAST PATH: hit restartable critical section and return immediately
    int slow_path = restartable_critical_section_free(mama_s, bptr);
    if (slow_path != 0) {
        return;
    }

    // SLOW PATH: lock mama_s, add bptr to its 'remote' free list
    add_block_to_remote(mama_s, bptr);

    // TODO: destory mama_s,
    // when: mama_s->in_use_count = 0, and global_heap has too many
    // completely free superblocks (whose in_use_count = 0)

    return;
}
void free(void *mem_ptr) __attribute__((weak, alias("__lib_free")));