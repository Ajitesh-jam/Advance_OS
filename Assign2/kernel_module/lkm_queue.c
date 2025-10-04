// Assignment 2  - Advances in Operating Systems
// Code by Ajitesh Jamulkar 22CS10004
// and Ansh Sahu 22CS10010
// File: ~/lkm_assignment/kernel_module/lkm_queue.c

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>    // For kmalloc/kfree
#include <linux/uaccess.h> // For copy_to_user/copy_from_user
#include <linux/kdev_t.h>  // For MAJOR and MINOR macros
#include <linux/version.h> // For LINUX_VERSION_CODE and KERNEL_VERSION

MODULE_LICENSE("DUAL BSD/GPL");

#define PROCFS_NAME "lkm_22CS10004_22CS10010"

// This struct holds all data for a single process's queue.
struct process_queue
{
    int *data;        // Pointer to the queue's integer array
    int head;         // Index of the front element
    int tail;         // Index of the next free spot
    int size;         // Current number of elements
    int capacity;     // Maximum capacity (N)
    bool initialized; // Flag to check if capacity has been set
};

// --- Function Prototypes for File Operations ---
static int lkm_open(struct inode *inode, struct file *file);
static int lkm_release(struct inode *inode, struct file *file);
static ssize_t lkm_read(struct file *filp, char __user *user_buf, size_t count, loff_t *f_pos);
static ssize_t lkm_write(struct file *filp, const char __user *user_buf, size_t count, loff_t *f_pos);
static struct proc_ops lkm_proc_ops = {
    .proc_open = lkm_open,
    .proc_release = lkm_release,
    .proc_read = lkm_read,
    .proc_write = lkm_write,
};

/**
 * @brief Called when a process opens the proc file.
 * [cite_start]Allocates a private queue structure for the process. [cite: 45]
 */
static int lkm_open(struct inode *inode, struct file *file)
{
    struct process_queue *queue;

    pr_info("proc file opened.\n");

    // Allocate memory for our private queue struct
    queue = kmalloc(sizeof(struct process_queue), GFP_KERNEL);
    if (!queue)
    {
        pr_warn("Failed to allocate memory for queue struct.\n");
        return -ENOMEM;
    }

    // Initialize the queue state
    queue->data = NULL;
    queue->head = 0;
    queue->tail = 0;
    queue->size = 0;
    queue->capacity = 0;
    queue->initialized = false;

    // Store the pointer in the file's private data field
    file->private_data = queue;

    return 0; // Success
}

/**
 * @brief Called when a process closes the proc file.
 * [cite_start]Frees all resources allocated for that process's queue. [cite: 43]
 */
static int lkm_release(struct inode *inode, struct file *file)
{
    struct process_queue *queue = file->private_data;

    pr_info("proc file closed.\n");

    if (queue)
    {
        // Free the integer array if it was allocated
        kfree(queue->data);
        // Free the queue struct itself
        kfree(queue);
    }

    return 0; // Success
}

/**
 * @brief Called on a read() call. Dequeues all elements.
 * [cite_start]Returns all elements currently in the queue in FIFO order. [cite: 34]
 */
static ssize_t lkm_read(struct file *filp, char __user *user_buf, size_t count, loff_t *f_pos)
{
    struct process_queue *queue = filp->private_data;
    int bytes_to_read;
    int *temp_buf;
    int i;

    // Reading from the same position again gives nothing
    if (*f_pos > 0)
    {
        return 0;
    }

    // Check if the queue is empty
    if (queue->size == 0)
    {
        return -EACCES;
        [cite_start] // Return -EACCES if the queue is empty [cite: 38]
    }

    bytes_to_read = queue->size * sizeof(int);

    // Allocate a temporary kernel buffer to assemble the output
    temp_buf = kmalloc(bytes_to_read, GFP_KERNEL);
    if (!temp_buf)
    {
        return -ENOMEM;
    }

    // Copy elements from the circular buffer to the linear temporary buffer
    for (i = 0; i < queue->size; ++i)
    {
        temp_buf[i] = queue->data[(queue->head + i) % queue->capacity];
    }

    // Copy the assembled data to the user
    if (copy_to_user(user_buf, temp_buf, bytes_to_read))
    {
        kfree(temp_buf);
        return -EFAULT;
    }

    kfree(temp_buf);

    // Reset the queue after reading all elements
    queue->head = 0;
    queue->tail = 0;
    queue->size = 0;

    *f_pos += bytes_to_read;

    return bytes_to_read;
    [cite_start] // Return number of bytes read [cite: 36]
}

/**
 * @brief Called on a write() call. Initializes or enqueues.
 */
static ssize_t lkm_write(struct file *filp, const char __user *user_buf, size_t count, loff_t *f_pos)
{
    struct process_queue *queue = filp->private_data;

    // --- Stage 1: Initialization ---
    if (!queue->initialized)
    {
        char capacity_char;

        if (count != 1)
        {
            pr_warn("Initialization failed: write must be exactly 1 byte.\n");
            return -EINVAL;
        }

        if (copy_from_user(&capacity_char, user_buf, 1))
        {
            return -EFAULT;
        }

        [cite_start] // Check if N is within the valid range [1, 100] [cite: 25]
            if (capacity_char < 1 || capacity_char > 100)
        {
            pr_warn("Initialization failed: capacity %d is out of range (1-100).\n", capacity_char);
            return -EINVAL;
            [cite_start] // Return EINVAL if N is invalid [cite: 26]
        }

        queue->capacity = (int)capacity_char;
        queue->data = kmalloc(queue->capacity * sizeof(int), GFP_KERNEL);
        if (!queue->data)
        {
            return -ENOMEM;
        }

        queue->initialized = true;
        pr_info("Queue initialized with capacity %d\n", queue->capacity);
        return 1; // Return bytes written
    }

    // --- Stage 2: Enqueue Operation ---
    [cite_start] // Argument size must be a 32-bit integer (4 bytes) [cite: 28, 31]
        if (count != sizeof(int))
    {
        return -EINVAL;
    }

    // Check if the queue is full
    if (queue->size == queue->capacity)
    {
        return -EACCES;
        [cite_start] // Return -EACCES if full [cite: 30]
    }

    // Copy integer from user space and add to queue
    if (copy_from_user(&queue->data[queue->tail], user_buf, sizeof(int)))
    {
        return -EFAULT;
    }

    // Update tail and size for the circular buffer
    queue->tail = (queue->tail + 1) % queue->capacity;
    queue->size++;

    return sizeof(int);
    [cite_start] // Return number of bytes written [cite: 29]
}

// --- Module Init and Exit ---
static int __init lkm_init(void)
{
    [cite_start] // Create the proc entry [cite: 17]
        proc_create(PROCFS_NAME, 0666, NULL, &lkm_proc_ops);
    [cite_start] // World-readable and writable [cite: 18]
        pr_info("/proc/%s created\n", PROCFS_NAME);
    return 0;
}

static void __exit lkm_exit(void)
{
    remove_proc_entry(PROCFS_NAME, NULL);
    pr_info("/proc/%s removed\n", PROCFS_NAME);
}

module_init(lkm_init);
module_exit(lkm_exit);
