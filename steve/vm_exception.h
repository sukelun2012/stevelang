/*
 * Copyright (c) 2024 Kekun Su(苏科纶).
 * 
 * Refer to the LICENSE file for full license information.
 * SPDX-License-Identifier: MIT
 */

#ifndef STEVE_VM_EXCEPTION_H
#define STEVE_VM_EXCEPTION_H
#include <exception>
#include <string>

namespace steve {
    namespace VM {

        // Virtual machine exception base class
        class VMException : public std::exception {
        protected:
            std::string message;
            int line;
            int column;

        public:
            VMException(const std::string& msg, int l = -1, int c = -1)
                : message(msg), line(l), column(c) {
            }

            virtual const char* what() const noexcept override {
                return message.c_str();
            }

            int getLine() const { return line; }
            int getColumn() const { return column; }
        };

        // Runtime error exception
        class RuntimeError : public VMException {
        public:
            RuntimeError(const std::string& msg, int l = -1, int c = -1)
                : VMException("Runtime Error: " + msg, l, c) {
            }
        };

        // Type error exception
        class TypeError : public VMException {
        public:
            TypeError(const std::string& msg, int l = -1, int c = -1)
                : VMException("Type Error: " + msg, l, c) {
            }
        };

        // Access error exception
        class AccessError : public VMException {
        public:
            AccessError(const std::string& msg, int l = -1, int c = -1)
                : VMException("Access Error: " + msg, l, c) {
            }
        };

        // Memory error exception
        class MemoryError : public VMException {
        public:
            MemoryError(const std::string& msg, int l = -1, int c = -1)
                : VMException("Memory Error: " + msg, l, c) {
            }
        };

    } // namespace VM
} // namespace steve

#endif // STEVE_VM_EXCEPTION_H