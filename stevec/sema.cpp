/*
 * Copyright (c) 2024 Kekun Su(苏科纶).
 * 
 * Refer to the LICENSE file for full license information.
 * SPDX-License-Identifier: MIT
 */

#include "sema.h"
#include "language.h"
#include "lexer.h"
#include "parser.h"
#include <fstream>
#include <sstream>
#include <algorithm>
using namespace steve;
using namespace steve::AST;

void SymbolTable::enterScope() {
    scopes.emplace_back();
}
void SymbolTable::leaveScope() {
    if (!scopes.empty()) scopes.pop_back();
}
bool SymbolTable::declare(const std::string& name, const Symbol& sym, std::string& outErr) {
    if (scopes.empty()) enterScope();
    auto &cur = scopes.back();
    if (cur.find(name) != cur.end()) {
        outErr = "Duplicate symbol: " + name;
        return false;
    }
    cur[name] = sym;
    return true;
}
Symbol* SymbolTable::resolve(const std::string& name) {
    for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
        auto found = it->find(name);
        if (found != it->end()) return &found->second;
    }
    return nullptr;
}
std::unordered_map<std::string, Symbol> SymbolTable::getGlobalSymbols() const {
    if (scopes.empty()) return {};
    return scopes.front();
}

Sema::Sema(AST::Program* prog) : prog(prog) {}

// Helper: load and analyze module, return its exported symbols (non-fatal)
static std::pair<bool, std::unordered_map<std::string, Symbol>> loadModuleAndCollectExports(SymbolTable &table, const std::string& moduleName, std::vector<std::string> &errorsOut) {
    if (moduleName.empty()) {
        errorsOut.push_back(steve::localize("ImportError", "Empty module name"));
        return {false, std::unordered_map<std::string, Symbol>{}};
    }

    if (table.loadedModules.find(moduleName) != table.loadedModules.end()) {
        auto it = table.moduleExports.find(moduleName);
        if (it != table.moduleExports.end()) return {true, it->second};
        return {true, std::unordered_map<std::string, Symbol>{}};
    }

    std::string tryfile1 = moduleName + ".steve";
#ifdef _WIN32
    std::string path = moduleName;
    for (auto &c : path) if (c == '.') c = '\\';
    std::string tryfile2 = path + ".steve";
#else
    std::string path = moduleName;
    for (auto &c : path) if (c == '.') c = '/';
    std::string tryfile2 = path + ".steve";
#endif

    std::ifstream in(tryfile1, std::ios::binary);
    if (!in) in.open(tryfile2, std::ios::binary);
    if (!in) {
        errorsOut.push_back(steve::localize("ImportError", "Module file not found: " + tryfile1 + " or " + tryfile2));
        return {false, std::unordered_map<std::string, Symbol>{}};
    }

    std::ostringstream ssbuf;
    ssbuf << in.rdbuf();
    std::string src = ssbuf.str();

    // Lex + Parse (non-fatal) + Sema (non-fatal)
    steve::Lexer lex(src);
    auto tokens = lex.tokenize();
    steve::Parser parser(tokens);
    auto moduleProg = parser.parse(false); // non-fatal parse
    if (!parser.errors.empty()) {
        std::ostringstream es;
        for (auto &e : parser.errors) es << e << "\n";
        errorsOut.push_back(steve::localize("ImportError", es.str()));
        return {false, std::unordered_map<std::string, Symbol>{}};
    }

    Sema moduleSema(moduleProg.get());
    moduleSema.run(false); // non-fatal
    if (!moduleSema.errors.empty()) {
        std::ostringstream es;
        for (auto &e : moduleSema.errors) es << e << "\n";
        errorsOut.push_back(steve::localize("ImportError", es.str()));
        return {false, std::unordered_map<std::string, Symbol>{}};
    }

    auto globals = moduleSema.getGlobalSymbols();  // Use public method to access symbols
    table.loadedModules.insert(moduleName);
    table.moduleExports[moduleName] = globals;
    return {true, globals};
}

