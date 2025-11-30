/*
 * Copyright (c) 2024 Kekun Su(苏科纶).
 * 
 * Refer to the LICENSE file for full license information.
 * SPDX-License-Identifier: MIT
 */

#include "mem.h"
#include <cstdlib>
#include <cstring>
#include <iostream>

namespace steve {

    // Memory pool implementation
    MemoryPool::MemoryPool(size_t poolSize, size_t blockSize) 
        : poolSize(poolSize), blockSize(blockSize), currentOffset(0) {
        pool = static_cast<char*>(std::malloc(poolSize));
        if (!pool) {
            throw std::bad_alloc();
        }
        // Initialize free list if needed
        freeList = nullptr;
    }

    MemoryPool::~MemoryPool() {
        std::free(pool);
        pool = nullptr;
    }

    void* MemoryPool::allocate() {
        if (currentOffset + blockSize <= poolSize) {
            void* ptr = pool + currentOffset;
            currentOffset += blockSize;
            return ptr;
        }
        return nullptr; // Pool exhausted
    }

    void MemoryPool::deallocate(void* ptr) {
        // For simplicity, we're not actually returning the block to the pool
        // In a full implementation, we would add it to the free list
    }

    void MemoryPool::reset() {
        currentOffset = 0;
    }

    size_t MemoryPool::getUsedSize() const {
        return currentOffset;
    }

    size_t MemoryPool::getFreeSize() const {
        return poolSize - currentOffset;
    }

    // Memory manager implementation (Singleton)
    MemoryManager* MemoryManager::instance = nullptr;
    size_t MemoryManager::poolSizes[NUM_POOLS] = {16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192}; // Different block sizes

    MemoryManager::MemoryManager() {
        // Initialize memory pools
        for (int i = 0; i < NUM_POOLS; i++) {
            pools[i] = new MemoryPool(poolSizes[i] * 100, poolSizes[i]); // Each pool can hold 100 blocks of its size
        }
    }

    MemoryManager::~MemoryManager() {
        // Clean up memory pools
        for (int i = 0; i < NUM_POOLS; i++) {
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
        // Find an appropriate pool or use malloc for large allocations
        for (int i = 0; i < NUM_POOLS; i++) {
            if (size <= poolSizes[i]) {
                void* ptr = pools[i]->allocate();
                if (ptr) {
                    return ptr;
                }
            }
        }
        // If no pool has space or size is too big, use standard malloc
        return std::malloc(size);
    }

    void MemoryManager::deallocate(void* ptr, size_t size) {
        // Try to return to a matching pool
        for (int i = 0; i < NUM_POOLS; i++) {
            if (size <= poolSizes[i] && ptr) {
                // Check if ptr belongs to this pool's memory range
                void* poolStart = pools[i]->getUsedSize() > 0 ? (void*)pools[i] : nullptr; // Simplified check
                // For simplicity, we'll just call free for now
                break;
            }
        }
        // If not returned to pool, just free directly
        std::free(ptr);
    }

    void MemoryManager::cleanup() {
        for (int i = 0; i < NUM_POOLS; i++) {
            pools[i]->reset();
        }
    }

    void MemoryManager::getMemoryStats(size_t& totalAllocated, size_t& totalFree) const {
        totalAllocated = 0;
        totalFree = 0;
        for (int i = 0; i < NUM_POOLS; i++) {
            totalAllocated += pools[i]->getUsedSize();
            totalFree += pools[i]->getFreeSize();
        }
    }

    // Memory operation implementations
    void* malloc(size_t size) {
        return MemoryManager::getInstance()->allocate(size);
    }

    void free(void* ptr) {
        // For simplicity, we'll just use standard free for now
        // In a more sophisticated implementation, we'd determine the size and use deallocate
        std::free(ptr);
    }

    void* realloc(void* ptr, size_t newSize) {
        if (!ptr) return malloc(newSize);
        if (newSize == 0) {
            free(ptr);
            return nullptr;
        }

        void* newPtr = malloc(newSize);
        if (newPtr) {
            // We don't know the original size, so this is a simplification
            // In a real implementation, we'd need to track sizes
            std::free(newPtr);
            return std::realloc(ptr, newSize);
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

    // Memory copy and move implementations
    void* memcpy(void* dest, const void* src, size_t count) {
        return std::memcpy(dest, src, count);
    }

    void* memmove(void* dest, const void* src, size_t count) {
        return std::memmove(dest, src, count);
    }

    int memcmp(const void* lhs, const void* rhs, size_t count) {
        return std::memcmp(lhs, rhs, count);
    }

    // Memory fill implementation
    void* memset(void* dest, int ch, size_t count) {
        return std::memset(dest, ch, count);
    }

    // Get type size implementation
    size_t sizeofType(const std::string& typeName) {
        if (typeName == "int") return sizeof(int);
        if (typeName == "double") return sizeof(double);
        if (typeName == "float") return sizeof(float);
        if (typeName == "bool") return sizeof(bool);
        if (typeName == "char") return sizeof(char);
        if (typeName == "long") return sizeof(long);
        if (typeName == "long long") return sizeof(long long);
        if (typeName == "short") return sizeof(short);
        if (typeName == "void*") return sizeof(void*);
        return 0; // Unknown type
    }

} // namespace steve