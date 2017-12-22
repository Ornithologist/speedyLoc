# speedyLoc: C library for dynamic memory allocation.

It uses the ideas from LFMalloc (Lock free malloc implementation) and TCMalloc to create a more time and memory efficient approach to
memory allocation in C Language.

**Refer to the following literature for understanding implementation of LFMalloc and TCMalloc.**
1. Dice, David & Garthwaite, Alex. (2002). Mostly lock-free malloc​. SIGPLAN Notices. 38. 269-280.
2. http://goog-perftools.sourceforge.net/doc/tcmalloc.html


## API's implemented
```
void *malloc(size_t size);
void free(void *ptr);
```

## Novelty:
1. Fine grained size classes for small sized memory requests.
2. Balance between number of size classes and fragmentation.
3. Mostly Lock free approach to memory allocation to increase parallelism.

## Results/Benchmarks 
Performance comparison with libc malloc library for 10, 50, 500, 1000 threads. Please refer to docs for detailed benchmarks.


## Existing Issues (Future work)
1. High frequency of upcalls.
2. Kernel driver does not support re-routing upcalls based on specific threads.
3. Process can go into a cycle of upcall->kernel->upcall as the number of threads increases.




