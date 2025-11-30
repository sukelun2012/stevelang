/*
 * Copyright (c) 2024 Kekun Su(苏科纶).
 * 
 * Refer to the LICENSE file for full license information.
 * SPDX-License-Identifier: MIT
 */

#ifndef STEVE_MEM_H
#define STEVE_MEM_H
#include <cstddef>  // For size_t
#include <new>      // For placement new
#include <string>   // For std::string

namespace steve {

    // Memory pool class, for efficient allocation and deallocation of memory
    class MemoryPool {
    private:
        char* pool;           // Memory pool start address
        size_t poolSize;      // Total memory pool size
        size_t currentOffset; // Current allocation offset
        void** freeList;      // Free block list
        size_t blockSize;     // Size of each block

    public:
        // Constructor, create memory pool with specified size
        MemoryPool(size_t poolSize, size_t blockSize);

        // Destructor
        ~MemoryPool();

        // Allocate memory
        void* allocate();

        // Deallocate memory
        void deallocate(void* ptr);

        // Reset memory pool
        void reset();

        // Get pool usage status
        size_t getUsedSize() const;
        size_t getFreeSize() const;
    };

    // Memory manager class, managing multiple memory pools
    class MemoryManager {
    private:
        static const int NUM_POOLS = 10;  // Number of memory pools of different sizes
        MemoryPool* pools[NUM_POOLS];     // Memory pool array
        size_t poolSizes[NUM_POOLS];      // Block sizes for each pool
        static MemoryManager* instance;   // Singleton instance

        MemoryManager();  // Private constructor
        ~MemoryManager(); // Private destructor

    public:
        // Get singleton instance
        static MemoryManager* getInstance();

        // Allocate memory of specified size
        void* allocate(size_t size);

        // Deallocate memory
        void deallocate(void* ptr, size_t size);

        // Memory pool cleanup
        void cleanup();

        // Get memory statistics
        void getMemoryStats(size_t& totalAllocated, size_t& totalFree) const;
    };

    // Memory operation related functions
    void* malloc(size_t size);                    // Allocate memory
    void free(void* ptr);                         // Deallocate memory
    void* realloc(void* ptr, size_t newSize);     // Reallocate memory
    void* calloc(size_t count, size_t size);      // Allocate and initialize to zero

    // Memory copy and move
    void* memcpy(void* dest, const void* src, size_t count);
    void* memmove(void* dest, const void* src, size_t count);
    int memcmp(const void* lhs, const void* rhs, size_t count);

    // Memory fill
    void* memset(void* dest, int ch, size_t count);

    // Get type size
    size_t sizeofType(const std::string& typeName);

    // Get variable size
    #define SIZEOF_VAR(var) sizeof(var)

} // namespace steve

#endif // STEVE_MEM_H