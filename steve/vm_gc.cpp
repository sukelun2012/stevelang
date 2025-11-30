/*
 * Copyright (c) 2024 Kekun Su(苏科纶).
 * 
 * Refer to the LICENSE file for full license information.
 * SPDX-License-Identifier: MIT
 */

#include "vm_gc.h"
#include "vm.h"
#include <iostream>
#include <set>
#include <cstdlib> // for std::free

namespace steve {
    namespace VM {

        VMGarbageCollector::VMGarbageCollector() {
            // Constructor
        }

        VMGarbageCollector::~VMGarbageCollector() {

            // Clean up all heap objects

            for (void* obj : heap) {
                std::free(obj);

            }

            heap.clear();
            roots.clear();
            references.clear();

        }

        void* VMGarbageCollector::allocate(size_t size) {
            void* obj = std::malloc(size);
            if (obj) {
                heap.insert(obj);
            }
            return obj;
        }

        void VMGarbageCollector::markRoot(void* obj) {
            if (obj && heap.count(obj)) {
                roots.insert(obj);
            }
        }

        void VMGarbageCollector::addReference(void* from, void* to) {
            if (from && to && heap.count(from) && heap.count(to)) {
                references[from].push_back(to);
            }
        }

        void VMGarbageCollector::setReachable(void* obj) {

            // This function is used to mark objects as reachable during the mark phase

            // Actually managed through roots and references structures

        }

        void VMGarbageCollector::mark() {

            // Use depth-first search to mark all reachable objects

            std::unordered_set<void*> marked;

            std::vector<void*> worklist;



            // Add all root objects to the worklist

            for (void* root : roots) {

                if (heap.count(root)) {

                    marked.insert(root);

                    worklist.push_back(root);

                }

            }



            // Traverse all reachable objects

            while (!worklist.empty()) {

                void* current = worklist.back();

                worklist.pop_back();



                // Check other objects referenced by the current object

                auto it = references.find(current);

                if (it != references.end()) {

                    for (void* ref : it->second) {

                        if (heap.count(ref) && marked.insert(ref).second) {

                            worklist.push_back(ref);

                        }

                    }

                }

            }



            // For the mark phase implementation, we need a temporary set to store marked objects

            // But the actual marking is maintained through roots and references

            // Here we just verify reachability

        }

        size_t VMGarbageCollector::collect() {
            if (heap.empty()) {
                return 0;

            }

            // Mark phase: find all reachable objects
            std::unordered_set<void*> reachable;
            std::vector<void*> worklist;

            // Start from root objects
            for (void* root : roots) {
                if (heap.count(root) && reachable.insert(root).second) {
                    worklist.push_back(root);
                }

            }

            // Depth-first search all reachable objects
            while (!worklist.empty()) {
                void* current = worklist.back();
                worklist.pop_back();

                // Check all objects referenced by the current object
                auto it = references.find(current);
                if (it != references.end()) {
                    for (void* ref : it->second) {
                        if (heap.count(ref) && reachable.insert(ref).second) {
                            worklist.push_back(ref);

                        }

                    }

                }

            }

            // Sweep phase: release all unreachable objects

            size_t collected = 0;

            auto it = heap.begin();

            while (it != heap.end()) {

                if (reachable.count(*it) == 0) {

                    // Object unreachable, release it

                    std::free(*it);

                    // Also remove from reference graph

                    references.erase(*it);



                    // Also remove from other objects' reference lists

                    for (auto& refEntry : references) {

                        auto& refList = refEntry.second;

                        refList.erase(std::remove(refList.begin(), refList.end(), *it), refList.end());

                    }



                    it = heap.erase(it);

                    collected++;

                }

                else {

                    ++it;

                }

            }

            // Remove reference entries for objects that no longer exist
            auto refIt = references.begin();
            while (refIt != references.end()) {
                if (heap.count(refIt->first) == 0) {
                    refIt = references.erase(refIt);
                }

                else {
                    // Remove objects that no longer exist from reference lists
                    auto& refList = refIt->second;
                   refList.erase(std::remove_if(refList.begin(), refList.end(),
                        [this](void* obj) { return heap.count(obj) == 0; }),
                        refList.end());
                    ++refIt;
                }

            }
            return collected;

        }

        void VMGarbageCollector::deallocate(void* obj) {
            if (obj && heap.count(obj)) {
                // Remove from heap
                heap.erase(obj);

                // Remove from root set
                roots.erase(obj);

                // Remove from reference graph
                references.erase(obj);

                // Remove this object from other objects' reference lists
                for (auto& refEntry : references) {
                    auto& refList = refEntry.second;
                    refList.erase(std::remove(refList.begin(), refList.end(), obj), refList.end());

                }

                // Release memory
                std::free(obj);

            }

        }

        size_t VMGarbageCollector::getHeapSize() const {
            return heap.size();
        }

        size_t VMGarbageCollector::getLiveObjects() const {

            // Recalculate the number of reachable objects
            std::unordered_set<void*> reachable;
            std::vector<void*> worklist;

            // Start from root objects
            for (void* root : roots) {
                if (heap.count(root) && reachable.insert(root).second) {
                    worklist.push_back(root);

                }

            }

            // Depth-first search all reachable objects
            while (!worklist.empty()) {
                void* current = worklist.back();
                worklist.pop_back();

                // Check all objects referenced by the current object
                auto it = references.find(current);
                if (it != references.end()) {
                    for (void* ref : it->second) {
                        if (heap.count(ref) && reachable.insert(ref).second) {
                            worklist.push_back(ref);

                        }

                    }

                }

            }

            return reachable.size();

        }

    } // namespace VM
} // namespace steve