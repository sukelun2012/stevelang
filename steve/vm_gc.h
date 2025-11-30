/*
 * Copyright (c) 2024 Kekun Su(苏科纶).
 * 
 * Refer to the LICENSE file for full license information.
 * SPDX-License-Identifier: MIT
 */

#ifndef STEVE_VM_GC_H
#define STEVE_VM_GC_H

#include <memory>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <cstddef> // for size_t

namespace steve {
    namespace VM {

        // VM-specific garbage collector manager

        class VMGarbageCollector {

        private:

            // Mark-sweep garbage collector
            std::unordered_set<void*> heap;           // all objects in heap
            std::unordered_set<void*> roots;          // root object collection
            std::unordered_map<void*, std::vector<void*>> references;  // object reference relationships

        public:

            VMGarbageCollector();
            ~VMGarbageCollector();

            // Allocate new object
            void* allocate(size_t size);

            // Mark root object
            void markRoot(void* obj);

            // Add reference relationship between objects
            void addReference(void* from, void* to);

            // Run garbage collection
            size_t collect();

            // Deallocate object
            void deallocate(void* obj);

            // Set object reachability
            void setReachable(void* obj);

            // Get heap statistics
            size_t getHeapSize() const;
            size_t getLiveObjects() const;

        private:

            // Mark phase - mark all reachable objects
            void mark();

            // Sweep phase - delete all unreachable objects
            void sweep();

        };

    } // namespace VM
} // namespace steve

#endif // STEVE_VM_GC_H