void Sema::run(bool fatal) {
    table.enterScope();
    // 注册内置函数类型
    Symbol printSym; printSym.kind = Symbol::Kind::Function; printSym.name = "print"; printSym.type = "function"; printSym.returnType = "any";
    std::string ignore;
    table.declare("print", printSym, ignore);
    
    // Register type conversion functions
    Symbol intSym; intSym.kind = Symbol::Kind::Function; intSym.name = "int"; intSym.type = "function"; intSym.returnType = "int";
    table.declare("int", intSym, ignore);
    Symbol stringSym; stringSym.kind = Symbol::Kind::Function; stringSym.name = "string"; stringSym.type = "function"; stringSym.returnType = "string";
    table.declare("string", stringSym, ignore);
    Symbol floatSym; floatSym.kind = Symbol::Kind::Function; floatSym.name = "float"; floatSym.type = "function"; floatSym.returnType = "float";
    table.declare("float", floatSym, ignore);
    Symbol boolSym; boolSym.kind = Symbol::Kind::Function; boolSym.name = "bool"; boolSym.type = "function"; boolSym.returnType = "bool";
    table.declare("bool", boolSym, ignore);
    Symbol doubleSym; doubleSym.kind = Symbol::Kind::Function; doubleSym.name = "double"; doubleSym.type = "function"; doubleSym.returnType = "double";
    table.declare("double", doubleSym, ignore);
    Symbol longSym; longSym.kind = Symbol::Kind::Function; longSym.name = "long"; longSym.type = "function"; longSym.returnType = "long";
    table.declare("long", longSym, ignore);
    Symbol shortSym; shortSym.kind = Symbol::Kind::Function; shortSym.name = "short"; shortSym.type = "function"; shortSym.returnType = "short";
    table.declare("short", shortSym, ignore);
    Symbol byteSym; byteSym.kind = Symbol::Kind::Function; byteSym.name = "byte"; byteSym.type = "function"; byteSym.returnType = "byte";
    table.declare("byte", byteSym, ignore);
    
    // Register other built-in functions

    Symbol typeSym; typeSym.kind = Symbol::Kind::Function; typeSym.name = "type"; typeSym.type = "function"; typeSym.returnType = "string";

    table.declare("type", typeSym, ignore);

    Symbol hashSym; hashSym.kind = Symbol::Kind::Function; hashSym.name = "hash"; hashSym.type = "function"; hashSym.returnType = "string";

    table.declare("hash", hashSym, ignore);

    Symbol bsSym; bsSym.kind = Symbol::Kind::Function; bsSym.name = "bs"; bsSym.type = "function"; bsSym.returnType = "string";

    table.declare("bs", bsSym, ignore);

    Symbol openSym; openSym.kind = Symbol::Kind::Function; openSym.name = "open"; openSym.type = "function"; openSym.returnType = "string";

    table.declare("open", openSym, ignore);

    Symbol inputSym; inputSym.kind = Symbol::Kind::Function; inputSym.name = "input"; inputSym.type = "function"; inputSym.returnType = "string";

    table.declare("input", inputSym, ignore);

    Symbol closeSym; closeSym.kind = Symbol::Kind::Function; closeSym.name = "close"; closeSym.type = "function"; closeSym.returnType = "any";

    table.declare("close", closeSym, ignore);

    Symbol delSym; delSym.kind = Symbol::Kind::Function; delSym.name = "del"; delSym.type = "function"; delSym.returnType = "any";

    table.declare("del", delSym, ignore);

    Symbol appendSym; appendSym.kind = Symbol::Kind::Function; appendSym.name = "append"; appendSym.type = "function"; appendSym.returnType = "any";

    table.declare("append", appendSym, ignore);

    Symbol runSym; runSym.kind = Symbol::Kind::Function; runSym.name = "run"; runSym.type = "function"; runSym.returnType = "string"; // Returns execution result



    table.declare("run", runSym, ignore);
    // Garbage collection functions
    Symbol newSym; newSym.kind = Symbol::Kind::Function; newSym.name = "new"; newSym.type = "function"; newSym.returnType = "any"; // Creates new objects
    table.declare("new", newSym, ignore);
    Symbol deleteSym; deleteSym.kind = Symbol::Kind::Function; deleteSym.name = "delete"; deleteSym.type = "function"; deleteSym.returnType = "any"; // Marks object for deletion
    table.declare("delete", deleteSym, ignore);
    Symbol gcSym; gcSym.kind = Symbol::Kind::Function; gcSym.name = "gc"; gcSym.type = "function"; gcSym.returnType = "int"; // Returns number of objects collected
    table.declare("gc", gcSym, ignore);
    // Memory management functions
    Symbol mallocSym; mallocSym.kind = Symbol::Kind::Function; mallocSym.name = "malloc"; mallocSym.type = "function"; mallocSym.returnType = "any";
    table.declare("malloc", mallocSym, ignore);
    Symbol freeSym; freeSym.kind = Symbol::Kind::Function; freeSym.name = "free"; freeSym.type = "function"; freeSym.returnType = "any";
    table.declare("free", freeSym, ignore);
    Symbol reallocSym; reallocSym.kind = Symbol::Kind::Function; reallocSym.name = "realloc"; reallocSym.type = "function"; reallocSym.returnType = "any";
    table.declare("realloc", reallocSym, ignore);
    Symbol callocSym; callocSym.kind = Symbol::Kind::Function; callocSym.name = "calloc"; callocSym.type = "function"; callocSym.returnType = "any";
    table.declare("calloc", callocSym, ignore);
    Symbol memcpySym; memcpySym.kind = Symbol::Kind::Function; memcpySym.name = "memcpy"; memcpySym.type = "function"; memcpySym.returnType = "any";
    table.declare("memcpy", memcpySym, ignore);
    Symbol memmoveSym; memmoveSym.kind = Symbol::Kind::Function; memmoveSym.name = "memmove"; memmoveSym.type = "function"; memmoveSym.returnType = "any";
    table.declare("memmove", memmoveSym, ignore);
    Symbol memcmpSym; memcmpSym.kind = Symbol::Kind::Function; memcmpSym.name = "memcmp"; memcmpSym.type = "function"; memcmpSym.returnType = "int";
    table.declare("memcmp", memcmpSym, ignore);
    Symbol memsetSym; memsetSym.kind = Symbol::Kind::Function; memsetSym.name = "memset"; memsetSym.type = "function"; memsetSym.returnType = "any";
    table.declare("memset", memsetSym, ignore);
    Symbol sizeofTypeSym; sizeofTypeSym.kind = Symbol::Kind::Function; sizeofTypeSym.name = "sizeofType"; sizeofTypeSym.type = "function"; sizeofTypeSym.returnType = "int";
    table.declare("sizeofType", sizeofTypeSym, ignore);
    Symbol sizeofVarSym; sizeofVarSym.kind = Symbol::Kind::Function; sizeofVarSym.name = "sizeofVar"; sizeofVarSym.type = "function"; sizeofVarSym.returnType = "int";
    table.declare("sizeofVar", sizeofVarSym, ignore);

    // 分析语法树节点
    if (!prog) return;
    for (auto &nptr : prog->topLevel) visitNode(nptr.get());

    if (!errors.empty()) {
        std::ostringstream ss;
        for (auto &e: errors) ss << e << "\n";
        if (fatal) steve::reportError("InternalError", ss.str(), true);
    }
}

