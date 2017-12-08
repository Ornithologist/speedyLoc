#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char** argv)
{
    printf("Hello!!\n");
    int i;
    // for (i = 0; i < 1000000; i++) {
    //     // printf("%d\n", i);
    // }
    void* mem = malloc(1123);
    assert(mem != NULL);
    printf("malloc successful!\n");
    return 0;
}