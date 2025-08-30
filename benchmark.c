#include <stdio.h>
#include <stdlib.h>

// How many megabytes to allocate
#define MEGABYTES 256
#define ARRAY_SIZE (MEGABYTES * 1024 * 1024 / sizeof(long long))

int main() {
    printf("Benchmark starting. Allocating %d MB of memory...\n", MEGABYTES);

    // This call to malloc() will be intercepted by our library.
    long long* data = (long long*)malloc(ARRAY_SIZE * sizeof(long long));
    
    if (data == NULL) {
        fprintf(stderr, "Memory allocation failed!\n");
        return 1;
    }
    printf("Memory allocated successfully. Pointer: %p\n", (void*)data);

    // Do some simple work on the memory to make sure it's usable.
    printf("Writing to memory...\n");
    for (size_t i = 0; i < ARRAY_SIZE; ++i) {
        data[i] = i;
    }
    printf("Work complete.\n");

    // We will add a pause here so we have time to check the process.
    printf("\n--- Memory is allocated. Check the numa_maps now in another terminal. ---\n");
    printf("--- Press Enter to free memory and exit. ---\n");
    getchar(); // Pauses the program, waiting for you to press Enter.

    free(data); // This call to free() will also be intercepted.
    printf("Memory freed. Exiting.\n");
    
    return 0;
}


