#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>   // Required for memcpy
#include <dlfcn.h>
#include <unistd.h>
#include <numa.h>      // The NUMA library header
#include <pthread.h>  // For making our code thread-safe
#include <sys/mman.h> // Required for mmap and munmap
#include <numaif.h>   // Required for mbind and memory policies

// --- Global variables for our allocator's state ---
static pthread_once_t init_once = PTHREAD_ONCE_INIT;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static int num_nodes = 0; // Will be initialized by init_allocator
static int next_node_to_use = 0;

// This function is guaranteed by pthread_once to be called exactly once
// across all threads, the first time it's needed.
static void init_allocator() {
    if (numa_available() == -1) {
        num_nodes = 1; // Default to 1 if NUMA is not supported.
        return;
    }
    num_nodes = numa_max_node() + 1;

    // Safety check in case numa_max_node() returns an error (<0).
    if (num_nodes <= 0) {
        num_nodes = 1;
    }
}

// Helper function to write one byte to each page of memory.
static void first_touch_pages(void* ptr, size_t size) {
    char* memory = (char*)ptr;
    long page_size = sysconf(_SC_PAGESIZE);

    for (size_t i = 0; i < size; i += page_size) {
        memory[i] = 0;
    }
}

// --- Our Malloc Replacement ---
void* malloc(size_t size) {
    // This ensures init_allocator() is called safely before we proceed.
    pthread_once(&init_once, init_allocator);

    if (size == 0) {
        return NULL;
    }

    pthread_mutex_lock(&lock);
    int target_node = next_node_to_use;
    next_node_to_use = (next_node_to_use + 1) % num_nodes;
    pthread_mutex_unlock(&lock);

    // We need space for the user's data plus our metadata (just the size).
    size_t metadata_size = sizeof(size_t);
    size_t total_size_needed = size + metadata_size;

    void* ptr = mmap(NULL, total_size_needed, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if (ptr == MAP_FAILED) {
        return NULL;
    }

    // Set the NUMA policy by creating the bitmask manually on the stack.
    unsigned long nodemask = 1UL << target_node;

    if (mbind(ptr, total_size_needed, MPOL_BIND, &nodemask, sizeof(nodemask) * 8, 0) != 0) {
        perror("mbind failed");
        munmap(ptr, total_size_needed);
        return NULL;
    }

    first_touch_pages(ptr, total_size_needed);
    
    // Store our metadata at the beginning of the block.
    *(size_t*)ptr = size;

    // Return a pointer to the region the user can safely use.
    void* user_ptr = (char*)ptr + metadata_size;

    return user_ptr;
}

// --- Our Free Replacement ---
void free(void* user_ptr) {
    if (user_ptr == NULL) {
        return;
    }

    size_t metadata_size = sizeof(size_t);
    // Calculate the address of the original mmap'd block.
    void* original_ptr = (char*)user_ptr - metadata_size;
    
    // Read the original requested size from our metadata.
    size_t original_size = *(size_t*)original_ptr;
    size_t total_size = original_size + metadata_size;

    // Use munmap to release the memory back to the kernel.
    munmap(original_ptr, total_size);
}

// --- Our Calloc Replacement ---
void* calloc(size_t nmemb, size_t size) {
    // Check for multiplication overflow.
    if (nmemb > 0 && size > __SIZE_MAX__ / nmemb) {
        return NULL;
    }
    size_t total_size = nmemb * size;
    void* ptr = malloc(total_size);
    // Our malloc uses mmap which returns zero-initialized memory,
    // satisfying the requirement of calloc.
    return ptr;
}

// --- Our Realloc Replacement ---
void* realloc(void* user_ptr, size_t new_size) {
    // Standard behavior: if ptr is NULL, realloc is the same as malloc.
    if (user_ptr == NULL) {
        return malloc(new_size);
    }

    // Standard behavior: if size is 0, realloc is the same as free.
    if (new_size == 0) {
        free(user_ptr);
        return NULL;
    }

    // Get the original size from our metadata.
    size_t metadata_size = sizeof(size_t);
    void* original_ptr = (char*)user_ptr - metadata_size;
    size_t original_size = *(size_t*)original_ptr;

    // Allocate a new block of memory.
    void* new_user_ptr = malloc(new_size);
    if (new_user_ptr == NULL) {
        // If allocation fails, the original block is untouched.
        return NULL;
    }

    // Copy the old data to the new block.
    size_t copy_size = (original_size < new_size) ? original_size : new_size;
    memcpy(new_user_ptr, user_ptr, copy_size);

    // Free the old block.
    free(user_ptr);

    return new_user_ptr;
}


