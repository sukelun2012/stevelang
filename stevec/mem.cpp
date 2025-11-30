/*
 * Copyright (c) 2024 Kekun Su(苏科纶).
 * 
 * Refer to the LICENSE file for full license information.
 * SPDX-License-Identifier: MIT
 */

#include "mem.h"
#include <cstring>  // For memcpy, memmove, memcmp, memset
#include <cstdlib>  // For malloc/free (fallback)

namespace steve {



    // MemoryPool implementation

    MemoryPool::MemoryPool(size_t poolSize, size_t blockSize)

        : poolSize(poolSize), blockSize(blockSize), currentOffset(0) {

        pool = static_cast<char*>(std::malloc(poolSize + sizeof(size_t) * (poolSize / blockSize))); // Extra space for size tracking

        if (!pool) {

            throw std::bad_alloc();

        }

        // Initialize free list

        size_t numBlocks = poolSize / blockSize;

        freeList = static_cast<void**>(std::malloc(numBlocks * sizeof(void*)));

        for (size_t i = 0; i < numBlocks; ++i) {

            freeList[i] = pool + i * blockSize;

        }

    }



    MemoryPool::~MemoryPool() {

        if (pool) std::free(pool);

        if (freeList) std::free(freeList);

    }



    void* MemoryPool::allocate() {

        if (currentOffset < (poolSize / blockSize) && freeList[currentOffset]) {

            void* ptr = freeList[currentOffset];

            freeList[currentOffset] = nullptr;

            currentOffset++;

            return ptr;

        }
        else if (currentOffset * blockSize < poolSize) {

            // Allocate from remaining pool space

            void* ptr = pool + currentOffset * blockSize;

            currentOffset++;

            return ptr;

        }

        return nullptr;  // Pool exhausted

    }



    void MemoryPool::deallocate(void* ptr) {

        if (ptr >= pool && ptr < pool + poolSize) {

            size_t blockIndex = ((static_cast<char*>(ptr) - pool) / blockSize);

            if (blockIndex < poolSize / blockSize && currentOffset > 0) {

                currentOffset--;

                freeList[currentOffset] = ptr;

            }

        }

    }

    void MemoryPool::reset() {
        currentOffset = 0;
        size_t numBlocks = poolSize / blockSize;
        for (size_t i = 0; i < numBlocks; ++i) {
            freeList[i] = pool + i * blockSize;
        }
    }

    size_t MemoryPool::getUsedSize() const {
        return currentOffset * blockSize;
    }

    size_t MemoryPool::getFreeSize() const {
        return poolSize - getUsedSize();
    }

    // MemoryManager singleton implementation
    MemoryManager* MemoryManager::instance = nullptr;

    MemoryManager::MemoryManager() {
        // Initialize different pool sizes (powers of 2)
        size_t baseSize = 16;
        for (int i = 0; i < NUM_POOLS; ++i) {
            poolSizes[i] = baseSize;
            pools[i] = new MemoryPool(1024 * baseSize, baseSize);  // 1024 blocks of each size
            baseSize *= 2;
        }
    }

    MemoryManager::~MemoryManager() {
        for (int i = 0; i < NUM_POOLS; ++i) {
            delete pools[i];
        }
    }

    MemoryManager* MemoryManager::getInstance() {
        if (!instance) {
            instance = new MemoryManager();
        }
        return instance;
    }

    void* MemoryManager::allocate(size_t size) {
        // Find appropriate pool size
        for (int i = 0; i < NUM_POOLS; ++i) {
            if (size <= poolSizes[i]) {
                void* ptr = pools[i]->allocate();
                if (ptr) return ptr;
            }
        }

        // Fallback to system malloc if no pool can accommodate
        return std::malloc(size);
    }

    void MemoryManager::deallocate(void* ptr, size_t size) {
        // Find appropriate pool size
        for (int i = 0; i < NUM_POOLS; ++i) {
            if (size <= poolSizes[i]) {
                pools[i]->deallocate(ptr);
                return;
            }
        }

        // Fallback to system free for large objects
        std::free(ptr);
    }

