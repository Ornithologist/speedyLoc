#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define NUM_THREADS 500

void* simpletest(void* idx)
{
    long id = (long)idx;
    printf("------------TEST CASE %ld----------\n", id);
    printf("Creating array of 100 integers\n");
    int* array = (int*)malloc(100 * sizeof(int));
    int i;
    for (i = 0; i < 100; i++) {
        array[i] = 9999;
    }
    array[81] = 13;
    array[23] = 2432;
    assert(array[1] == 9999);
    assert(array[23] == 2432);
    assert(array[81] == 13);
    printf("index 1 has %d\n", array[1]);
    printf("index 81 has %d\n", array[81]);
    printf("index 23 has %d\n", array[23]);
    printf("Successfully allocated an array\n");
    printf("---------END TEST CASE %ld---------\n", id);
}

void multithread_test()
{
    pthread_t threads[NUM_THREADS];
    int rc;
    long t;
    for (t = 0; t < NUM_THREADS; t++) {
        printf("In main: creating thread %ld\n", t);
        rc = pthread_create(&threads[t], NULL, simpletest, (void*)(t + 1));
        if (rc) {
            printf("ERROR; return code from pthread_create() is %d\n", rc);
            exit(-1);
        }
    }

    /* Last thing that main() should do */
    pthread_exit(NULL);
}

int main(int argc, char** argv)
{
    // printf("Hello!!\n");
    // int i;
    // for (i = 0; i < 1000000; i++) {
    //     // printf("%d\n", i);
    // }
    void* mem = malloc(1123);

    simpletest((void*)0);

    multithread_test();

    return 0;
}
