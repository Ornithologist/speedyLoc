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

#ifndef __COMMON_H__
#define __COMMON_H__

#define SUCCESS 0
#define FAILURE 1
#define IN_USE 1
#define VACANT 0
#define VALID 0
#define INVALID 1

#define MAX_BINS 96  // FIXME: number of size classes
#define FLAT_CLASS_NO 377
#define SYS_CORE_COUNT 1    // default val
#define SYS_PAGE_SIZE 4096  // default val
#define MAX_SML_SIZE 1024
#define MAX_LRG_SIZE 256 * 1024
#define SML_ALIGN 8
#define LRG_ALIGN 128
#define REAL_SML_ALIGN 16
#define SML_SIZE_CLASS(s) ((uint32_t)(s) + 7) >> 3
#define LRG_SIZE_CLASS(s) ((uint32_t)(s) + 127 + (120 << 7)) >> 7

/*
 * struct for a memory block in the buddy system
 * @attri status: IN_USE (returned by malloc) / VACANT (free to use)
 * @attri next: pointer to the cloest next block with the same size
 */
typedef struct _block_header {
    uint8_t status;
    struct _block_header *next;
} block_h_t;

/*
 * struct for a superblock of a (heap, size_class)
 * @attri size_class: size class
 * @attri base_block: addr for the first block_h_t
 */
typedef struct _superblock_header {
    unsigned int size_class;
    void *base_block;
} superblock_h_t;

/*
 * struct for a heap of a CPU
 * @attri cpu: determine CPU for which the heap is allocated
 * @attri bins: list of superblocks allocated, index refers to size_class
 */
typedef struct _heap_header {
    unsigned int cpu;  // zero for global heap
    superblock_h_t *bins[MAX_BINS];
} heap_h_t;

/*
 * struct for malloc info
 * @attri arena: total number of bytes allocated with mmap/sbrk
 * @attri narenas: number of arenas
 * @attri alloreqs: number of allocation requests
 * @attri freereqs: number of free requests
 * @attri alloblks: number of allocated blocks
 * @attri freeblks: number of free blocks
 * @attri uordblks: total allocated space in bytes
 * @attri fordblks: total free space in bytes
 */
typedef struct _mallinfo {
    int arena;
    int narenas;
    int alloreqs;
    int freereqs;
    int alloblks;
    int freeblks;
    int uordblks;
    int fordblks;
} mallinfo_t;

int lg_floor(size_t size);  // only for size < 32 bits
int size_to_no_blocks(size_t size);
int size_to_class(size_t size);
int class_index(size_t size);
int size_to_alignment(size_t size);
void *initialize_lib(size_t size, const void *caller);
void initialize_free(void *ptr, const void *caller);
void *initialize_realloc(void *ptr, size_t size, const void *caller);
void *initialize_calloc(size_t nmemb, size_t size, const void *caller);
int initialize_malloc();
int initialize_heaps();
int initialize_size_classes();

typedef void *(*__malloc_hook_t)(size_t size, const void *caller);
typedef void (*__free_hook_t)(void *ptr, const void *caller);
typedef void *(*__realloc_hook_t)(void *ptr, size_t size, const void *caller);
typedef void *(*__calloc_hook_t)(size_t nmemb, size_t size, const void *caller);

void *__lib_malloc(size_t size);  // le alias
extern void *malloc(size_t size);
extern void free(void *mem_ptr);
extern void *calloc(size_t nmemb, size_t size);
extern void *realloc(void *ptr, size_t size);

extern long sys_page_size;
extern int sys_core_count;
extern int malloc_initialized;
extern __thread int restartable;
extern char class_array_[FLAT_CLASS_NO];
extern int class_to_size_[MAX_BINS];
extern size_t class_to_pages_[MAX_BINS];

#endif