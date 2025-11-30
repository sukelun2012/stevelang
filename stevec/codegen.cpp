/*
 * Copyright (c) 2024 Kekun Su(苏科纶).
 * 
 * Refer to the LICENSE file for full license information.
 * SPDX-License-Identifier: MIT
 */

#include "codegen.h"
#include <iostream>

using namespace steve;
using namespace steve::AST;

CodeGen::CodeGen(std::ostream& out) : out(out) {}

void CodeGen::writeIndent() {
    for (int i=0;i<indent;i++) out << "  ";
}

void CodeGen::generate(AST::Program* prog) {
    out << "# IR BEGIN\n";
    for (auto &n : prog->topLevel) genNode(n.get());
    out << "# IR END\n";
}

void CodeGen::genNode(AST::Node* n) {
    if (!n) return;
    if (auto s = dynamic_cast<Statement*>(n)) genStatement(s);
    else if (auto e = dynamic_cast<Expression*>(n)) genExpression(e);
}

void CodeGen::genStatement(AST::Statement* s) {
    if (auto v = dynamic_cast<VarDecl*>(s)) {
        writeIndent();
        out << "DEFVAR " << v->name;
        if (!v->typeName.empty()) out << " :" << v->typeName;
        out << "\n";
        if (v->init) {
            writeIndent(); out << "  ; init\n";
            writeIndent(); out << "  LOAD ";
            genExpression(v->init.get()); out << "\n";
            writeIndent(); out << "  STORE " << v->name << "\n";
        }
    } else if (auto f = dynamic_cast<FuncDecl*>(s)) {
        writeIndent();
        // Include access modifier in function declaration if not default
        if (f->access != AST::AccessModifier::Default) {
            std::string accessStr = "";
            switch (f->access) {
                case AST::AccessModifier::Public: accessStr = "public "; break;
                case AST::AccessModifier::Private: accessStr = "private "; break;
                case AST::AccessModifier::Protected: accessStr = "protected "; break;
                default: break;
            }
            out << accessStr;
        }
        out << "FUNC " << f->name << "(";
        bool first=true;
        for (auto &p : f->params) {
            if (!first) out << ", ";
            out << (p.first.empty() ? "any" : p.first) << " " << p.second;
            first=false;
        }
        out << ")";
        if (!f->returnType.empty()) out << " -> " << f->returnType;
        out << " {\n";
        indent++;
        if (f->body) genStatement(f->body.get());
        indent--;
        writeIndent();
        out << "}\n";
    } else if (auto c = dynamic_cast<ClassDecl*>(s)) {
        writeIndent();
        out << "CLASS " << c->name;
        if (!c->base.empty()) out << " EXTENDS " << c->base;
        out << " {\n";
        indent++;
        if (c->body) genStatement(c->body.get());
        indent--;
        writeIndent();
        out << "}\n";
    } else if (auto pd = dynamic_cast<PackageDecl*>(s)) {
        writeIndent();
        out << "; PACKAGE " << pd->packageName << "\n";
    } else if (auto b = dynamic_cast<BlockStmt*>(s)) {
        for (auto &st : b->stmts) genNode(st.get());
    } else if (auto es = dynamic_cast<ExprStmt*>(s)) {
        writeIndent();
        genExpression(es->expr.get());
        out << "\n";
    } else if (auto iff = dynamic_cast<IfStmt*>(s)) {
        writeIndent(); out << "IF ";
        genExpression(iff->cond.get()); out << " THEN\n";
        indent++; genStatement(iff->thenBranch.get()); indent--;
        if (iff->elseBranch) {
            writeIndent(); out << "ELSE\n";
            indent++; genStatement(iff->elseBranch.get()); indent--;
        }
        writeIndent(); out << "END\n";
    } else if (auto ws = dynamic_cast<WhileStmt*>(s)) {
        writeIndent(); out << "WHILE "; genExpression(ws->cond.get()); out << " DO\n";
        indent++; genStatement(ws->body.get()); indent--;
        writeIndent(); out << "END\n";
    } else if (auto fs = dynamic_cast<ForStmt*>(s)) {
        writeIndent(); out << "FOR ... DO\n";
        indent++; genStatement(fs->body.get()); indent--;
        writeIndent(); out << "END\n";
    } else if (auto rs = dynamic_cast<ReturnStmt*>(s)) {
        writeIndent(); out << "RETURN";
        if (rs->value) { out << " "; genExpression(rs->value.get()); }
        out << "\n";
    } else if (auto imp = dynamic_cast<ImportDecl*>(s)) {
        writeIndent(); out << "IMPORT " << imp->module;
        if (!imp->name.empty()) out << " FROM " << imp->name;
        if (!imp->alias.empty()) out << " AS " << imp->alias;
        out << "\n";
    } else if (auto ts = dynamic_cast<TryStmt*>(s)) {
        writeIndent(); out << "; TRY-CATCH block\n";
        writeIndent(); out << "TRY {\n";
        indent++; genStatement(ts->tryBlock.get()); indent--;
        writeIndent(); out << "} CATCH(" << ts->exceptionVar << ") {\n";
        indent++; if (ts->catchBlock) genStatement(ts->catchBlock.get()); indent--;
        writeIndent(); out << "}\n";
    } else if (auto bs = dynamic_cast<BreakStmt*>(s)) {
        writeIndent(); out << "BREAK\n";
    } else if (auto cs = dynamic_cast<ContinueStmt*>(s)) {
        writeIndent(); out << "CONTINUE\n";
    } else if (auto ps = dynamic_cast<PassStmt*>(s)) {
        writeIndent(); out << "; PASS (no operation)\n";
    }
}

