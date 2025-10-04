Assignment 2: LKM Concurrent Queue
Group Members:

Ajitesh Jamulkar (22CS10004)

Ansh Sahu (22CS10010)

Description
This project implements a Loadable Kernel Module (LKM) that creates a character device at /proc/lkm_22CS10004_22CS10010. This device provides a simple, concurrent, per-process queue for 32-bit integers.

Multiple processes can interact with the module simultaneously, and each will be provided with its own private queue. The module is also thread-safe, meaning a single process can use multiple threads to interact with its queue without causing race conditions, thanks to the use of kernel mutexes.

How to Compile
The project is split into two parts: the kernel module and a user-space test program.

1. Compiling the Kernel Module
   Navigate to the kernel module's directory and run make:

cd ~/lkm_assignment/kernel_module
make

This will produce the kernel object file lkm_queue.ko.

2. Compiling the User-Space Test Program
   Navigate to the user-space program's directory and compile it using gcc:

cd ~/lkm_assignment/user_space
gcc test.c -o test

This will create an executable file named test.

How to Run

1. Load the Kernel Module
   Use the insmod command with sudo to load the module into the kernel:

cd ~/lkm_assignment/kernel_module
sudo insmod lkm_queue.ko

You can verify that the module is loaded and the proc file is created by running:

lsmod | grep lkm_queue
ls -l /proc/lkm_22CS10004_22CS10010

2. Run the Test Program
   Execute the compiled user-space program. You can run multiple instances in parallel from different terminals to test the concurrency and per-process queue functionality.

cd ~/lkm_assignment/user_space
./test

To run two tests at the same time:

Open Terminal 1: ./test

Open Terminal 2: ./test

Both instances will run independently without interfering with each other's queues.

3. Unload the Kernel Module
   Once you are finished testing, unload the module using rmmod:

sudo rmmod lkm_queue

Note: You may need to use sudo rmmod lkm_queue_mod if you used the generic filename from the example. The module name is taken from the object file name.

Test Cases Implemented
The test.c program covers the following scenarios:

Successful Initialization: Opens the proc file and initializes a queue with a valid capacity.

Enqueue Operations: Pushes a full set of integers into the queue.

Full Queue Error: Tries to enqueue an element into a full queue and verifies that it receives the correct error (EACCES).

Dequeue Operation: Reads all elements from the queue and verifies they are in the correct FIFO order.

Empty Queue Error: Tries to dequeue from an empty queue and verifies it receives the correct error (EACCES).

Queue Reset: Closes and re-opens the file to ensure all resources are freed and a new, clean queue is provided.
