NUMA-Aware Memory Allocator

This project demonstrates a custom memory allocator in C that replaces the standard library's malloc, free, calloc, and realloc functions. The custom allocator is NUMA-aware, meaning it strategically places memory on specific NUMA nodes to improve performance on multi-socket systems.

The core mechanism uses LD_PRELOAD to intercept memory calls, mmap to request memory from the kernel, and mbind to set the NUMA policy for that memory.
**Prerequisites**

Before you begin, ensure you have the following installed on your Linux system (e.g., Ubuntu, Debian, Zorin OS):

    A C compiler (gcc) and essential build tools.

    The NUMA development library (libnuma-dev).

You can install these with the following command:

sudo apt-get update
sudo apt-get install build-essential libnuma-dev


**Code Files**

This project consists of two main files:

    numa_alloc.c: The source code for our custom, NUMA-aware memory allocator. This gets compiled into a shared library (.so file).

    benchmark.c: A simple test program that allocates a large block of memory to demonstrate and verify the custom allocator.

**Compilation**

Follow these two steps to compile the project.

1. Compile the NUMA Allocator

Compile numa_alloc.c into a shared library named numa_alloc.so.

gcc -shared -fPIC -o numa_alloc.so numa_alloc.c -ldl -lnuma -lpthread


2. Compile the Benchmark Program

Compile benchmark.c into an executable file named benchmark.

gcc -o benchmark benchmark.c


**How to Run and Verify**

To see the allocator in action, you need to run the benchmark program with the custom library preloaded. The verification process requires two terminals.

Step 1: Run the Benchmark (Terminal 1)

In your first terminal, execute the following command:

LD_PRELOAD=./numa_alloc.so ./benchmark


The program will run and then pause, waiting for you to press Enter. The memory is now allocated.

Step 2: Verify Memory Placement (Terminal 2)

While the first terminal is paused, open a second terminal.

First, find the Process ID (PID) of the running benchmark:

pidof benchmark


This will return a number (e.g., 12345).

Next, use this PID to inspect the kernel's NUMA memory map for the process. (Replace PID with the number you received).

cat /proc/PID/numa_maps


Step 3: Analyze the Output

In the output of the numa_maps command, you are looking for a line corresponding to the large 256MB allocation. It will look like this:

7f... bind:0 anon=65537 dirty=65537 active=0 N0=65537 ...


    bind:0: Proves that your code successfully set the memory policy to be bound to Node 0.

    N0=65537: Proves that the kernel honored the policy and the memory is physically located on Node 0.

Step 4: Finish the Program

Go back to your first terminal and press Enter to allow the benchmark program to finish and free the memory.
**Testing on a True Multi-Node System**

If you have access to a multi-socket server with multiple NUMA nodes, you can use the taskset utility to see the performance impact of memory locality.

    Find the CPU cores for each node with lscpu.

    Good Case (CPU and Memory Local): Pin the process to a core on Node 0.

    # Run on CPU core 0 (assuming it's on Node 0)
    LD_PRELOAD=./numa_alloc.so taskset -c 0 ./benchmark


    Bad Case (CPU and Memory Remote): Pin the process to a core on a different node (e.g., Node 1).

    # Run on CPU core 32 (assuming it's on Node 1)
    LD_PRELOAD=./numa_alloc.so taskset -c 32 ./benchmark


You will observe that the "Bad Case" takes significantly longer to complete, demonstrating the importance of NUMA-aware allocation.
