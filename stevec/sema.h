/*
 * Copyright (c) 2024 Kekun Su(苏科纶).
 * 
 * Refer to the LICENSE file for full license information.
 * SPDX-License-Identifier: MIT
 */

#ifndef STEVE_SEMA_H
#define STEVE_SEMA_H
#include "ast.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>

namespace steve {

    struct Symbol {
        enum class Kind { Variable, Function, Class, Module };
        Kind kind;
        std::string name;
        std::string type; // variable/class type or "function"/"module"
        std::string returnType; // for functions: return type (may be empty)
    };

    class SymbolTable {
    public:
        void enterScope();
        void leaveScope();
        bool declare(const std::string& name, const Symbol& sym, std::string& outErr);
        Symbol* resolve(const std::string& name);

        // class field storage: className -> (fieldName -> type)
        std::unordered_map<std::string, std::unordered_map<std::string, std::string>> classFields;

        // class methods: className -> (methodName -> returnType)
        std::unordered_map<std::string, std::unordered_map<std::string, std::string>> classMethods;

        // module exports: moduleName -> (exportName -> Symbol)
        std::unordered_map<std::string, std::unordered_map<std::string, Symbol>> moduleExports;

        // loaded modules set to avoid duplicate loads / cycles
        std::unordered_set<std::string> loadedModules;

        // get a copy of global scope symbols (top-level of a module)
        std::unordered_map<std::string, Symbol> getGlobalSymbols() const;

    private:
        std::vector<std::unordered_map<std::string, Symbol>> scopes;
    };

    class Sema {

    public:

        explicit Sema(AST::Program* prog);

        // run semantic analysis; if fatal==true, will call steve::reportError on errors.
        void run(bool fatal = true);

        std::vector<std::string> errors;

        // expose symbol table for callers that need to inspect it (e.g. module loader)
        SymbolTable& symbolTable() { return table; }

        // get global symbols for module importing
        std::unordered_map<std::string, Symbol> getGlobalSymbols() const { return table.getGlobalSymbols(); }

    private:

        AST::Program* prog;
        SymbolTable table;



        void visitNode(AST::Node* n);
        void visitStatement(AST::Statement* s);
        void visitExpression(AST::Expression* e);
        void visitVarDecl(AST::VarDecl* d);
        void visitFuncDecl(AST::FuncDecl* f);
        void visitClassDecl(AST::ClassDecl* c);
        void visitImport(AST::ImportDecl* im);



        // 推断/检查
        std::string inferExpressionType(AST::Expression* e);
        bool isNumericType(const std::string& t);
        bool isAssignable(const std::string& ltype, const std::string& rtype);

    };

} // namespace steve

#endif // STEVE_SEMA_H