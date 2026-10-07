#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/wait.h>

#define BUFFER_SIZE  256

#define DEVICE_NAME "test_device_with_bolck"
static struct class *cls;
static dev_t dev;
static DECLARE_WAIT_QUEUE_HEAD(read_queue);
static bool data_is_ready = false;


static int mod_open(struct inode* inode, struct file *file);
static int mod_release(struct inode* inode, struct file* file);
static ssize_t mod_write(struct file *file, const char __user *buf, size_t count, loff_t *loff);
static ssize_t mod_read(struct file *file, char __user *buf, size_t count, loff_t *loff);
static long mod_ioctl(struct file *file, unsigned int cmd, unsigned long arg);



struct file_operations new_fops = {
    .open = mod_open,
    .release = mod_release,
    .write = mod_write,
    .read = mod_read,
    .unlocked_ioctl = mod_ioctl
};

static int major;

static char BUFFER[BUFFER_SIZE] = {};

static int __init mod_init(void){
    major = register_chrdev(0, DEVICE_NAME, &new_fops);

    if (major < 0){
        printk(KERN_ALERT "Failed to register a device\n");
        return major;
    }
    dev = MKDEV(major, 0);
    cls = class_create(DEVICE_NAME);
    device_create(cls, NULL, dev, NULL, DEVICE_NAME);
    printk(KERN_INFO "New device %d %d was registred\n", major, 0);
    printk(KERN_INFO "Init module\n");
    return 0;
}

static void __exit mod_exit(void){
    device_destroy(cls, dev);
    class_destroy(cls);
    unregister_chrdev(major, DEVICE_NAME);
    printk(KERN_INFO "Remove module\n");
}

static int mod_open(struct inode* inode, struct file *file){
    printk(KERN_INFO "Call open function\n");    
    return 0;
}

static int mod_release(struct inode* inode, struct file* file){
    printk(KERN_INFO "Release function call\n");
    return 0;
}

static ssize_t mod_write(struct file *file, const char __user *buf, size_t count, loff_t *loff){
    if(count > BUFFER_SIZE){
        count = BUFFER_SIZE;
    }

    if(copy_from_user(BUFFER, buf, count)){
        printk(KERN_ERR "Failed to read data from user\n");
        return -EFAULT;
    }
    data_is_ready = true;
    wake_up_interruptible(&read_queue);
    return count;
}

static ssize_t mod_read(struct file *file, char __user *buf, size_t count, loff_t *loff){
    

    if (*loff > 0){
        return 0;
    }
    printk(KERN_INFO "start_read\n");
    if (count > BUFFER_SIZE){
        count = BUFFER_SIZE;
    }

    if(!data_is_ready){

        if (file->f_flags & O_NONBLOCK) {
            printk(KERN_INFO "Unblcked read\n");
            return -EAGAIN;
        }
        else {
            int ret = wait_event_interruptible(read_queue, data_is_ready);
            if(ret){
                return ret;
            }
        }
    }

    if(copy_to_user(buf, BUFFER, count)){
        printk(KERN_ERR "Failed to read data from buffer\n");
        return -EFAULT;
    }
    data_is_ready = false;
    *loff += count;
    return count;
}

static long mod_ioctl(struct file *file, unsigned int cmd, unsigned long arg){
    int i;
    for(i = 0; i != BUFFER_SIZE; i++){
        BUFFER[i] = ' ';
    }
    printk(KERN_INFO "Buffer was cleared\n");
    return 0;
}

module_init(mod_init);
module_exit(mod_exit);

MODULE_LICENSE("GPL"); 