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
#include <setjmp.h>

__thread jmp_buf critalSection;

__thread int restartable;

void myprint(int val){
    char buf[1024];
    snprintf(buf, 1024, "%d printed\n",val);
    write(STDOUT_FILENO, buf, strlen(buf) + 1);
}


void register_to_driver(){
    // TODO: Add the code to connect with the driver
}


/*
checks if the process thread was in its critical section
if yes then restarts the critical section by making a longjmp
else it resumes the execution.
*/
void upcall_handler(){
    myprint(1111);
    if (restartable){
        longjmp(critalSection, 1);
    }else{
        void (*old_ins_ptr)() = NULL;
        asm volatile("jmp *%0" : : "r" (old_ins_ptr));
    }
}

__attribute__((constructor))
void myconstructor() {
    myprint(0000);
    register_to_driver();
}

void restartable_critical_section(){
    for(int i=0; i<1000; i++);
    myprint(3333);
}

void *malloc(size_t size){
    for(int i=0; i<10; i++){
        myprint(i);
    }
    restartable = 1;
    int r = setjmp(critalSection);
    restartable_critical_section();
    restartable = 0;
    myprint(2222);
    return NULL;
}
