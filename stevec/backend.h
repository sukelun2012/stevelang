/*
 * Copyright (c) 2024 Kekun Su(苏科纶).
 * 
 * Refer to the LICENSE file for full license information.
 * SPDX-License-Identifier: MIT
 */

#ifndef STEVE_BACKEND_H

#define STEVE_BACKEND_H

#include "ast.h"
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <sstream>  // For std::ostringstream

namespace steve {

namespace Backend {

class CodeGenerator {
private:
    std::ostream* out;
    int labelCounter;
    std::vector<std::string> stack;
    std::unordered_map<std::string, int> variables; // variable name to stack offset
    std::vector<std::string> functions;
    int currentOffset;

public:
    CodeGenerator();
    ~CodeGenerator();
    
    // Generate assembly code from AST
    void generate(AST::Program* prog, const std::string& outputFile);
    
    // Generate code for different AST node types
    void genNode(AST::Node* n);
    void genStatement(AST::Statement* s);
    void genExpression(AST::Expression* e);
    
    // Generate specific instruction types
    void genVarDecl(AST::VarDecl* v);
    void genFuncDecl(AST::FuncDecl* f);
    void genIfStmt(AST::IfStmt* iff);
    void genWhileStmt(AST::WhileStmt* ws);
    void genForStmt(AST::ForStmt* fs);
    void genReturnStmt(AST::ReturnStmt* rs);
    void genExprStmt(AST::ExprStmt* es);
    void genBlockStmt(AST::BlockStmt* b);
    void genCallExpr(AST::CallExpr* c);
    void genBinaryExpr(AST::BinaryExpr* b);
    void genUnaryExpr(AST::UnaryExpr* u);
    void genIdentifier(AST::Identifier* id);
    void genLiteral(AST::Literal* lit);
    void genMemberExpr(AST::MemberExpr* m);
    void genIndexExpr(AST::IndexExpr* ix);
    void genListExpr(AST::ListExpr* l);
    void genDictExpr(AST::DictExpr* d);
    void genClassDecl(AST::ClassDecl* c);
    void genImportDecl(AST::ImportDecl* imp);
    void genPackageDecl(AST::PackageDecl* pd);
    void genTryStmt(AST::TryStmt* ts);
    void genBreakStmt(AST::BreakStmt* bs);
    void genContinueStmt(AST::ContinueStmt* cs);
    void genPassStmt(AST::PassStmt* ps);
    
    // Helper functions
    std::string newLabel();
    void emit(const std::string& instruction);
    void emitLine(const std::string& instruction);
    void setupFunction(const std::string& name, const std::vector<std::pair<std::string, std::string>>& params);
    void teardownFunction();
};

} // namespace Backend
} // namespace steve

#endif // STEVE_BACKEND_H