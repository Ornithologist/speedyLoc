#ifndef QUERY_IOCTL_H
#define QUERY_IOCTL_H
#include <linux/ioctl.h>

typedef struct {
    int pid;
} registered_proc_t;

#define _SET_PROC_META _IOW('q', 1, registered_proc_t *)
#define _GET_PROC_META _IOWR('q', 2, registered_proc_t *)
#endif
