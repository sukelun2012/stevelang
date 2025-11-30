/*
 * Copyright (c) 2024 Kekun Su(苏科纶).
 * 
 * Refer to the LICENSE file for full license information.
 * SPDX-License-Identifier: MIT
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include "vm.h"
#include "language.h"
using namespace std;

int main(int argc, char** argv) {
    // Initialize language system

    language::initLanguage();

    if (argc < 2) {
        cerr << language::localize("Usage") << endl;
        return 1;
    }

    const char* fname = argv[1];
    ifstream in(fname, ios::binary);

    if (!in) {
        cerr << language::localize("FileNotFound") + ": " + fname << endl;
        return 1;
    }

    std::ostringstream ss;
    ss << in.rdbuf();
    string src = ss.str();

    // Create and run virtual machine
    steve::VM::VirtualMachine vm;

    try {
        if (!vm.loadProgram(fname)) {
            std::cerr << language::localize("InternalError") + ": Failed to load program" << std::endl;
            return 1;
        }

        if (!vm.execute()) {
            std::cerr << language::localize("InternalError") + ": Failed to execute program" << std::endl;
            return 1;
        }
    }
    catch (const std::exception& e) {
        std::cerr << language::localize("InternalError") + ": " + e.what() << std::endl;
        return 1;
    }
    catch (...) {
        std::cerr << language::localize("InternalError") + ": Unknown exception occurred" << std::endl;
        return 1;
    }

    return 0;
}