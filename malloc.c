#define _GNU_SOURCE

#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <sched.h>
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
int sys_core_count = MAX_SYS_CORE_COUNT;
int malloc_initialized = 0;
int num_size_classes;
char class_array_[FLAT_CLASS_NO];
size_t class_to_size_[MAX_BINS];
size_t class_to_pages_[MAX_BINS];
heap_h_t cpu_heaps[MAX_SYS_CORE_COUNT + 1];

// per thread global
__thread int restartable = 0;
__thread int my_cpu = 0;

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

// utility function to find the number of blocks
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
    num_size_classes = sc;
    int c;
    for (c = 1; c < num_size_classes; c++) {
        int max_size_in_class = class_to_size_[c];
        int s;
        for (s = next_size; s <= max_size_in_class; s += SML_ALIGN) {
            class_array_[class_index(s)] = c;
        }
        next_size = max_size_in_class + SML_ALIGN;
    }

    return SUCCESS;
}

/*
 * confirm sys_page_size and sys_core_count;
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
        sys_core_count = MAX_SYS_CORE_COUNT;

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
    int i;  // when i = sysc_core_count, the heap is global
    for (i = 0; i <= sys_core_count; i++) {
        create_heap(&cpu_heaps[i], i);
    }
}

/*
 * create a heap for a particular core;
 * initiates the superblocks for each size class;
 */
void create_heap(heap_h_t *hp, int cpu)
{
    hp->cpu = cpu;
    int i;
    for (i = 1; i < num_size_classes; i++) {
        int sc = i;
        size_t pages = class_to_pages_[sc];
        size_t bk_size = class_to_size_[sc];
        hp->bins[i] = create_superblock(bk_size, sc, pages);
    }
    return;
}

/*
 * create a superblock for a given size class;
 * allocate $pages number of pages;
 * create linked list of blocks;
 */
superblock_h_t *create_superblock(size_t bk_size, int sc, int pages)
{
    // allocate pages to fill the superblock, with some wasted spaces
    int size_to_allocate = sys_page_size * pages + sizeof(superblock_h_t);
    int blocks_to_add = (sys_page_size * pages) / (int)bk_size;
    superblock_h_t *sbptr;
    if ((sbptr = (superblock_h_t *)sbrk(size_to_allocate)) == NULL) return NULL;

    // ini the superblock
    void *head_addr = (void *)((char *)sbptr + sizeof(superblock_h_t));
    sbptr->in_use_count = 0;
    sbptr->local_head = head_addr;
    sbptr->remote_head = NULL;
    sbptr->next = NULL;
    if (pthread_mutex_init(&sbptr->lock, NULL) != 0) {
        return NULL;
    }

    // create a linked list of blocks
    void *itr = head_addr;
    block_h_t *prev = NULL;
    while (itr < (head_addr + blocks_to_add * bk_size)) {
        // create cur
        block_h_t *cur = (block_h_t *)itr;
        cur->size_class = sc;
        cur->next = NULL;
        // link prev
        if (prev != NULL) prev->next = cur;
        // swap and move on
        prev = cur;
        itr += bk_size;
    }
    return sbptr;
}

/*
 * destorys an empty superblock and the mutext lock
 */
void destory_superblock(superblock_h_t *sbptr)
{
    if (pthread_mutex_destroy(&sbptr->lock) == 0) {
        sbptr = NULL;  // FIXME: really? this does not destory the instance
    }
}

/*
 * search from the head of global heap's superblock linked list
 * for a size class, find a superblock that has either non-null
 * local_head or non-null remote_head, else return NULL
 */
superblock_h_t *retrieve_superblock_from_global_heap(int sc)
{
    superblock_h_t *itr = cpu_heaps[sys_core_count].bins[sc], *prev_itr;
    while (itr != NULL && itr->local_head == NULL && itr->remote_head == NULL) {
        prev_itr = itr;
        itr = itr->next;
    }
    return itr;
}

/*
 * recursive call to fetch a free block for the requested sc;
 * keeps retrying until a superblock that fulfills the request
 * is found.
 */
