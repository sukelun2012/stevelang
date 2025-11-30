/*
 * Copyright (c) 2024 Kekun Su(苏科纶).
 * 
 * Refer to the LICENSE file for full license information.
 * SPDX-License-Identifier: MIT
 */

#include "vm_exception.h"
#include <string>

namespace steve {
    namespace VM {

        VMException::VMException(const std::string& msg, int l, int c)
            : message(msg), line(l), column(c) {
        }

        const char* VMException::what() const noexcept {
            return message.c_str();
        }

        int VMException::getLine() const {
            return line;
        }

        int VMException::getColumn() const {
            return column;
        }

        RuntimeError::RuntimeError(const std::string& msg, int l, int c)
            : VMException("Runtime Error: " + msg, l, c) {
        }

        TypeError::TypeError(const std::string& msg, int l, int c)
            : VMException("Type Error: " + msg, l, c) {
        }

        AccessError::AccessError(const std::string& msg, int l, int c)
            : VMException("Access Error: " + msg, l, c) {
        }

        MemoryError::MemoryError(const std::string& msg, int l, int c)
            : VMException("Memory Error: " + msg, l, c) {
        }

    } // namespace VM
} // namespace steve