void CodeGen::genExpression(AST::Expression* e) {
    if (!e) return;
    if (auto id = dynamic_cast<Identifier*>(e)) out << id->name;
    else if (auto lit = dynamic_cast<Literal*>(e)) out << "\"" << lit->raw << "\"";
    else if (auto bin = dynamic_cast<BinaryExpr*>(e)) {
        out << "(";
        genExpression(bin->left.get());
        out << " " << bin->op << " ";
        genExpression(bin->right.get());
        out << ")";
    } else if (auto u = dynamic_cast<UnaryExpr*>(e)) {
        out << u->op;
        genExpression(u->operand.get());
    } else if (auto c = dynamic_cast<CallExpr*>(e)) {
        // Check if it's a built-in garbage collection or memory management function
        if (auto calleeId = dynamic_cast<Identifier*>(c->callee.get())) {
            std::string funcName = calleeId->name;
            if (funcName == "new" || funcName == "delete" || funcName == "gc") {
                out << "GC_" << funcName << "(";
                bool first = true;
                for (auto &a : c->args) {
                    if (!first) out << ", ";
                    genExpression(a.get());
                    first = false;
                }
                out << ")";
            } else if (funcName == "malloc" || funcName == "free" || funcName == "realloc" || funcName == "calloc" ||
                       funcName == "memcpy" || funcName == "memmove" || funcName == "memcmp" || funcName == "memset" ||
                       funcName == "sizeofType" || funcName == "sizeofVar") {
                out << "MEM_" << funcName << "(";
                bool first = true;
                for (auto &a : c->args) {
                    if (!first) out << ", ";
                    genExpression(a.get());
                    first = false;
                }
                out << ")";
            } else {
                genExpression(c->callee.get());
                out << "(";
                bool first = true;
                for (auto &a : c->args) {
                    if (!first) out << ", ";
                    genExpression(a.get());
                    first = false;
                }
                out << ")";
            }
        } else {
            genExpression(c->callee.get());
            out << "(";
            bool first = true;
            for (auto &a : c->args) {
                if (!first) out << ", ";
                genExpression(a.get());
                first = false;
            }
            out << ")";
        }
    } else if (auto m = dynamic_cast<MemberExpr*>(e)) {
        genExpression(m->obj.get());
        out << "." << m->member;
    } else if (auto ix = dynamic_cast<IndexExpr*>(e)) {
        genExpression(ix->obj.get());
        out << "[";
        genExpression(ix->index.get());
        out << "]";
    } else if (auto l = dynamic_cast<ListExpr*>(e)) {
        out << "[";
        bool first=true;
        for (auto &it : l->items) {
            if (!first) out << ", ";
            genExpression(it.get());
            first=false;
        }
        out << "]";
    } else if (auto d = dynamic_cast<DictExpr*>(e)) {
        out << "{";
        bool first=true;
        for (auto &p : d->pairs) {
            if (!first) out << ", ";
            genExpression(p.first.get());
            out << ": ";
            genExpression(p.second.get());
            first=false;
        }
        out << "}";
    }
}