block_h_t *search_local_block(int sc)
{
    // FAST PATH: find one in local free list
    block_h_t *bptr = restartable_critical_section(sc);
    if (bptr != NULL) return bptr;
    // SLOW PATH: super block is empty, search in global heap
    superblock_h_t *local_sbptr = cpu_heaps[my_cpu].bins[sc];
    superblock_h_t *global_sbptr = retrieve_superblock_from_global_heap(sc);
    if (global_sbptr == NULL) {
        // if all global superblocks are full, construct new
        size_t max_size = class_to_size_[sc];
        int pages = class_to_pages_[sc];
        global_sbptr = create_superblock(max_size, sc, pages);
    } else {
        // lock global_sbptr and merge its remote list into local list
        pthread_mutex_lock(&global_sbptr->lock);
        if (global_sbptr->local_head == NULL) {
            global_sbptr->local_head = global_sbptr->remote_head;
            global_sbptr->remote_head = NULL;
        } else if (global_sbptr->remote_head != NULL) {
            block_h_t *prev_itr, *itr = (block_h_t *)global_sbptr->local_head;
            while (itr != NULL) {
                prev_itr = itr;
                itr = itr->next;
            }
            prev_itr->next = (block_h_t *)global_sbptr->remote_head;
            global_sbptr->remote_head = NULL;
        }
        pthread_mutex_unlock(&global_sbptr->lock);
    }

    // (?) Do we need lock here, suppose another thread from another core
    // comes in and claim global_sbptr, it will be shard by two cores.
    // DO: move global_sbptr to local heap
    // IF FAIL: move global_sbptr to global heap | (?) how can it fail
    // IF SUCCESS: move local_sbptr to global heap | (DONE)

    // below is SUCCESS case
    // move g to l
    cpu_heaps[my_cpu].bins[sc] = global_sbptr;
    // move l to g
    superblock_h_t *itr = cpu_heaps[sys_core_count].bins[sc];
    superblock_h_t *prev_itr = itr;
    while (itr != NULL && ((char *)itr - (char *)global_sbptr) != 0) {
        prev_itr = itr;
        itr = itr->next;
    }
    // link l to global heap's sc linked list
    if (prev_itr == NULL) {
        // if prev_itr is NULL, it's a new superblock, and global heap is empty
        cpu_heaps[sys_core_count].bins[sc] = local_sbptr;
    } else if (itr == NULL) {
        // if itr is NULL, it's a new superblock, just append;
        prev_itr->next = local_sbptr;
    } else {
        // neither is NULL, associate their nexts
        prev_itr->next = local_sbptr;
        local_sbptr->next = global_sbptr->next;
    }
    // finish moving g to l by NULLing the next of g (now l)
    global_sbptr->next = NULL;

    // retry
    return search_local_block(sc);
}

/*
 * restartable critical section
 * enters fast path if return value is not NULL
 * enters slow path (retry on global heap) if NULL returned
 */
block_h_t *restartable_critical_section(int sc)
{
    restartable = 1;

    // sanity check. TODO: think of moving this outside
    if (sc == 0) {
        restartable = 0;
        return NULL;
    }

    // get current CPU id
    my_cpu = sched_getcpu();
    if (my_cpu < 0) {
        restartable = 0;
        return NULL;
    }

    // find superblock of the requested size class in current core
    heap_h_t hp = cpu_heaps[my_cpu];
    superblock_h_t *sbptr = hp.bins[sc];
    if (sbptr == NULL) {
        restartable = 0;
        return NULL;
    }
    block_h_t *bptr = (block_h_t *)sbptr->local_head;
    if (bptr == NULL) {
        restartable = 0;
        return NULL;
    }

    // pop the local head off
    sbptr->local_head = (void *)bptr->next;
    // update flag and stats
    sbptr->in_use_count++;
    restartable = 0;
    return bptr;
}

/*
 * ask system for memory using mmap;
 * construct a block out of it and return;
 */
block_h_t *create_big_block(size_t size)
{
    void *mmapped;
    block_h_t *bptr;

    if ((mmapped = mmap(NULL, size, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0)) == MAP_FAILED) {
        errno = ENOMEM;
        return NULL;
    }

    bptr = (block_h_t *)mmapped;
    bptr->size_class = MAX_BINS + 1;  // FIXME: do we need plus one?
    bptr->next = NULL;
    return bptr;
}

/*
 * assign hook; compute size class;
 * retrieve block and return;
 */
void *__lib_malloc(size_t size)
{
    block_h_t *ret_addr = NULL;
    if (initialize_malloc() != SUCCESS) {
        errno = ENOMEM;
        return NULL;
    }

    // get size class; retrieve block
    size += sizeof(block_h_t);
    int sc, sc_idx = class_index(size);
    if (sc_idx < 0) {
        // construct and return a big block
        ret_addr = create_big_block(size);
    } else {
        // retreive block from local heap
        sc = class_array_[sc_idx];
        ret_addr = search_local_block(sc);
        ret_addr->next = NULL;  // is this needed?
    }

    // move pointer ahead for header size
    if (ret_addr != NULL) {
        ret_addr = (void *)((char *)ret_addr + sizeof(block_h_t));
    }
    return ret_addr;
}

void *malloc(size_t size) __attribute__((weak, alias("__lib_malloc")));