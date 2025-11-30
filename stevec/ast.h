/*
 * Copyright (c) 2024 Kekun Su(苏科纶).
 * 
 * Refer to the LICENSE file for full license information.
 * SPDX-License-Identifier: MIT
 */

#ifndef STEVE_AST_H
#define STEVE_AST_H
#include <string>
#include <vector>
#include <memory>

namespace steve {
namespace AST {

// 节点：所有节点的基类
struct Node {
    virtual ~Node() = default;
    int line = 0;
    int column = 0;
    std::vector<std::string> decorators;
};

using NodePtr = std::unique_ptr<Node>;

struct Program : Node {
    std::vector<NodePtr> topLevel;
};

struct Statement : Node { };
struct Expression : Node {
    // Sema 会推断类型，如"int","float","string","bool","any","ClassName","list","dict"等
    std::string inferredType;
};

// 访问修饰符枚举
enum class AccessModifier {
    Default,    // 默认访问级别
    Public,
    Private,
    Protected
};

// Import 声明
struct ImportDecl : Statement {
    bool isFrom = false;
    std::string module;
    std::string name;   // imported name or "*" for all
    std::string alias;  // optional alias
};

// 声明
struct VarDecl : Statement {
    AccessModifier access = AccessModifier::Default;  // 访问修饰符
    std::string typeName; // optional type annotation (e.g. "int")
    std::string name;
    std::unique_ptr<Expression> init; // may be null
};

struct ConstDecl : Statement {
    AccessModifier access = AccessModifier::Default;  // 访问修饰符
    std::string name;
    std::unique_ptr<Expression> init;
};

struct FuncDecl : Statement {
    AccessModifier access = AccessModifier::Default;  // 访问修饰符
    std::string name;
    std::vector<std::pair<std::string,std::string>> params; // (type,name)
    std::unique_ptr<Statement> body; // usually a BlockStmt
    std::string returnType; // optional
};

struct ClassDecl : Statement {
    std::string name;
    std::string base; // extends
    std::unique_ptr<Statement> body; // block containing member declarations
};

// 包声明
struct PackageDecl : Statement {
    std::string packageName;
};

// Try-Catch 语句
struct TryStmt : Statement {
    std::unique_ptr<Statement> tryBlock;
    std::string exceptionVar;  // catch 中的异常变量名
    std::unique_ptr<Statement> catchBlock;  // optional
};

// Break 语句
struct BreakStmt : Statement {};

// Continue 语句
struct ContinueStmt : Statement {};

// Pass 语句
struct PassStmt : Statement {};

// Statements
struct BlockStmt : Statement {
    std::vector<NodePtr> stmts;
};

struct ExprStmt : Statement {
    std::unique_ptr<Expression> expr;
};

struct IfStmt : Statement {
    std::unique_ptr<Expression> cond;
    std::unique_ptr<Statement> thenBranch;
    std::unique_ptr<Statement> elseBranch; // optional
};

struct WhileStmt : Statement {
    std::unique_ptr<Expression> cond;
    std::unique_ptr<Statement> body;
};

struct ForStmt : Statement {
    std::unique_ptr<Statement> init; // var decl or exprstmt
    std::unique_ptr<Expression> cond;
    std::unique_ptr<Expression> step;
    std::unique_ptr<Statement> body;
};

struct ReturnStmt : Statement {
    std::unique_ptr<Expression> value; // optional
};

// Expressions
struct Identifier : Expression {
    std::string name;
};

struct Literal : Expression {
    std::string raw; // original lexeme or interpreted string
};

struct BinaryExpr : Expression {
    std::string op;
    std::unique_ptr<Expression> left;
    std::unique_ptr<Expression> right;
};

struct UnaryExpr : Expression {
    std::string op;
    std::unique_ptr<Expression> operand;
};

struct CallExpr : Expression {
    std::unique_ptr<Expression> callee;
    std::vector<std::unique_ptr<Expression>> args;
};

struct MemberExpr : Expression {
    // obj.field
    std::unique_ptr<Expression> obj;
    std::string member;
};

struct IndexExpr : Expression {
    // obj[index]
    std::unique_ptr<Expression> obj;
    std::unique_ptr<Expression> index;
};

struct ListExpr : Expression {
    std::vector<std::unique_ptr<Expression>> items;
};

struct DictExpr : Expression {
    std::vector<std::pair<std::unique_ptr<Expression>, std::unique_ptr<Expression>>> pairs;
};

// Tuple 表达式 (使用 list() 语法)
struct TupleExpr : Expression {
    std::vector<std::unique_ptr<Expression>> items;
};

// 指针相关表达式
struct PointerExpr : Expression {
    std::string pointerType;  // ptr<T>, ref<T>, weak<T>, array_ptr<T>
    std::string baseType;     // T from ptr<T>
    std::unique_ptr<Expression> value; // 被指向的值
};

struct DereferenceExpr : Expression {
    std::unique_ptr<Expression> pointer; // 指针表达式
    bool safe = false;  // 是否使用安全解引用 (?.)
};

struct PointerMemberAccess : Expression {
    std::unique_ptr<Expression> pointer; // 指针表达式
    std::string member; // 成员名
    bool safe = false;  // 是否使用安全访问 (?.)
};

} // namespace AST
} // namespace steve

#endif // STEVE_AST_H