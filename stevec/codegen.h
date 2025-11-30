/*
 * Copyright (c) 2024 Kekun Su(苏科纶).
 * 
 * Refer to the LICENSE file for full license information.
 * SPDX-License-Identifier: MIT
 */

#ifndef STEVE_CODEGEN_H
#define STEVE_CODEGEN_H

#include "ast.h"
#include <ostream>

namespace steve {

class CodeGen {
public:
    explicit CodeGen(std::ostream& out);
    void generate(AST::Program* prog);

private:
    std::ostream& out;
    int indent = 0;
    void writeIndent();
    void genNode(AST::Node* n);
    void genStatement(AST::Statement* s);
    void genExpression(AST::Expression* e);
};

} // namespace steve

#endif // STEVE_CODEGEN_H