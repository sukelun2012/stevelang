/*
 * Copyright (c) 2024 Kekun Su(苏科纶).
 * 
 * Refer to the LICENSE file for full license information.
 * SPDX-License-Identifier: MIT
 */

#include "gc.h"
#include <iostream>
#include <stack>
#include <algorithm>
#include <cstdlib>  // for std::free

namespace steve {

// Global garbage collector instance
GarbageCollector gc;

GarbageCollector::GarbageCollector() {
    // Constructor body
}

GarbageCollector::~GarbageCollector() {
    // Clean up all objects
    for (auto obj : objects) {
        if (obj) delete obj;
    }
    objects.clear();
}

void GarbageCollector::deallocate(void* ptr) {
    if (!ptr) return;
    
    GCObject* obj = static_cast<GCObject*>(ptr);
    // Find and mark the object for deletion in the object list
    auto it = std::find(objects.begin(), objects.end(), obj);
    if (it != objects.end()) {
        // Actually delete the object
        delete obj;
        objects.erase(it);
    }
}

int GarbageCollector::collect() {
    int collectedCount = 0;
    
    // Mark phase - mark all reachable objects
    mark();
    
    // Sweep phase - delete all unmarked objects
    sweep();
    
    return collectedCount;
}

void GarbageCollector::markRoot(void* ptr) {
    if (!ptr) return;
    GCObject* obj = static_cast<GCObject*>(ptr);
    rootObjects.insert(obj);
}

void GarbageCollector::addReference(void* from, void* to) {
    if (!from || !to) return;
    GCObject* fromObj = static_cast<GCObject*>(from);
    GCObject* toObj = static_cast<GCObject*>(to);
    references[fromObj].push_back(toObj);
}

void GarbageCollector::mark() {
    // First, clear all marks
    for (auto obj : objects) {
        if (obj) obj->marked = false;
    }
    
    // Start from root objects, use depth-first search to mark all reachable objects
    std::stack<GCObject*> worklist;
    
    // Add root objects to the worklist
    for (auto root : rootObjects) {
        if (root) {
            root->marked = true;
            worklist.push(root);
        }
    }
    
    // Traverse all reachable objects
    while (!worklist.empty()) {
        GCObject* current = worklist.top();
        worklist.pop();
        
        // Traverse other objects referenced by the current object
        auto refIt = references.find(current);
        if (refIt != references.end()) {
            for (auto referenced : refIt->second) {
                if (referenced && !referenced->marked) {
                    referenced->marked = true;
                    worklist.push(referenced);
                }
            }
        }
    }
}

void GarbageCollector::sweep() {
    int collectedCount = 0;
    auto it = objects.begin();
    while (it != objects.end()) {
        GCObject* obj = *it;
        if (obj && !obj->marked) {
            // Object is not marked, needs to be reclaimed
            delete obj;
            it = objects.erase(it);
            collectedCount++;
        } else {
            ++it;
        }
    }
}

void initGC() {
    // Initialize garbage collector
    std::cout << "Garbage Collector initialized\n";
}

void cleanupGC() {
    // Perform final garbage collection
    gc.collect();
    std::cout << "Garbage Collector cleaned up\n";
}

} // namespace steve