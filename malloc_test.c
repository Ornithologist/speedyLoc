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
#include <sys/ioctl.h>
#include <sys/types.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>

#include "./ioctl_poc/query_ioctl.h"

#define SIG_UPCALL 44
struct sigaction sig;

__thread jmp_buf critalSection;
__thread int restartable;

void upcall_handler();
void query_driver();


void myprint(unsigned long val){
    char buf[8192];
    snprintf(buf, 8192, "%lu printed\n",val);
    write(STDOUT_FILENO, buf, strlen(buf) + 1);
}

/*
Queries the Driver for the old instruction pointer in order to
resume operation from where it was supposed to
*/
void query_driver(){
    myprint(94444);
    char *file_name = "/dev/query";
    int fd;
    fd = open(file_name, O_RDWR);
    if (fd == -1) {
        perror("Could not open device file");
        exit(1);
    }
    int v;
    registered_proc_t q;
    q.pid = getpid();
    if (ioctl(fd, _GET_PROC_META, &q) == -1)
    {
        perror("query ioctl set");
    }
}

/*
checks if the process thread was in its critical section
if yes then restarts the critical section by making a longjmp
else it resumes the execution.
*/
void upcall_handler(){
    myprint(91111);
    if (restartable){
        myprint(92222);
        longjmp(critalSection, 1);
    }else{
        myprint(93333);
        // resume execution
        return;
    }
}

/*
    Registers the process with the driver
*/
void register_to_driver(){
    myprint(33330);
    char *file_name = "/dev/query";
    int fd;
    fd = open(file_name, O_RDWR);
    if (fd == -1) {
        perror("Could not open device file");
        exit(1);
    }
    int v;
    registered_proc_t q;
    q.pid = getpid();
    if (ioctl(fd, _SET_PROC_META, &q) == -1)
    {
        perror("query ioctl set");
    }
    myprint(33331);
}

/*
    plants the upcall signal handler in the global scope
*/
void attach_upcall_signal(){
    myprint(22222);
	sig.sa_sigaction = upcall_handler;
	sig.sa_flags = SA_SIGINFO;
	sigaction(SIG_UPCALL, &sig, NULL);
}

/*
Global constructor gets called one time when the malloc
library gets loaded in the process environment.
*/
__attribute__((constructor))
void myconstructor() {
    myprint(11111);
    attach_upcall_signal();
    register_to_driver();
}

void restartable_critical_section(){
    myprint(66666);
    for(int i=0; i<1000; i++);

}

void *malloc(size_t size){
    myprint(44444);
    restartable = 1;
    int r = setjmp(critalSection);
    restartable_critical_section();
    restartable = 0;
    myprint(77777);
    for(int i=0; i<100000; i++);
    myprint(88888);
    return NULL;
}
