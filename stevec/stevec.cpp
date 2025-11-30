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
#include "lexer.h"
#include "parser.h"
#include "sema.h"
#include "codegen.h"
#include "backend.h"  // Include backend code generator
#include "language.h"
#include "gc.h"  // Include garbage collector
using namespace std;

int main(int argc, char** argv) {

    steve::initLanguage();
    steve::initGC();  // Initialize garbage collector

    if (argc < 2) {

        cerr << steve::localize("Usage") << endl;
        steve::cleanupGC();  // Cleanup garbage collector
        return 1;

    }

    const char* fname = argv[1];
    ifstream in(fname, ios::binary);

    if (!in) {

        steve::reportError("FileNotFound", fname, true);
        steve::cleanupGC();  // Cleanup garbage collector

    }

    std::ostringstream ss;
    ss << in.rdbuf();
    string src = ss.str();

    // tokenize
    steve::Lexer lex(src);
    auto tokens = lex.tokenize();

    // parse (fatal)
    steve::Parser parser(tokens);
    auto prog = parser.parse(true);

    // semantic checks
    steve::Sema sema(prog.get());
    sema.run(true);

    // Generate output filename with .ste extension
    std::string inputFileName(fname);
    std::string outputFileName = inputFileName.substr(0, inputFileName.find_last_of('.')) + ".ste";

    // Use backend code generator to generate assembly code and save to .ste file
    steve::Backend::CodeGenerator backend;
    backend.generate(prog.get(), outputFileName);
    steve::cleanupGC();  // Cleanup garbage collector
    return 0;

}