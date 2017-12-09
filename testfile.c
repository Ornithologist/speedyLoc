#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void simpletest()
{
    printf("-----------TEST CASE1---------\n");
    printf("Creating array of 100 integers\n");
    int* array = (int*)malloc(100 * sizeof(int));
    int i;
    for (i = 0; i < 100; i++) {
        array[i] = 9999;
    }
    array[81] = 13;
    array[23] = 2432;
    assert(array[23] == 2432);
    assert(array[81] == 13);
    printf("index 1 has %d\n", array[1]);
    printf("index 81 has %d\n", array[81]);
    printf("index 23 has %d\n", array[23]);
    printf("Successfully allocated an array\n");
    printf("--------END TEST CASE 1-------\n");
}

int main(int argc, char** argv)
{
    // printf("Hello!!\n");
    // int i;
    // for (i = 0; i < 1000000; i++) {
    //     // printf("%d\n", i);
    // }
    void* mem = malloc(1123);

    simpletest();

    return 0;
}