void Sema::visitNode(AST::Node* n) {
    if (!n) return;
    if (auto stmt = dynamic_cast<AST::Statement*>(n)) visitStatement(stmt);
    else if (auto expr = dynamic_cast<AST::Expression*>(n)) visitExpression(expr);
}

void Sema::visitStatement(AST::Statement* s) {

    if (!s) return;

    if (auto v = dynamic_cast<VarDecl*>(s)) visitVarDecl(v);

    else if (auto f = dynamic_cast<FuncDecl*>(s)) visitFuncDecl(f);

    else if (auto c = dynamic_cast<ClassDecl*>(s)) visitClassDecl(c);

    else if (auto pd = dynamic_cast<PackageDecl*>(s)) {

        // Package declaration - just record it

        return;

    }

    else if (auto b = dynamic_cast<BlockStmt*>(s)) {

        table.enterScope();

        for (auto &st : b->stmts) visitNode(st.get());

        table.leaveScope();

    }

    else if (auto e = dynamic_cast<ExprStmt*>(s)) visitExpression(e->expr.get());

    else if (auto iff = dynamic_cast<IfStmt*>(s)) {

        visitExpression(iff->cond.get());

        visitStatement(iff->thenBranch.get());

        if (iff->elseBranch) visitStatement(iff->elseBranch.get());

    }

    else if (auto ws = dynamic_cast<WhileStmt*>(s)) {

        visitExpression(ws->cond.get());

        visitStatement(ws->body.get());

    } else if (auto fs = dynamic_cast<ForStmt*>(s)) {

        if (fs->init) visitStatement(fs->init.get());

        if (fs->cond) visitExpression(fs->cond.get());

        if (fs->step) visitExpression(fs->step.get());

        visitStatement(fs->body.get());

    } else if (auto rs = dynamic_cast<ReturnStmt*>(s)) {

        if (rs->value) visitExpression(rs->value.get());

    } else if (auto imp = dynamic_cast<ImportDecl*>(s)) {

        visitImport(imp);

    } else if (auto ts = dynamic_cast<TryStmt*>(s)) {

        visitStatement(ts->tryBlock.get());

        if (ts->catchBlock) visitStatement(ts->catchBlock.get());

    } else if (auto bs = dynamic_cast<BreakStmt*>(s)) {

        // Break statement - check if inside a loop context (simplified)

        return;

    } else if (auto cs = dynamic_cast<ContinueStmt*>(s)) {

        // Continue statement - check if inside a loop context (simplified)

        return;

    } else if (auto ps = dynamic_cast<PassStmt*>(s)) {

        // Pass statement - no action needed

        return;

    }

}

