#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <sys/mman.h>  // mmap, munmap

typedef char ALIGN[16];

union header {
    struct {
        size_t size;
        unsigned is_free;
        union header *next;
    } s;
    ALIGN stub;  // Ensures 16-byte alignment
};
typedef union header header_t;

header_t *head = NULL, *tail = NULL;
pthread_mutex_t global_malloc_lock = PTHREAD_MUTEX_INITIALIZER;

#define MIN_BLOCK_SIZE 32  // Minimum block size for splitting

/* Aligns the size to a multiple of 16 bytes */
size_t align(size_t size, size_t alignment) {
    return (size + alignment - 1) & ~(alignment - 1);
}

/* Find a free block that can fit the requested size */
header_t *get_free_block(size_t size) {
    header_t *curr = head;
    while (curr) {
        if (curr->s.is_free && curr->s.size >= size)
            return curr;
        curr = curr->s.next;
    }
    return NULL;
}

/* Coalesce neighboring free blocks */
void coalesce() {
    header_t *curr = head;
    while (curr && curr->s.next) {
        if (curr->s.is_free && curr->s.next->s.is_free) {
            // Merge the current and next blocks
            curr->s.size += sizeof(header_t) + curr->s.next->s.size;
            curr->s.next = curr->s.next->s.next;
        } else {
            curr = curr->s.next;
        }
    }
}

/* Split a block if it's significantly larger than the requested size */
void split_block(header_t *block, size_t size) {
    if (block->s.size >= size + sizeof(header_t) + MIN_BLOCK_SIZE) {
        header_t *new_block = (header_t *)((char *)block + sizeof(header_t) + size);
        new_block->s.size = block->s.size - size - sizeof(header_t);
        new_block->s.is_free = 1;
        new_block->s.next = block->s.next;
        block->s.size = size;
        block->s.next = new_block;
    }
}

/* Allocate memory using mmap instead of sbrk for thread-safety */
void *malloc(size_t size) {
    size_t aligned_size;
    header_t *header;
    void *block;

    if (!size)
        return NULL;

    aligned_size = align(size, 16);  // Ensure 16-byte alignment

    pthread_mutex_lock(&global_malloc_lock);
    
    header = get_free_block(aligned_size);
    if (header) {
        header->s.is_free = 0;
        split_block(header, aligned_size);  // Split block if large enough
        pthread_mutex_unlock(&global_malloc_lock);
        return (void *)(header + 1);
    }

    // Allocate new block using mmap
    size_t total_size = sizeof(header_t) + aligned_size;
    block = mmap(NULL, total_size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    if (block == MAP_FAILED) {
        pthread_mutex_unlock(&global_malloc_lock);
        return NULL;
    }

    header = block;
    header->s.size = aligned_size;
    header->s.is_free = 0;
    header->s.next = NULL;

    // Add to the list
    if (!head)
        head = header;
    if (tail)
        tail->s.next = header;
    tail = header;

    pthread_mutex_unlock(&global_malloc_lock);
    return (void *)(header + 1);
}

/* Free memory and potentially coalesce neighboring free blocks */
void free(void *block) {
    header_t *header;

    if (!block)
        return;

    pthread_mutex_lock(&global_malloc_lock);

    header = (header_t *)block - 1;
    header->s.is_free = 1;

    coalesce();  // Attempt to merge adjacent free blocks

    // Optionally, munmap the last block if it's free
    if (tail && tail->s.is_free) {
        if (tail == head) {
            munmap(tail, sizeof(header_t) + tail->s.size);
            head = tail = NULL;
        } else {
            header_t *prev = head;
            while (prev && prev->s.next != tail)
                prev = prev->s.next;
            prev->s.next = NULL;
            munmap(tail, sizeof(header_t) + tail->s.size);
            tail = prev;
        }
    }

    pthread_mutex_unlock(&global_malloc_lock);
}

/* Allocate zero-initialized memory for an array */
void *calloc(size_t num, size_t nsize) {
    size_t size;
    void *block;

    if (!num || !nsize)
        return NULL;

    size = num * nsize;

    // Overflow check
    if (nsize != size / num)
        return NULL;

    block = malloc(size);
    if (!block)
        return NULL;

    memset(block, 0, size);
    return block;
}

/* Reallocate memory with a new size */
void *realloc(void *block, size_t size) {
    header_t *header;
    void *new_block;

    if (!block)
        return malloc(size);

    if (!size) {
        free(block);
        return NULL;
    }

    header = (header_t *)block - 1;
    if (header->s.size >= size)
        return block;  // Current block is already big enough

    new_block = malloc(size);
    if (new_block) {
        memcpy(new_block, block, header->s.size);  // Copy old data
        free(block);  // Free old block
    }

    return new_block;
}

/* Debugging function to print the memory list */
void print_mem_list() {
    header_t *curr = head;
    size_t total_free = 0, total_used = 0;
    printf("Memory List:\n");
    while (curr) {
        printf("Address: %p, Size: %zu, Is Free: %u\n", (void *)curr, curr->s.size, curr->s.is_free);
        if (curr->s.is_free)
            total_free += curr->s.size;
        else
            total_used += curr->s.size;
        curr = curr->s.next;
    }
    printf("Total Used Memory: %zu bytes\n", total_used);
    printf("Total Free Memory: %zu bytes\n", total_free);
}
