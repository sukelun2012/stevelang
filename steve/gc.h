/*
 * Copyright (c) 2024 Kekun Su(苏科纶).
 * 
 * Refer to the LICENSE file for full license information.
 * SPDX-License-Identifier: MIT
 */

#ifndef STEVE_GC_H
#define STEVE_GC_H

#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <memory>
#include <string>

namespace steve {

    // Base object type definition
    class GCObject {
    public:
        virtual ~GCObject() = default;
        bool marked = false;  // Mark bit for garbage collection
        size_t size = 0;      // Object size
    };

    // Garbage collector class
    class GarbageCollector {
    public:
        GarbageCollector();
        ~GarbageCollector();

        // Allocate new object
        template<typename T, typename... Args>
        T* allocate(Args&&... args) {
            T* obj = new T(std::forward<Args>(args)...);
            obj->size = sizeof(T);
            objects.push_back(obj);
            return obj;
        }

        // Manually delete object
        void deallocate(void* ptr);

        // Perform garbage collection
        int collect();

        // Mark root object
        void markRoot(void* ptr);

        // Add reference (for tracking object relationships)
        void addReference(void* from, void* to);

    private:
        std::vector<GCObject*> objects;                    // All objects
        std::unordered_set<GCObject*> rootObjects;        // Root object set
        std::unordered_map<GCObject*, std::vector<GCObject*>> references;  // Object reference relationships

        // Mark phase
        void mark();

        // Sweep phase
        void sweep();
    };

    // Global garbage collector instance
    extern GarbageCollector gc;

    // Initialize garbage collector
    void initGC();

    // Clean up garbage collector
    void cleanupGC();

} // namespace steve

#endif // STEVE_GC_H