void Sema::visitImport(AST::ImportDecl* im) {
    if (im->module.empty()) {
        std::ostringstream ss; ss << im->line << ":" << im->column << " - " << "Empty module name";
        errors.push_back(steve::localize("ImportError", ss.str()));
        return;
    }

    std::vector<std::string> loaderErrors;
    auto result = loadModuleAndCollectExports(table, im->module, loaderErrors);
    if (!result.first) {  // not successful
        for (auto &e : loaderErrors) errors.push_back(e);
        return;
    }

    auto exports = result.second;

    if (im->isFrom && !im->name.empty() && im->name != "*") {
        auto it = exports.find(im->name);
        if (it == exports.end()) {
            std::ostringstream ss; ss << im->line << ":" << im->column << " - " << im->module << "." << im->name;
            errors.push_back(steve::localize("ImportError", ss.str()));
            return;
        }
        Symbol s = it->second;
        std::string declareName = im->alias.empty() ? im->name : im->alias;
        std::string derr;
        if (!table.declare(declareName, s, derr)) errors.push_back(steve::localize("InternalError", derr));
        return;
    }

    std::string modName = im->alias.empty() ? im->module : im->alias;
    Symbol modSym; modSym.kind = Symbol::Kind::Module; modSym.name = im->module; modSym.type = "module";
    std::string derr;
    if (!table.declare(modName, modSym, derr)) errors.push_back(steve::localize("InternalError", derr));
    table.moduleExports[modName] = exports;
}