    void MemoryManager::cleanup() {
        for (int i = 0; i < NUM_POOLS; ++i) {
            pools[i]->reset();
        }
    }

    void MemoryManager::getMemoryStats(size_t& totalAllocated, size_t& totalFree) const {
        totalAllocated = 0;
        totalFree = 0;
        for (int i = 0; i < NUM_POOLS; ++i) {
            totalAllocated += pools[i]->getUsedSize();
            totalFree += pools[i]->getFreeSize();
        }
    }

    // Memory operation implementations



    // Structure to track allocation metadata

    struct AllocationHeader {

        size_t size;

        bool isFree;

    };



    void* malloc(size_t size) {

        // Add space for header

        size_t totalSize = size + sizeof(AllocationHeader);

        void* rawPtr = MemoryManager::getInstance()->allocate(totalSize);

        if (!rawPtr) return nullptr;



        // Set up header

        AllocationHeader* header = static_cast<AllocationHeader*>(rawPtr);

        header->size = size;

        header->isFree = false;



        // Return pointer past the header

        return static_cast<char*>(rawPtr) + sizeof(AllocationHeader);

    }



    void free(void* ptr) {

        if (!ptr) return;



        // Get back to header

        AllocationHeader* header = reinterpret_cast<AllocationHeader*>(static_cast<char*>(ptr) - sizeof(AllocationHeader));

        if (header->isFree) return; // Already freed



        header->isFree = true;

        // In a more complete implementation, we'd return the block to the memory pool

        // For now, we'll just mark it as free and let the pool manage it

        // Actual deallocation would depend on the memory pool strategy

    }



    void* realloc(void* ptr, size_t newSize) {

        if (!ptr) return malloc(newSize);

        if (newSize == 0) {

            free(ptr);

            return nullptr;

        }



        // Get header to find old size

        AllocationHeader* oldHeader = reinterpret_cast<AllocationHeader*>(static_cast<char*>(ptr) - sizeof(AllocationHeader));

        size_t oldSize = oldHeader->size;



        // If the new size fits in the current block and it's not marked as free

        if (newSize <= oldSize && !oldHeader->isFree) {

            // Just update the size if smaller

            if (newSize < oldSize) {

                oldHeader->size = newSize;

            }

            return ptr;

        }



        // Otherwise allocate new memory, copy, and free old

        void* newPtr = malloc(newSize);

        if (newPtr) {

            // Copy the minimum of old and new sizes

            size_t copySize = (oldSize < newSize) ? oldSize : newSize;

            std::memcpy(newPtr, ptr, copySize);

            free(ptr);

            return newPtr;

        }

        return nullptr;

    }

    void* calloc(size_t count, size_t size) {
        size_t totalSize = count * size;
        void* ptr = malloc(totalSize);
        if (ptr) {
            std::memset(ptr, 0, totalSize);
        }
        return ptr;
    }

    // Standard memory operations
    void* memcpy(void* dest, const void* src, size_t count) {
        return std::memcpy(dest, src, count);
    }

    void* memmove(void* dest, const void* src, size_t count) {
        return std::memmove(dest, src, count);
    }

    int memcmp(const void* lhs, const void* rhs, size_t count) {
        return std::memcmp(lhs, rhs, count);
    }

    void* memset(void* dest, int ch, size_t count) {

        return std::memset(dest, ch, count);

    }



    size_t sizeofType(const std::string& typeName) {

        if (typeName == "int" || typeName == "long") {

            return sizeof(int);

        }
        else if (typeName == "short") {

            return sizeof(short);

        }
        else if (typeName == "byte" || typeName == "char") {

            return sizeof(char);

        }
        else if (typeName == "float") {

            return sizeof(float);

        }
        else if (typeName == "double") {

            return sizeof(double);

        }
        else if (typeName == "bool") {

            return sizeof(bool);

        }
        else if (typeName == "string") {

            return sizeof(std::string);  // Size of string object, not content

        }
        else {

            // Default size for unknown types

            return sizeof(void*);

        }

    }



} // namespace steve