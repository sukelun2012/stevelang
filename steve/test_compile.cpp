/*
 * Copyright (c) 2024 Kekun Su(苏科纶).
 * 
 * Refer to the LICENSE file for full license information.
 * SPDX-License-Identifier: MIT
 */

#include "vm.h"
#include "vm_gc.h"
#include "vm_jit.h"
#include "vm_exception.h"
#include "language.h"
#include "gc.h"
#include "mem.h"

int main() {
    // Create a simple VirtualMachine instance to test if everything compiles properly
    steve::VM::VirtualMachine vm;
    return 0;
}