void Sema::visitExpression(AST::Expression* e) {
    if (!e) return;
    if (auto id = dynamic_cast<Identifier*>(e)) {
        Symbol* sym = table.resolve(id->name);
        if (!sym) {
            std::ostringstream ss; ss << id->line << ":" << id->column << " - " << id->name;
            errors.push_back(steve::localize("UndefinedIdentifier", ss.str()));
            id->inferredType = "any";
        } else {
            id->inferredType = sym->type.empty() ? "any" : sym->type;
        }
    } else if (auto b = dynamic_cast<BinaryExpr*>(e)) {
        if (b->left) visitExpression(b->left.get());
        if (b->right) visitExpression(b->right.get());
        b->inferredType = inferExpressionType(b);
    } else if (auto u = dynamic_cast<UnaryExpr*>(e)) {
        if (u->operand) visitExpression(u->operand.get());
        u->inferredType = inferExpressionType(u);
    } else if (auto c = dynamic_cast<CallExpr*>(e)) {
        if (c->callee) visitExpression(c->callee.get());
        for (auto &a : c->args) visitExpression(a.get());
        if (auto calleeId = dynamic_cast<Identifier*>(c->callee.get())) {

            // Handle built-in functions



            std::string funcName = calleeId->name;



            // Handle built-in functions

            if (funcName == "type" || funcName == "hash" || funcName == "bs" || funcName == "open" || funcName == "input" || funcName == "run" || 

                funcName == "new" || funcName == "delete" || funcName == "gc" ||

                funcName == "malloc" || funcName == "free" || funcName == "realloc" || funcName == "calloc" ||

                funcName == "memcpy" || funcName == "memmove" || funcName == "memcmp" || funcName == "memset" ||

                funcName == "sizeofType" || funcName == "sizeofVar") {

                // These functions return specific types

                if (funcName == "type" || funcName == "hash" || funcName == "run" || funcName == "bs" ||

                    funcName == "malloc" || funcName == "realloc" || funcName == "calloc" ||

                    funcName == "memcpy" || funcName == "memmove" || funcName == "memset") {

                    c->inferredType = "any";  // Memory functions return pointer-like values

                } else if (funcName == "gc" || funcName == "memcmp" || funcName == "sizeofType" || funcName == "sizeofVar") {

                    c->inferredType = "int";  // gc returns number of objects collected, others return int

                } else if (funcName == "open") {

                    c->inferredType = "string";  // File handle represented as string

                } else if (funcName == "input") {

                    c->inferredType = "string";

                } else {

                    c->inferredType = "any";

                }

            }

            // Handle type conversion functions: int(var), string(var), float(var), etc.

            else if (funcName == "int" || funcName == "string" || funcName == "float" || funcName == "bool" ||

                     funcName == "double" || funcName == "long" || funcName == "short" || funcName == "byte") {

                c->inferredType = funcName;  // Return the target type

            }

            else {

                Symbol* cs = table.resolve(calleeId->name);

                if (cs && cs->kind == Symbol::Kind::Function && !cs->returnType.empty()) {

                    c->inferredType = cs->returnType;

                } else {

                    c->inferredType = "any";

                }

            }
        } else if (auto calleeMem = dynamic_cast<MemberExpr*>(c->callee.get())) {
            if (calleeMem->obj) visitExpression(calleeMem->obj.get());
            std::string objt = calleeMem->obj->inferredType.empty() ? "any" : calleeMem->obj->inferredType;
            auto mit = table.moduleExports.find(objt);
            if (mit != table.moduleExports.end()) {
                auto &exports = mit->second;
                auto eit = exports.find(calleeMem->member);
                if (eit != exports.end()) {
                    if (eit->second.kind == Symbol::Kind::Function && !eit->second.returnType.empty()) {
                        c->inferredType = eit->second.returnType;
                    } else {
                        c->inferredType = eit->second.type.empty() ? "any" : eit->second.type;
                    }
                } else c->inferredType = "any";
            } else {
                auto cmIt = table.classMethods.find(objt);
                if (cmIt != table.classMethods.end()) {
                    auto &mmap = cmIt->second;
                    auto mit2 = mmap.find(calleeMem->member);
                    if (mit2 != mmap.end()) c->inferredType = mit2->second.empty() ? "any" : mit2->second;
                    else c->inferredType = "any";
                } else c->inferredType = "any";
            }
        } else {
            c->inferredType = "any";
        }
    } else if (auto m = dynamic_cast<MemberExpr*>(e)) {
        if (m->obj) visitExpression(m->obj.get());
        std::string objt = m->obj->inferredType.empty() ? "any" : m->obj->inferredType;
        auto modIt = table.moduleExports.find(objt);
        if (modIt != table.moduleExports.end()) {
            auto &exports = modIt->second;
            auto it = exports.find(m->member);
            if (it != exports.end()) {
                if (it->second.kind == Symbol::Kind::Function && !it->second.returnType.empty())
                    m->inferredType = it->second.returnType;
                else m->inferredType = it->second.type.empty() ? "any" : it->second.type;
                return;
            } else {
                std::ostringstream ss; ss << m->line << ":" << m->column << " - " << objt << "." << m->member;
                errors.push_back(steve::localize("UndefinedIdentifier", ss.str()));
                m->inferredType = "any";
                return;
            }
        }
        auto cfIt = table.classFields.find(objt);
        if (cfIt != table.classFields.end()) {
            auto &fields = cfIt->second;
            auto fit = fields.find(m->member);
            if (fit != fields.end()) {
                m->inferredType = fit->second;
                return;
            }
            auto cmIt = table.classMethods.find(objt);
            if (cmIt != table.classMethods.end()) {
                auto &mmap = cmIt->second;
                auto mit = mmap.find(m->member);
                if (mit != mmap.end()) {
                    m->inferredType = mit->second.empty() ? "function" : mit->second;
                    return;
                }
            }
            std::ostringstream ss; ss << m->line << ":" << m->column << " - " << objt << "." << m->member;
            errors.push_back(steve::localize("UndefinedIdentifier", ss.str()));
            m->inferredType = "any";
            return;
        }
        m->inferredType = "any";
    } else if (auto idx = dynamic_cast<IndexExpr*>(e)) {
        if (idx->obj) visitExpression(idx->obj.get());
        if (idx->index) visitExpression(idx->index.get());
        idx->inferredType = "any";
    } else if (auto l = dynamic_cast<ListExpr*>(e)) {
        for (auto &it: l->items) visitExpression(it.get());
        l->inferredType = "list";
    } else if (auto d = dynamic_cast<DictExpr*>(e)) {
        for (auto &p: d->pairs) { visitExpression(p.first.get()); visitExpression(p.second.get()); }
        d->inferredType = "dict";
    } else if (auto lit = dynamic_cast<Literal*>(e)) {
        std::string raw = lit->raw;
        if (raw == "true" || raw == "false") lit->inferredType = "bool";
        else if (raw == "null") lit->inferredType = "null";
        else {
            bool allDigits = !raw.empty() && std::all_of(raw.begin(), raw.end(), [](char c){ return std::isdigit(static_cast<unsigned char>(c)) || c=='.' || c=='-'; });
            if (allDigits) lit->inferredType = (raw.find('.') != std::string::npos) ? "float" : "int";
            else lit->inferredType = "string";
        }
    }
}

