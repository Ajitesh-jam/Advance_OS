// Assignment 2  - Advances in Operating Systems
// Code by Ajitesh Jamulkar 22CS10004
// and Ansh Sahu 22CS10010
//
// CONCURRENCY UPDATE: Added mutex locks to ensure thread-safety for read/write operations.
// File: ~/lkm_assignment/kernel_module/lkm_queue.c

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>    // For kmalloc/kfree
#include <linux/uaccess.h> // For copy_to_user/copy_from_user
#include <linux/mutex.h>   // For mutex locks
#include <linux/version.h> // For LINUX_VERSION_CODE and KERNEL_VERSION

MODULE_LICENSE("DUAL BSD/GPL");
MODULE_AUTHOR("Ajitesh Jamulkar & Ansh Sahu");
MODULE_DESCRIPTION("A thread-safe kernel module for a concurrent integer queue.");

struct process_queue
{
    int *data;
    int head;
    int tail;
    int size;
    int capacity;
    bool initialized;
    struct mutex lock; // Mutex to protect this specific queue from concurrent access
};

// --- Function Prototypes for File Operations ---
static int lkm_open(struct inode *inode, struct file *file);
static int lkm_release(struct inode *inode, struct file *file);
static ssize_t lkm_read(struct file *filp, char __user *user_buf, size_t count, loff_t *f_pos);
static ssize_t lkm_write(struct file *filp, const char __user *user_buf, size_t count, loff_t *f_pos);

// Proc file operations struct for modern kernels
static struct proc_ops lkm_proc_ops = {
    .proc_open = lkm_open,
    .proc_release = lkm_release,
    .proc_read = lkm_read,
    .proc_write = lkm_write,
};

static int lkm_open(struct inode *inode, struct file *file)
{
    struct process_queue *queue;

    printk(KERN_INFO "lkm_queue: Process ID %d opened the proc file.\n", current->pid);

    queue = kmalloc(sizeof(struct process_queue), GFP_KERNEL);
    if (!queue)
    {
        printk(KERN_ERR "lkm_queue: Failed to allocate memory for queue struct.\n");
        return -ENOMEM;
    }

    // Initialize queue state
    queue->data = NULL;
    queue->head = 0;
    queue->tail = 0;
    queue->size = 0;
    queue->capacity = 0;
    queue->initialized = false;

    // Initialize the mutex for this queue instance
    mutex_init(&queue->lock);

    file->private_data = queue;
    return 0;
}

static int lkm_release(struct inode *inode, struct file *file)
{
    struct process_queue *queue = file->private_data;
    printk(KERN_INFO "lkm_queue: Process ID %d closed the file.\n", current->pid);

    if (queue)
    {
        // The mutex does not need to be explicitly destroyed when the object is freed.
        kfree(queue->data);
        kfree(queue);
    }
    return 0;
}

static ssize_t lkm_read(struct file *filp, char __user *user_buf, size_t count, loff_t *f_pos)
{
    struct process_queue *queue = filp->private_data;
    int bytes_to_read;
    int *temp_buf;
    int i;
    int retval;

    // Acquire lock to ensure atomic read of the whole queue
    if (mutex_lock_interruptible(&queue->lock))
    {
        return -ERESTARTSYS;
    }

    if (*f_pos > 0)
    {
        retval = 0;
        goto out;
    }

    if (queue->size == 0)
    {
        printk(KERN_INFO "lkm_queue: Queue is empty, nothing to read.\n");
        retval = -EACCES;
        goto out;
    }

    bytes_to_read = queue->size * sizeof(int);
    temp_buf = kmalloc(bytes_to_read, GFP_KERNEL);
    if (!temp_buf)
    {
        retval = -ENOMEM;
        goto out;
    }

    // Assemble data in FIFO order
    for (i = 0; i < queue->size; ++i)
    {
        temp_buf[i] = queue->data[(queue->head + i) % queue->capacity];
    }

    if (copy_to_user(user_buf, temp_buf, bytes_to_read))
    {
        kfree(temp_buf);
        retval = -EFAULT;
        goto out;
    }

    kfree(temp_buf);

    // Reset the queue after reading all elements
    queue->head = 0;
    queue->tail = 0;
    queue->size = 0;

    *f_pos += bytes_to_read;
    retval = bytes_to_read;

out:
    mutex_unlock(&queue->lock); // Release the lock
    return retval;
}

static ssize_t lkm_write(struct file *filp, const char __user *user_buf, size_t count, loff_t *f_pos)
{
    struct process_queue *queue = filp->private_data;
    int retval;

    // Acquire lock to ensure atomic write operation
    if (mutex_lock_interruptible(&queue->lock))
    {
        return -ERESTARTSYS;
    }

    // Stage 1: Initialization
    if (!queue->initialized)
    {
        char capacity_char;

        if (count != 1)
        {
            pr_warn("lkm_queue: Initialization failed: write must be exactly 1 byte.\n");
            retval = -EINVAL;
            goto out;
        }

        if (copy_from_user(&capacity_char, user_buf, 1))
        {
            retval = -EFAULT;
            goto out;
        }

        if (capacity_char < 1 || capacity_char > 100)
        {
            printk(KERN_INFO "lkm_queue: Invalid capacity %d from PID %d\n", capacity_char, current->pid);
            retval = -EINVAL;
            goto out;
        }

        queue->capacity = (int)capacity_char;
        queue->data = kmalloc(queue->capacity * sizeof(int), GFP_KERNEL);
        if (!queue->data)
        {
            retval = -ENOMEM;
            goto out;
        }

        queue->initialized = true;
        printk(KERN_INFO "lkm_queue: PID %d initialized queue with capacity %d\n", current->pid, queue->capacity);
        retval = 1; // successful write of 1 byte
        goto out;
    }

    // Stage 2: Enqueue Operation
    if (count != sizeof(int))
    {
        retval = -EINVAL;
        goto out;
    }

    if (queue->size == queue->capacity)
    {
        retval = -EACCES; // Queue is full
        goto out;
    }

    if (copy_from_user(&queue->data[queue->tail], user_buf, sizeof(int)))
    {
        retval = -EFAULT;
        goto out;
    }

    queue->tail = (queue->tail + 1) % queue->capacity;
    queue->size++;
    retval = sizeof(int);

out:
    mutex_unlock(&queue->lock); // Release the lock
    return retval;
}

static int __init lkm_init(void)
{
    // Create the proc entry with permissions 0666 (world-readable and writable)
    if (!proc_create("lkm_22CS10004_22CS10010", 0666, NULL, &lkm_proc_ops))
    {
        printk(KERN_ALERT "lkm_queue: Error: Could not initialize /proc/lkm_22CS10004_22CS10010\n");
        return -ENOMEM;
    }
    printk(KERN_INFO "lkm_queue: /proc/lkm_22CS10004_22CS10010 created\n");
    return 0;
}

static void __exit lkm_exit(void)
{
    remove_proc_entry("lkm_22CS10004_22CS10010", NULL);
    printk(KERN_INFO "lkm_queue: /proc/lkm_22CS10004_22CS10010 removed\n");
}

module_init(lkm_init);
module_exit(lkm_exit);
