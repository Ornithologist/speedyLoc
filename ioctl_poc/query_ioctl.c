#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <asm/uaccess.h>
#include <linux/kprobes.h>
#include <linux/init.h>
#include <linux/rcupdate.h>
#include <linux/sched.h>
#include <asm/siginfo.h>

#define SIG_UPCALL 44

int send_upcall_signal(unsigned long pid);
int send_upcall_signal(unsigned long pid){
    struct siginfo info;
    struct task_struct *t;
    memset(&info, 0, sizeof(struct siginfo));
	info.si_signo = SIG_UPCALL;
    info.si_code = SI_QUEUE;
    info.si_int = 1234;
    t = pid_task(find_pid_ns(pid, &init_pid_ns), PIDTYPE_PID);
    if(t == NULL){
        printk("no such pid\n");
		return -EINVAL;
	}else{
        int ret = send_sig_info(SIG_UPCALL, &info, t);
        if (ret < 0) {
            printk("error sending signal\n");
            return -EINVAL;
	    }
    }
    return 0;
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("NEU-CS5600 : SRS  <sandarshsrivastav_at_husky_neu_edu>");
MODULE_DESCRIPTION("Driver program for mostly lock free malloc");


#include "query_ioctl.h"

#define MAX_SYMBOL_LEN  64
#define FIRST_MINOR 0
#define MINOR_COUNT 1
#define SUPPORTED_PROCS 10

static char symbol[MAX_SYMBOL_LEN] = "__switch_to";
module_param_string(symbol, symbol, sizeof(symbol), 0644);

static struct kprobe kp = {
    .symbol_name    = symbol,
};

static dev_t dev;
static struct cdev c_dev;
static struct class *cl;


static registered_proc_t procArr[SUPPORTED_PROCS];


/* kprobe pre_handler: called just before the probed instruction is executed */
static int handler_pre(struct kprobe *p, struct pt_regs *regs)
{
    int i;
    int result;
    for(i=0;i<SUPPORTED_PROCS;i++){
        if(procArr[i].pid == current->pid){
            printk("My process %d\n", current->pid);
            result = send_upcall_signal(procArr[i].pid);
            if(result == 0){
                break;
            }
            printk("Do you return from signal");
        }
    }
    return 0;
}


/* kprobe post_handler: called after the probed instruction is executed */
static void handler_post(struct kprobe *p, struct pt_regs *regs,
                unsigned long flags)
{
    //TODO: Add clean up code
}

static int handler_fault(struct kprobe *p, struct pt_regs *regs, int trapnr)
{
    pr_info("fault_handler: p->addr = 0x%p, trap #%dn", p->addr, trapnr);
    /* Return 0 because we don't handle the fault. */
    return 0;
}




static int my_open(struct inode *i, struct file *f)
{
    return 0;
}


static int my_close(struct inode *i, struct file *f)
{
    return 0;
}


#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,35))
static int my_ioctl(struct inode *i, struct file *f, unsigned int cmd, unsigned long arg)
#else
static long my_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
#endif
{
    int i;
    int flag = 0;
    registered_proc_t q;
    registered_proc_t out;
    printk("my_ioctl\n");
    for(i=0; i<SUPPORTED_PROCS; i++){
        printk("PID - %d\n", procArr[i].pid);
    }
    switch (cmd) {
        case _SET_PROC_META:
            printk("Inside _SET_PROC_META\n");
            if (copy_from_user(&q, (registered_proc_t *)arg, sizeof(registered_proc_t))) {
                return -EACCES;
            }


            for(i=0; i<SUPPORTED_PROCS; i++){
                if (procArr[i].pid == -1){
                    procArr[i].pid = q.pid;
                    //procArr[i].upcall_addr = q.upcall_addr;
                    flag = 1;
                    break;
                }
            }
            if (flag){
                return 0;
            }
            return -EINVAL;
        case _GET_PROC_META:
            printk("Inside _GET_PROC_META\n");
            if (copy_from_user(&q, (registered_proc_t *)arg,        sizeof(registered_proc_t))) {
                return -EACCES;
            }
            for(i=0; i<SUPPORTED_PROCS; i++){
                if (procArr[i].pid == q.pid)
                {
                    out = procArr[i];
                    break;
                }
            }
            if (copy_to_user((registered_proc_t *)arg, &out, sizeof(registered_proc_t))) {
                return -EACCES;
            }
            return 0;
        default:
            return -EINVAL;
    }
    return -EINVAL;
}


static struct file_operations query_fops =
{
    .owner = THIS_MODULE,
    .open = my_open,
    .release = my_close,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,35))
    .ioctl = my_ioctl
#else
    .unlocked_ioctl = my_ioctl
#endif
};


static int __init query_ioctl_init(void)
{
    int ret;
    struct device *dev_ret;
    int i;

    for (i=0; i < SUPPORTED_PROCS; i++) {
        procArr[i].pid = -1;
        printk("PID - %d\n", procArr[i].pid);
    }

    if ((ret = alloc_chrdev_region(&dev, FIRST_MINOR, MINOR_COUNT, "query_ioctl")) < 0)
    {
        return ret;
    }

    cdev_init(&c_dev, &query_fops);

    if ((ret = cdev_add(&c_dev, dev, MINOR_COUNT)) < 0)
    {
        return ret;
    }

    if (IS_ERR(cl = class_create(THIS_MODULE, "char")))
    {
        cdev_del(&c_dev);
        unregister_chrdev_region(dev, MINOR_COUNT);
        return PTR_ERR(cl);
    }
    if (IS_ERR(dev_ret = device_create(cl, NULL, dev, NULL, "query")))
    {
        class_destroy(cl);
        cdev_del(&c_dev);
        unregister_chrdev_region(dev, MINOR_COUNT);
        return PTR_ERR(dev_ret);
    }

    kp.pre_handler = handler_pre;
    kp.post_handler = handler_post;
    kp.fault_handler = handler_fault;

    ret = register_kprobe(&kp);
    if (ret < 0) {
        pr_err("register_kprobe failed, returned %d\n", ret);
        return ret;
    }
    pr_info("Planted kprobe at %p\n", kp.addr);

    return 0;
}

static void __exit query_ioctl_exit(void)
{
    unregister_kprobe(&kp);
    device_destroy(cl, dev);
    class_destroy(cl);
    cdev_del(&c_dev);
    unregister_chrdev_region(dev, MINOR_COUNT);
}

module_init(query_ioctl_init);
module_exit(query_ioctl_exit);