void Sema::visitVarDecl(AST::VarDecl* d) {
    // Check if variable is already declared in current scope
    if (table.resolve(d->name)) {
        std::ostringstream ss;
        ss << "Variable '" << d->name << "' already declared in this scope";
        errors.push_back(ss.str());
        return;
    }

    // Process type name for pointer types
    std::string processedType = d->typeName;
    // Here we could add more sophisticated type processing for pointer types
    // For example, normalize ptr<int> to a standard format

    // Declare the variable
    Symbol sym;
    sym.kind = Symbol::Kind::Variable;
    sym.name = d->name;
    sym.type = processedType; // Use processed type name
    sym.returnType = "";

    std::string err;
    if (!table.declare(d->name, sym, err)) {
        errors.push_back(err);
    }
}

void Sema::visitFuncDecl(AST::FuncDecl* f) {
    Symbol sym; sym.kind = Symbol::Kind::Function; sym.name = f->name; sym.type = "function"; sym.returnType = f->returnType;
    std::string err;
    if (!table.declare(f->name, sym, err)) errors.push_back(steve::localize("InternalError", err));
    table.enterScope();
    for (auto &p : f->params) {
        Symbol ps; ps.kind = Symbol::Kind::Variable; ps.name = p.second; ps.type = p.first.empty() ? "any" : p.first;
        std::string perr; table.declare(ps.name, ps, perr);
    }
    if (f->body) visitStatement(f->body.get());
    table.leaveScope();
}

