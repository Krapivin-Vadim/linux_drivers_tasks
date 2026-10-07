#include <linux/module.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/wait.h>
#include <linux/mutex.h>
#include <linux/workqueue.h>

MODULE_LICENSE("GPL");

#define BUFFER_SIZE 256
#define WORKER_SCHED 1000
#define DRIVER_NAME "my_driver"
#define DEVICE_NAME "test_device"

static DECLARE_WAIT_QUEUE_HEAD(read_queue);

static struct class *cls;
static dev_t dev;
static int major;

struct process_context {
    pid_t pid;
    char buffer[BUFFER_SIZE];
    struct delayed_work work;
    struct mutex lock;
    bool running;
    bool data_ready;
    unsigned int offset;
    char last_num;
};

void cleanup_buffer(char *buffer) {
    memset(buffer, 0, BUFFER_SIZE);
}

static void sched_worker(struct work_struct *work) {
    struct process_context *ctx = container_of(to_delayed_work(work), struct process_context, work);
    printk(KERN_INFO "Start work for process %d\n", ctx->pid);

    mutex_lock(&ctx->lock);
    if (ctx->running) {
        ctx->last_num += 1;
        unsigned int next_offset = (ctx->offset + 1) % BUFFER_SIZE;
        ctx->buffer[next_offset] = ctx->last_num;
        ctx->data_ready = true;
        schedule_delayed_work(&ctx->work, msecs_to_jiffies(WORKER_SCHED));
    }
    else {
        printk(KERN_INFO "Process %d is not running\n", current->pid);
    }
    mutex_unlock(&ctx->lock);
    printk(KERN_INFO "Finished work for process %d\n", current->pid);
    wake_up_interruptible(&read_queue);
}

static int open(struct inode *inode, struct file *file) {
    printk(KERN_INFO "Call open function for process %d\n", current->pid);
    struct process_context *context;
    context = kmalloc(sizeof(struct process_context), GFP_KERNEL);
    if (!context) {
        printk(KERN_ERR "kmalloc for %d failed\n", current->pid);
        kfree(context);
        return -ENOMEM;
    }
    mutex_init(&context->lock);
    context->running = true;
    context->last_num = 'a';
    INIT_DELAYED_WORK(&context->work, sched_worker);
    schedule_delayed_work(&context->work, msecs_to_jiffies(WORKER_SCHED));
    file->private_data = context;
    printk(KERN_INFO "Buffer for process %d\n was successfully created", current->pid);
    return 0;
}

static int release(struct inode *inode, struct file *file) {
    printk(KERN_INFO "Call release function for process %d\n", current->pid);
    struct process_context *context = file->private_data;
    cancel_delayed_work(&context->work);
    if (context) {
        context->running = false;
        kfree(context);
    }
    printk(KERN_INFO "Finished release for process %d\n", current->pid);
    return 0;
}

static ssize_t read(struct file *file, char __user *buf, size_t count, loff_t *pos) {
    struct process_context *context = file->private_data;
    mutex_lock(&context->lock);
    if (!context->running) {
        mutex_unlock(&context->lock);
        printk(KERN_INFO "Process %d is not running\n", current->pid);
        return -EAGAIN;
    }

    if (!context->data_ready) {
        mutex_unlock(&context->lock);
        int ret = wait_event_interruptible(read_queue, context->data_ready);
        if (ret) {
            return ret;
        }
    }

    if (copy_to_user(buf, context->buffer, BUFFER_SIZE)) {
        printk(KERN_INFO "Buffer copy for process %d failed\n", current->pid);
        return -EFAULT;
    }
    cleanup_buffer(context->buffer);
    context->offset = 0;
    context->data_ready = false;
    mutex_unlock(&context->lock);
    return BUFFER_SIZE;
}

static const struct file_operations fops = {
    .open = open,
    .read = read,
    .release = release,
};

static int __init mod_init(void) {
    major = register_chrdev(0, DRIVER_NAME, &fops);
    if (major < 0) {
        printk(KERN_ERR "Can't get major %d\n", major);
        return major;
    }
    dev = MKDEV(major, 0);
    cls = class_create(DRIVER_NAME);
    device_create(cls, NULL, dev, NULL, DEVICE_NAME);
    printk(KERN_INFO "New device %d %d was registred\n", major, 0);
    printk(KERN_INFO "Init module\n");
    return 0;
}

static void __exit mod_exit(void) {
    device_destroy(cls, dev);
    class_destroy(cls);
    unregister_chrdev(major, DRIVER_NAME);
    printk(KERN_INFO "Remove module\n");
}

module_init(mod_init);
module_exit(mod_exit);



