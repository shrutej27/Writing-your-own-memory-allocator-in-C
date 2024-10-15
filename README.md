
# Custom Memory Allocator

## Overview

This custom memory allocator implements the `malloc()`, `calloc()`, `realloc()`, and `free()` functions, mimicking the behavior of standard C library functions for dynamic memory allocation. The allocator uses `mmap()` for thread-safe memory management and implements block splitting, coalescing, and alignment to reduce memory fragmentation and ensure efficient usage of memory.

The allocator supports:
- Thread-safe memory management.
- Block splitting to reduce wasted memory.
- Coalescing adjacent free blocks to reduce fragmentation.
- Alignment of allocated memory for performance improvements.

## Data Structures

### `header_t`

The allocator uses a linked list of memory blocks, where each block has a header that stores metadata about the block (size, free status, and pointer to the next block).

```c
union header {
    struct {
        size_t size;           // Size of the block (excluding the header).
        unsigned is_free;       // 1 if the block is free, 0 otherwise.
        union header *next;     // Pointer to the next block in the linked list.
    } s;
    ALIGN stub;                // 16-byte alignment to improve memory access performance.
};
typedef union header header_t;
```

### Global Variables
- `header_t *head` – Pointer to the first block in the linked list.
- `header_t *tail` – Pointer to the last block in the linked list.
- `pthread_mutex_t global_malloc_lock` – Mutex lock to ensure thread safety during memory operations.

## Functions

### `malloc()`
Allocates a block of memory of the requested size.

#### Parameters
- `size_t size`: The size of the memory block to allocate, in bytes.

#### Returns
- Pointer to the allocated memory block on success, or `NULL` on failure.

#### Description
- Aligns the requested size to 16 bytes for optimal performance.
- Searches for a free block in the linked list of memory blocks.
- If a suitable free block is found, it is split if it's large enough.
- If no suitable block is found, a new block is allocated using `mmap()`.
- Ensures thread safety using `pthread_mutex_lock()`.

#### Example Usage
```c
void *ptr = malloc(128);
```

### `free()`
Frees a previously allocated memory block and potentially merges neighboring free blocks (coalescing).

#### Parameters
- `void *block`: Pointer to the memory block to be freed.

#### Returns
- None.

#### Description
- Marks the block as free.
- Coalesces neighboring free blocks to reduce fragmentation.
- If the last block is free, it is released back to the operating system using `munmap()`.

#### Example Usage
```c
free(ptr);
```

### `calloc()`
Allocates memory for an array of `num` elements, each of `nsize` bytes, and initializes the memory to zero.

#### Parameters
- `size_t num`: The number of elements to allocate.
- `size_t nsize`: The size of each element, in bytes.

#### Returns
- Pointer to the allocated and zero-initialized memory block, or `NULL` on failure.

#### Description
- Multiplies `num` and `nsize` to calculate the total size of memory required, with overflow protection.
- Allocates memory using `malloc()`.
- Initializes the memory to zero using `memset()`.

#### Example Usage
```c
int *arr = (int *)calloc(10, sizeof(int));
```

### `realloc()`
Resizes a previously allocated memory block to a new size.

#### Parameters
- `void *block`: Pointer to the previously allocated memory block.
- `size_t size`: The new size of the memory block, in bytes.

#### Returns
- Pointer to the newly allocated block with the data copied over, or `NULL` if the reallocation failed.

#### Description
- If the block is `NULL`, it behaves like `malloc()`.
- If the size is zero, it behaves like `free()`.
- If the new size is smaller than or equal to the current size, the block is returned as is.
- If the new size is larger, a new block is allocated, and the data is copied over from the old block.
- The old block is then freed.

#### Example Usage
```c
void *new_ptr = realloc(ptr, 256);
```

### `get_free_block()`
Searches for a free block in the memory block linked list that can accommodate the requested size.

#### Parameters
- `size_t size`: The size of the memory block to find.

#### Returns
- Pointer to the free block if found, or `NULL` if no suitable block is found.

#### Description
- Traverses the linked list of memory blocks and returns a pointer to a free block if it is large enough to satisfy the allocation request.

#### Example Usage
```c
header_t *free_block = get_free_block(128);
```

### `coalesce()`
Merges adjacent free blocks to reduce fragmentation.

#### Parameters
- None.

#### Returns
- None.

#### Description
- Traverses the linked list of blocks and merges adjacent free blocks into a single larger block.

#### Example Usage
```c
coalesce();
```

### `split_block()`
Splits a large block into two smaller blocks if the block is significantly larger than the requested size.

#### Parameters
- `header_t *block`: The block to be split.
- `size_t size`: The requested size of the block.

#### Returns
- None.

#### Description
- If the block's size is significantly larger than the requested size, the block is split into two blocks: one that is the requested size and another smaller free block.

#### Example Usage
```c
split_block(free_block, 128);
```

### `print_mem_list()`
Debugging function to print the state of the memory blocks in the linked list.

#### Parameters
- None.

#### Returns
- None.

#### Description
- Prints the addresses, sizes, and free status of all memory blocks in the linked list.
- Also prints the total used and free memory.

#### Example Usage
```c
print_mem_list();
```

## Improvements and Optimizations

1. **Thread Safety with `mmap()`**:
   - The allocator uses `mmap()` to request memory from the operating system. This is thread-safe, as opposed to the older `sbrk()`, which is not thread-safe.

2. **Block Splitting**:
   - If a free block is larger than necessary, it is split into two smaller blocks. This prevents memory waste and improves overall memory utilization.

3. **Block Coalescing**:
   - When a block is freed, the allocator attempts to merge it with neighboring free blocks to reduce fragmentation.

4. **16-Byte Alignment**:
   - The allocator ensures that memory blocks are aligned to 16 bytes for performance optimization and compatibility with modern hardware architectures.

5. **Overflow Checks in `calloc()`**:
   - The `calloc()` function includes overflow checks to prevent dangerous situations where multiplication of `num` and `nsize` could overflow and result in too small a memory allocation.

6. **Enhanced Debugging**:
   - The `print_mem_list()` function allows for easy debugging by printing the current state of the memory allocator, showing the address, size, and free status of each block.

## Example Program

Here is an example program that demonstrates the usage of the custom memory allocator functions:

```c
#include <stdio.h>

int main() {
    void *ptr1 = malloc(100);
    void *ptr2 = malloc(200);
    print_mem_list();  // Print current state of the memory list

    free(ptr1);
    print_mem_list();  // Check if memory was freed and coalesced

    void *ptr3 = calloc(10, sizeof(int));  // Allocates and zeroes memory
    print_mem_list();

    ptr2 = realloc(ptr2, 300);  // Resize the allocated block
    print_mem_list();

    free(ptr2);
    free(ptr3);
    print_mem_list();  // Final state of the memory list

    return 0;
}
```

## Conclusion

This custom memory allocator implements a flexible and thread-safe memory management system using `mmap()`. It supports dynamic memory allocation functions similar to the C standard library (`malloc()`, `calloc()`, `realloc()`, and `free()`). With enhancements like block splitting, coalescing, and alignment, the allocator efficiently manages memory and reduces fragmentation in both single-threaded and multi-threaded environments.