void Sema::visitClassDecl(AST::ClassDecl* c) {
    Symbol sym; sym.kind = Symbol::Kind::Class; sym.name = c->name; sym.type = c->name;
    std::string err;
    if (!table.declare(c->name, sym, err)) { errors.push_back(steve::localize("InternalError", err)); return; }
    table.enterScope();
    table.classFields[c->name] = {};
    table.classMethods[c->name] = {};
    if (c->body) {
        if (auto b = dynamic_cast<BlockStmt*>(c->body.get())) {
            for (auto &m : b->stmts) {
                if (auto vd = dynamic_cast<VarDecl*>(m.get())) {
                    std::string ftype = vd->typeName.empty() ? "any" : vd->typeName;
                    table.classFields[c->name][vd->name] = ftype;
                } else if (auto fd = dynamic_cast<FuncDecl*>(m.get())) {
                    table.classMethods[c->name][fd->name] = fd->returnType.empty() ? "any" : fd->returnType;
                }
                visitNode(m.get());
            }
        } else {
            visitNode(c->body.get());
        }
    }
    table.leaveScope();
}

bool Sema::isNumericType(const std::string& t) {
    return t == "int" || t == "float" || t == "double" || t == "long" || t == "short" || t == "byte";
}

bool Sema::isAssignable(const std::string& ltype, const std::string& rtype) {
    if (ltype == "any" || rtype == "any") return true;
    if (ltype == rtype) return true;
    if (isNumericType(ltype) && isNumericType(rtype)) return true;
    if (ltype == "string" && (rtype == "int" || rtype == "float")) return true;
    return false;
}

std::string Sema::inferExpressionType(AST::Expression* e) {
    if (!e) return "any";
    if (auto lit = dynamic_cast<Literal*>(e)) {
        if (lit->raw == "true" || lit->raw == "false") return "bool";
        if (lit->raw == "null") return "null";
        bool allDigits = !lit->raw.empty() && std::all_of(lit->raw.begin(), lit->raw.end(), [](char c) { return std::isdigit(static_cast<unsigned char>(c)) || c == '.' || c == '-'; });
        if (allDigits) return (lit->raw.find('.') != std::string::npos) ? "float" : "int";
        return "string";
    }
    if (auto id = dynamic_cast<Identifier*>(e)) {
        Symbol* s = table.resolve(id->name);
        return s ? (s->type.empty() ? "any" : s->type) : "any";
    }
    if (auto b = dynamic_cast<BinaryExpr*>(e)) {
        std::string lop = inferExpressionType(b->left.get());
        std::string rop = inferExpressionType(b->right.get());
        const std::string& op = b->op;
        if (op == "+") {
            if (lop == "string" || rop == "string") return "string";
            if (isNumericType(lop) && isNumericType(rop)) return "int";
            return "any";
        }
        if (op == "-" || op == "*" || op == "/" || op == "//" || op == "%") {
            if (isNumericType(lop) && isNumericType(rop)) return "int";
            return "any";
        }
        if (op == "==" || op == "!=" || op == ">" || op == "<" || op == ">=" || op == "<=") return "bool";
        if (op == "and" || op == "or") return "bool";
        if (op == "=") return lop;
        return "any";
    }
    if (auto u = dynamic_cast<UnaryExpr*>(e)) {
        if (u->op == "-") return inferExpressionType(u->operand.get());
        if (u->op == "not" || u->op == "~" || u->op == "!") return "bool";
        return "any";
    }
    if (auto c = dynamic_cast<CallExpr*>(e)) return c->inferredType.empty() ? "any" : c->inferredType;
    if (auto m = dynamic_cast<MemberExpr*>(e)) {
        if (m->obj) {
            std::string ot = inferExpressionType(m->obj.get());
            auto itc = table.classFields.find(ot);
            if (itc != table.classFields.end()) {
                auto& fields = itc->second;
                auto itf = fields.find(m->member);
                if (itf != fields.end()) return itf->second;
            }
        }
        return "any";
    }
    if (auto ix = dynamic_cast<IndexExpr*>(e)) {
        return "any";
    }
    if (auto l = dynamic_cast<ListExpr*>(e)) return "list";
    if (auto d = dynamic_cast<DictExpr*>(e)) return "dict";
    return "any";

}