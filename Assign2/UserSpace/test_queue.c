// Assignment 2  - Advances in Operating Systems
// Code by Ajitesh Jamulkar 22CS10004
// and Ansh Sahu 22CS10010

// File: ~/lkm_assignment/user_space/test.c

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#define PROC_FILE "/proc/lkm_queue_mod" // Must match PROCFS_NAME in the LKM
#define QUEUE_CAPACITY 10

void test_error(const char *message, int fd)
{
    if (fd < 0)
    {
        perror(message);
        exit(EXIT_FAILURE);
    }
}

int main()
{
    int fd;
    char capacity = QUEUE_CAPACITY;
    int i;
    int num;
    ssize_t ret;
    int read_buf[QUEUE_CAPACITY];

    printf("--- Starting LKM Queue Test ---\n");

    // === Test 1: Open the proc file ===
    printf("\n[TEST] Opening file: %s\n", PROC_FILE);
    fd = open(PROC_FILE, O_RDWR);
    test_error("Failed to open proc file", fd);
    printf("File opened successfully. fd = %d\n", fd);

    // === Test 2: Initialize the queue ===
    printf("\n[TEST] Initializing queue with capacity = %d\n", capacity);
    ret = write(fd, &capacity, 1);
    if (ret != 1)
    {
        perror("Failed to initialize queue");
        close(fd);
        return -1;
    }
    printf("Queue initialized.\n");

    // === Test 3: Enqueue operations ===
    printf("\n[TEST] Enqueuing %d elements...\n", QUEUE_CAPACITY);
    for (i = 0; i < QUEUE_CAPACITY; ++i)
    {
        num = (i + 1) * 10;
        printf("  Writing: %d\n", num);
        ret = write(fd, &num, sizeof(int));
        if (ret != sizeof(int))
        {
            perror("  Write failed");
        }
    }
    printf("Enqueue complete.\n");

    // === Test 4: Enqueue to a full queue (expect error) ===
    printf("\n[TEST] Attempting to enqueue to a full queue (expecting EACCES error)...\n");
    num = 999;
    ret = write(fd, &num, sizeof(int));
    if (ret == -1 && errno == EACCES)
    {
        printf("  SUCCESS: Received expected 'Permission denied' (EACCES) error.\n");
    }
    else
    {
        printf("  FAILURE: Did not receive the expected error.\n");
    }

    // === Test 5: Dequeue all elements ===
    printf("\n[TEST] Reading all elements from queue...\n");
    memset(read_buf, 0, sizeof(read_buf));
    ret = read(fd, read_buf, sizeof(read_buf));
    if (ret < 0)
    {
        perror("Read failed");
    }
    else
    {
        printf("  Read %zd bytes. Elements (FIFO order):\n", ret);
        for (i = 0; i < ret / sizeof(int); ++i)
        {
            printf("    %d\n", read_buf[i]);
        }
    }

    // === Test 6: Dequeue from an empty queue (expect error) ===
    printf("\n[TEST] Attempting to dequeue from an empty queue (expecting EACCES error)...\n");
    ret = read(fd, read_buf, sizeof(read_buf));
    if (ret == -1 && errno == EACCES)
    {
        printf("  SUCCESS: Received expected 'Permission denied' (EACCES) error.\n");
    }
    else
    {
        printf("  FAILURE: Did not receive the expected error.\n");
    }

    // === Test 7: Close and reopen to reset the queue ===
    printf("\n[TEST] Closing and reopening the file to reset the queue...\n");
    close(fd);
    fd = open(PROC_FILE, O_RDWR);
    test_error("Failed to reopen proc file", fd);
    printf("File reopened. fd = %d\n", fd);

    printf("\n[TEST] Re-initializing with capacity = 5\n");
    capacity = 5;
    write(fd, &capacity, 1);
    num = 77;
    printf("  Writing: %d\n", num);
    write(fd, &num, sizeof(int));
    ret = read(fd, read_buf, sizeof(read_buf));
    printf("  Read back %d bytes. First element is: %d\n", (int)ret, read_buf[0]);
    if (read_buf[0] == 77)
    {
        printf("  SUCCESS: Queue reset works as expected.\n");
    }
    else
    {
        printf("  FAILURE: Queue state was not reset.\n");
    }

    // === Final Cleanup ===
    printf("\n--- Test Complete ---\n");
    close(fd);
    return 0;
}