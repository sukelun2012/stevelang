/*
 * Copyright (c) 2024 Kekun Su(苏科纶).
 * 
 * Refer to the LICENSE file for full license information.
 * SPDX-License-Identifier: MIT
 */

#include "parser.h"
#include "language.h"
#include <sstream>
#include <iostream>

using namespace steve;
using namespace steve::AST;

Parser::Parser(const std::vector<Token> &tokens)
    : tokens(tokens), idx(0) {}

const Token &Parser::peek() const {
  return tokens[idx];
}
const Token &Parser::previous() const {
  return tokens[idx > 0 ? idx - 1 : 0];
}
bool Parser::isAtEnd() const {
  return idx >= tokens.size() || tokens[idx].type == TokenType::EndOfFile;
}
const Token &Parser::advance() {
  if (!isAtEnd())
    ++idx;
  return previous();
}

// 识别可能的 token 为关键字形式，如and/or/not等
bool Parser::matchOperator(const std::string &op) {
  if (isAtEnd())
    return false;
  const Token &t = peek();
  if ((t.type == TokenType::Operator || t.type == TokenType::Keyword) &&
      t.lexeme == op) {
    advance();
    return true;
  }
  return false;
}
bool Parser::matchPunct(const std::string &p) {
  if (isAtEnd())
    return false;
  const Token &t = peek();
  if (t.type == TokenType::Punctuator && t.lexeme == p) {
    advance();
    return true;
  }
  return false;
}
bool Parser::checkType(TokenType t, const std::string &lexeme) const {
  if (isAtEnd())
    return false;
  const Token &cur = peek();
  if (cur.type != t)
    return false;
  if (!lexeme.empty() && cur.lexeme != lexeme)
    return false;
  return true;
}

void Parser::consumeExpect(TokenType t, const std::string &lexeme,
                           const std::string &errMsg) {
  if (checkType(t, lexeme)) {
    advance();
    return;
  }
  std::ostringstream ss;
  ss << errMsg << " at " << peek().line << ":" << peek().column;
  errorToken(peek(), ss.str());
  if (!isAtEnd())
    advance();
}

void Parser::errorToken(const Token &tok, const std::string &msg) {
  std::ostringstream s;
  s << tok.line << ":" << tok.column << " - " << msg;
  errors.push_back(steve::localize("SyntaxError", s.str()));
}

// Top-level parse 将声明包装成语句添加到程序中
std::unique_ptr<Program> Parser::parse(bool fatal) {
  auto prog = std::make_unique<Program>();
  while (!isAtEnd()) {
    std::vector<std::string> decorators;
    while (checkType(TokenType::Decorator)) {
      decorators.push_back(peek().lexeme);
      advance();
    }
    auto decl = parseDeclaration();
    if (!decl) {
      if (!isAtEnd())
        advance();
      continue;
    }
    if (!decorators.empty())
      decl->decorators = decorators;
    prog->topLevel.push_back(std::move(decl));
  }
  if (!errors.empty()) {
    std::ostringstream ss;
    for (auto &e : errors)
      ss << e << "\n";
    if (fatal)
      steve::reportError("SyntaxError", ss.str(), true);
  }
  return prog;
}

// Declarations (导入 import)

std::unique_ptr<AST::Statement> Parser::parseDeclaration() {

  // Check for access modifiers first (public, private, protected)

  AST::AccessModifier access = AST::AccessModifier::Default;

  if (checkType(TokenType::Keyword, "public") || checkType(TokenType::Keyword, "private") || checkType(TokenType::Keyword, "protected")) {

    std::string modifier = peek().lexeme;

    advance(); // consume the modifier

    if (modifier == "public") access = AST::AccessModifier::Public;

    else if (modifier == "private") access = AST::AccessModifier::Private;

    else if (modifier == "protected") access = AST::AccessModifier::Protected;

  }



  if (checkType(TokenType::Keyword, "package")) {

    return parsePackage();

  }



  if (checkType(TokenType::Keyword, "import") || checkType(TokenType::Keyword, "from")) {

    bool isFrom = (peek().lexeme == "from");

    advance();

    auto id = std::make_unique<ImportDecl>();

    id->isFrom = isFrom;

    if (isFrom) {

      if (checkType(TokenType::Identifier)) {

        id->module = peek().lexeme;

        id->line = peek().line;

        id->column = peek().column;

        advance();

      } else {

        errorToken(peek(), "Expected module identifier after 'from'");

        return nullptr;

      }

      consumeExpect(TokenType::Keyword, "import", "Expected 'import' after from <module>");

      if (checkType(TokenType::Identifier) || (peek().type == TokenType::Operator && peek().lexeme == "*")) {

        id->name = peek().lexeme;

        advance();

      } else

        errorToken(peek(), "Expected name or '*' in from-import");

      if (checkType(TokenType::Keyword, "as")) {

        advance();

        if (checkType(TokenType::Identifier)) {

          id->alias = peek().lexeme;

          advance();

        } else

          errorToken(peek(), "Expected alias after as");

      }

      consumeExpect(TokenType::Punctuator, ";", "Expected ';' after import");

      return id;

    } else {

      if (checkType(TokenType::Identifier)) {

        id->module = peek().lexeme;

        id->line = peek().line;

        id->column = peek().column;

        advance();

      } else {

        errorToken(peek(), "Expected module identifier after 'import'");

        return nullptr;

      }

      if (checkType(TokenType::Keyword, "as")) {

        advance();

        if (checkType(TokenType::Identifier)) {

          id->alias = peek().lexeme;

          advance();

        } else

          errorToken(peek(), "Expected alias after as");

      }

      consumeExpect(TokenType::Punctuator, ";", "Expected ';' after import");

      return id;

    }

  }



  if (checkType(TokenType::Keyword, "var") || checkType(TokenType::Keyword, "const")) {

    auto decl = parseVarOrConst();

    if (decl) {

      if (auto varDecl = dynamic_cast<VarDecl*>(decl.get())) {

        varDecl->access = access;

      }

    }

    return decl;

  }



  if (checkType(TokenType::Keyword, "func")) {

    auto decl = parseFunc();

    if (decl) {

      if (auto funcDecl = dynamic_cast<FuncDecl*>(decl.get())) {

        funcDecl->access = access;

      }

    }

    return decl;

  }



  if (checkType(TokenType::Keyword, "class")) {

    return parseClass();

  }



  return parseStatement();

}

std::unique_ptr<AST::Statement> Parser::parseVarOrConst() {
  const Token kw = peek();
  bool isConst = (kw.lexeme == "const");
  advance();  // consume var/const

  std::string typeName;
  // Check for pointer types like ptr<T>, ref<T>, etc.
  if (checkType(TokenType::Keyword) && (
      peek().lexeme == "int" || peek().lexeme == "string" || peek().lexeme == "float" ||
      peek().lexeme == "bool" || peek().lexeme == "double" || peek().lexeme == "long" ||
      peek().lexeme == "short" || peek().lexeme == "byte")) {
    typeName = peek().lexeme;
    advance();
  } else if (checkType(TokenType::Identifier) && (peek().lexeme == "ptr" || peek().lexeme == "ref" || peek().lexeme == "weak" || peek().lexeme == "array_ptr")) {
    // Handle pointer types: ptr<T>, ref<T>, weak<T>, array_ptr<T>
    std::string ptrType = peek().lexeme;
    advance(); // consume ptr/ref/weak/array_ptr

    if (checkType(TokenType::Punctuator, "<")) {
      advance(); // consume '<'
      if (checkType(TokenType::Keyword) || checkType(TokenType::Identifier)) {
        std::string baseType = peek().lexeme;
        advance(); // consume base type

        if (checkType(TokenType::Punctuator, ">")) {
          advance(); // consume '>'
          typeName = ptrType + "<" + baseType + ">"; // Store as single type string
        } else {
          errorToken(peek(), "Expected '>' in pointer type declaration");
          return nullptr;
        }
      } else {
        errorToken(peek(), "Expected type in pointer type declaration");
        return nullptr;
      }
    } else {
      errorToken(peek(), "Expected '<' in pointer type declaration");
      return nullptr;
    }
  }

  if (!checkType(TokenType::Identifier)) {
    errorToken(peek(), "Expected identifier in declaration");
    return nullptr;
  }
  Token nameTok = peek();
  std::string name = nameTok.lexeme;
  advance();

  auto decl = std::make_unique<VarDecl>();
  decl->typeName = typeName;
  decl->name = name;
  decl->line = nameTok.line; decl->column = nameTok.column;

  if (matchOperator("=")) {
    decl->init = parseExpression();
    consumeExpect(TokenType::Punctuator, ";", "Expected ';' after declaration");
    return decl;
  } else {
    consumeExpect(TokenType::Punctuator, ";", "Expected ';' after declaration");
    return decl;
  }
}

std::unique_ptr<AST::Statement> Parser::parseFunc() {
  advance();  // consume func
  if (!checkType(TokenType::Identifier)) { errorToken(peek(), "Expected function name"); return nullptr; }
  Token nameTok = peek();
  std::string name = nameTok.lexeme; advance();

  // optional return type: func name(args) -> type { ... }  (暂不支持 "-> type")
  consumeExpect(TokenType::Punctuator, "(", "Expected '(' after function name");
  std::vector<std::pair<std::string,std::string>> params;
  if (!checkType(TokenType::Punctuator, ")")) {
    while (true) {
      std::string ptype;
      if (checkType(TokenType::Keyword)) { ptype = peek().lexeme; advance(); }
      if (!checkType(TokenType::Identifier)) { errorToken(peek(), "Expected parameter name"); return nullptr; }
      std::string pname = peek().lexeme; advance();
      params.emplace_back(ptype, pname);
      if (matchPunct(")")) break;
      consumeExpect(TokenType::Punctuator, ",", "Expected ',' between parameters");
    }
  } else {
    advance();
  }

  std::string retType;
  if (checkType(TokenType::Operator, "->")) {
    advance();
    if (checkType(TokenType::Keyword) || checkType(TokenType::Identifier)) {
      retType = peek().lexeme; advance();
    } else { errorToken(peek(), "Expected return type after '->'"); }
  }

  auto body = parseBlock();
  auto f = std::make_unique<FuncDecl>();
  f->name = name;
  f->params = std::move(params);
  f->body = std::move(body);
  f->returnType = retType;
  f->line = nameTok.line; f->column = nameTok.column;
  return f;
}

std::unique_ptr<AST::Statement> Parser::parseClass() {

  advance(); // consume 'class'

  if (!checkType(TokenType::Identifier)) { errorToken(peek(), "Expected class name"); return nullptr; }

  Token nameTok = peek();

  std::string name = nameTok.lexeme; advance();

  std::string base;

  if (checkType(TokenType::Keyword, "extends")) {

    advance();

    if (checkType(TokenType::Identifier)) { base = peek().lexeme; advance(); }

    else errorToken(peek(), "Expected base class identifier after extends");

  }

  auto body = parseBlock();

  auto cd = std::make_unique<ClassDecl>();

  cd->name = name;

  cd->base = base;

  cd->body = std::move(body);

  cd->line = nameTok.line; cd->column = nameTok.column;

  return cd;

}



std::unique_ptr<AST::Statement> Parser::parsePackage() {

  advance(); // consume 'package'

  if (!checkType(TokenType::Identifier)) { errorToken(peek(), "Expected package name"); return nullptr; }

  Token nameTok = peek();

  std::string packageName = nameTok.lexeme; advance();

  consumeExpect(TokenType::Punctuator, ";", "Expected ';' after package declaration");

  auto pd = std::make_unique<PackageDecl>();

  pd->packageName = packageName;

  pd->line = nameTok.line; pd->column = nameTok.column;

  return pd;

}

// Statements
std::unique_ptr<AST::Statement> Parser::parseStatement() {



  if (checkType(TokenType::Punctuator, "{")) return parseBlock();



  if (checkType(TokenType::Keyword, "if")) return parseIf();



  if (checkType(TokenType::Keyword, "do")) return parseDoWhile();



  if (checkType(TokenType::Keyword, "while")) return parseWhile();



  if (checkType(TokenType::Keyword, "for")) return parseFor();



  if (checkType(TokenType::Keyword, "return")) return parseReturn();



  if (checkType(TokenType::Keyword, "try")) return parseTry();



  if (checkType(TokenType::Keyword, "break")) return parseBreak();



  if (checkType(TokenType::Keyword, "continue")) return parseContinue();



  if (checkType(TokenType::Keyword, "pass")) return parsePass();



  return parseExpressionStatement();



}

std::unique_ptr<AST::Statement> Parser::parseBlock() {
  consumeExpect(TokenType::Punctuator, "{", "Expected '{' to start block");
  auto block = std::make_unique<BlockStmt>();
  while (!checkType(TokenType::Punctuator, "}") && !isAtEnd()) {
    auto decl = parseDeclaration();
    if (decl) block->stmts.push_back(std::move(decl));
    else if (!isAtEnd()) advance();
  }
  consumeExpect(TokenType::Punctuator, "}", "Expected '}' after block");
  return block;
}

std::unique_ptr<AST::Statement> Parser::parseIf() {
  advance(); // consume 'if'
  consumeExpect(TokenType::Punctuator, "(", "Expected '(' after if");
  auto cond = parseExpression();
  consumeExpect(TokenType::Punctuator, ")", "Expected ')' after if condition");
  
  // Check for 'then' keyword
  if (checkType(TokenType::Keyword, "then")) {
    advance();
    consumeExpect(TokenType::Punctuator, "{", "Expected '{' after then");
  }
  
  auto thenBranch = parseBlock();
  std::unique_ptr<Statement> elseBranch = nullptr;
  
  // Check for elif
  if (checkType(TokenType::Keyword, "elif")) {
    elseBranch = parseIf(); // Recursive call to handle elif as nested if-else
  }
  // Check for else
  else if (checkType(TokenType::Keyword, "else")) {
    advance();
    
    // Check for 'then' keyword after else
    if (checkType(TokenType::Keyword, "then")) {
      advance();
      consumeExpect(TokenType::Punctuator, "{", "Expected '{' after then");
    }
    
    elseBranch = parseBlock();
  }
  
  auto ifs = std::make_unique<IfStmt>();
  ifs->cond = std::move(cond);
  ifs->thenBranch = std::move(thenBranch);
  ifs->elseBranch = std::move(elseBranch);
  return ifs;
}

std::unique_ptr<AST::Statement> Parser::parseWhile() {

  advance(); // consume while

  consumeExpect(TokenType::Punctuator, "(", "Expected '(' after while");

  auto cond = parseExpression();

  consumeExpect(TokenType::Punctuator, ")", "Expected ')' after while condition");

  auto body = parseBlock();

  auto ws = std::make_unique<WhileStmt>();

  ws->cond = std::move(cond);

  ws->body = std::move(body);

  return ws;

}



std::unique_ptr<AST::Statement> Parser::parseDoWhile() {

  advance(); // consume 'do'

  

  // Check for 'then' keyword after do

  if (checkType(TokenType::Keyword, "then")) {

    advance();

    consumeExpect(TokenType::Punctuator, "{", "Expected '{' after then");

  }

  

  auto body = parseBlock();

  

  // Expect 'while' after the do block

  consumeExpect(TokenType::Keyword, "while", "Expected 'while' after 'do' block");

  consumeExpect(TokenType::Punctuator, "(", "Expected '(' after while");

  auto cond = parseExpression();

  consumeExpect(TokenType::Punctuator, ")", "Expected ')' after while condition");

  

  auto ws = std::make_unique<WhileStmt>();

  ws->cond = std::move(cond);

  ws->body = std::move(body);

  return ws;

}

std::unique_ptr<AST::Statement> Parser::parseFor() {

  advance(); // consume for

  

  // Check if it's a range-based for loop: for range(次数)

  if (checkType(TokenType::Identifier) && peek().lexeme == "range") {

    // Parse range(次数)

    advance(); // consume 'range'

    consumeExpect(TokenType::Punctuator, "(", "Expected '(' after range");

    auto countExpr = parseExpression();

    consumeExpect(TokenType::Punctuator, ")", "Expected ')' after range argument");

    

    // Check for 'then' keyword

    if (checkType(TokenType::Keyword, "then")) {

      advance();

      consumeExpect(TokenType::Punctuator, "{", "Expected '{' after then");

    }

    

    auto body = parseBlock();

    

    // Create a for loop with range. We'll create a counter variable and loop

    auto fs = std::make_unique<ForStmt>();

    fs->init = nullptr; // Will be handled differently for range loop

    fs->cond = std::move(countExpr); // Use count as condition for now

    fs->step = nullptr;

    fs->body = std::move(body);

    return fs;

  } else {

    // Traditional C-style for loop: for(init; condition; increment)

    consumeExpect(TokenType::Punctuator, "(", "Expected '(' after for");

    std::unique_ptr<Statement> init = nullptr;

    if (!checkType(TokenType::Punctuator, ";")) init = parseDeclaration();

    else advance();

    std::unique_ptr<Expression> cond = nullptr;

    if (!checkType(TokenType::Punctuator, ";")) cond = parseExpression();

    consumeExpect(TokenType::Punctuator, ";", "Expected ';' in for");

    std::unique_ptr<Expression> step = nullptr;

    if (!checkType(TokenType::Punctuator, ")")) step = parseExpression();

    consumeExpect(TokenType::Punctuator, ")", "Expected ')' after for");

    auto body = parseBlock();

    auto fs = std::make_unique<ForStmt>();

    fs->init = std::move(init);

    fs->cond = std::move(cond);

    fs->step = std::move(step);

    fs->body = std::move(body);

    return fs;

  }

}

std::unique_ptr<AST::Statement> Parser::parseReturn() {

  advance(); // consume return

  std::unique_ptr<Expression> val = nullptr;

  if (!checkType(TokenType::Punctuator, ";")) val = parseExpression();

  consumeExpect(TokenType::Punctuator, ";", "Expected ';' after return");

  auto rs = std::make_unique<ReturnStmt>();

  rs->value = std::move(val);

  return rs;

}



std::unique_ptr<AST::Statement> Parser::parseTry() {

  advance(); // consume 'try'

  consumeExpect(TokenType::Punctuator, "{", "Expected '{' after try");

  auto tryBlock = parseBlock();

  consumeExpect(TokenType::Keyword, "catch", "Expected 'catch' after try block");

  consumeExpect(TokenType::Punctuator, "(", "Expected '(' in catch clause");

  if (!checkType(TokenType::Identifier)) { errorToken(peek(), "Expected exception variable name in catch"); return nullptr; }

  std::string exceptionVar = peek().lexeme;

  advance(); // consume exception variable name

  consumeExpect(TokenType::Punctuator, ")", "Expected ')' in catch clause");

  consumeExpect(TokenType::Punctuator, "{", "Expected '{' after catch");

  auto catchBlock = parseBlock();

  auto ts = std::make_unique<TryStmt>();

  ts->tryBlock = std::move(tryBlock);

  ts->exceptionVar = exceptionVar;

  ts->catchBlock = std::move(catchBlock);

  return ts;

}



std::unique_ptr<AST::Statement> Parser::parseBreak() {

  advance(); // consume 'break'

  consumeExpect(TokenType::Punctuator, ";", "Expected ';' after break");

  auto bs = std::make_unique<BreakStmt>();

  return bs;

}



std::unique_ptr<AST::Statement> Parser::parseContinue() {

  advance(); // consume 'continue'

  consumeExpect(TokenType::Punctuator, ";", "Expected ';' after continue");

  auto cs = std::make_unique<ContinueStmt>();

  return cs;

}



std::unique_ptr<AST::Statement> Parser::parsePass() {

  advance(); // consume 'pass'

  consumeExpect(TokenType::Punctuator, ";", "Expected ';' after pass");

  auto ps = std::make_unique<PassStmt>();

  return ps;

}

std::unique_ptr<AST::ExprStmt> Parser::parseExpressionStatement() {
  auto stmt = std::make_unique<ExprStmt>();
  stmt->expr = parseExpression();
  consumeExpect(TokenType::Punctuator, ";", "Expected ';' after expression");
  return stmt;
}

// Expressions and postfix handling
std::unique_ptr<AST::Expression> Parser::parseExpression() {
  return parseAssignment();
}

std::unique_ptr<AST::Expression> Parser::parseAssignment() {
  auto left = parseOr();
  if (matchOperator("=")) {
    auto value = parseAssignment();
    auto bin = std::make_unique<BinaryExpr>();
    bin->op = "=";
    bin->left = std::move(left);
    bin->right = std::move(value);
    if (bin->left) { bin->line = bin->left->line; bin->column = bin->left->column; }
    return bin;
  }
  return left;
}

std::unique_ptr<AST::Expression> Parser::parseOr() {
  auto expr = parseAnd();
  while (matchOperator("or")) {
    auto right = parseAnd();
    auto bin = std::make_unique<BinaryExpr>();
    bin->op = "or";
    bin->left = std::move(expr);
    bin->right = std::move(right);
    if (bin->left) { bin->line = bin->left->line; bin->column = bin->left->column; }
    expr = std::move(bin);
  }
  return expr;
}

std::unique_ptr<AST::Expression> Parser::parseAnd() {
  auto expr = parseEquality();
  while (matchOperator("and")) {
    auto right = parseEquality();
    auto bin = std::make_unique<BinaryExpr>();
    bin->op = "and";
    bin->left = std::move(expr);
    bin->right = std::move(right);
    if (bin->left) { bin->line = bin->left->line; bin->column = bin->left->column; }
    expr = std::move(bin);
  }
  return expr;
}

std::unique_ptr<AST::Expression> Parser::parseEquality() {
  auto expr = parseComparison();
  while (true) {
    if (matchOperator("==") || matchOperator("!=")) {
      std::string op = previous().lexeme;
      auto right = parseComparison();
      auto bin = std::make_unique<BinaryExpr>();
      bin->op = op;
      bin->left = std::move(expr);
      bin->right = std::move(right);
      if (bin->left) { bin->line = bin->left->line; bin->column = bin->left->column; }
      expr = std::move(bin);
      continue;
    }
    break;
  }
  return expr;
}

std::unique_ptr<AST::Expression> Parser::parseComparison() {
  auto expr = parseBitwise();
  while (true) {
    if (matchOperator(">") || matchOperator("<") || matchOperator(">=") || matchOperator("<=")) {
      std::string op = previous().lexeme;
      auto right = parseBitwise();
      auto bin = std::make_unique<BinaryExpr>();
      bin->op = op;
      bin->left = std::move(expr);
      bin->right = std::move(right);
      if (bin->left) { bin->line = bin->left->line; bin->column = bin->left->column; }
      expr = std::move(bin);
      continue;
    }
    break;
  }
  return expr;
}

std::unique_ptr<AST::Expression> Parser::parseBitwise() {
  auto expr = parseShift();
  while (true) {
    if (matchOperator("&") || matchOperator("|") || matchOperator("^")) {
      std::string op = previous().lexeme;
      auto right = parseShift();
      auto bin = std::make_unique<BinaryExpr>();
      bin->op = op;
      bin->left = std::move(expr);
      bin->right = std::move(right);
      if (bin->left) { bin->line = bin->left->line; bin->column = bin->left->column; }
      expr = std::move(bin);
      continue;
    }
    break;
  }
  return expr;
}

std::unique_ptr<AST::Expression> Parser::parseShift() {
  auto expr = parseAddSub();
  while (true) {
    if (matchOperator("<<") || matchOperator(">>")) {
      std::string op = previous().lexeme;
      auto right = parseAddSub();
      auto bin = std::make_unique<BinaryExpr>();
      bin->op = op;
      bin->left = std::move(expr);
      bin->right = std::move(right);
      if (bin->left) { bin->line = bin->left->line; bin->column = bin->left->column; }
      expr = std::move(bin);
      continue;
    }
    break;
  }
  return expr;
}

std::unique_ptr<AST::Expression> Parser::parseAddSub() {
  auto expr = parseMulDivMod();
  while (true) {
    if (matchOperator("+") || matchOperator("-")) {
      std::string op = previous().lexeme;
      auto right = parseMulDivMod();
      auto bin = std::make_unique<BinaryExpr>();
      bin->op = op;
      bin->left = std::move(expr);
      bin->right = std::move(right);
      if (bin->left) { bin->line = bin->left->line; bin->column = bin->left->column; }
      expr = std::move(bin);
      continue;
    }
    break;
  }
  return expr;
}

std::unique_ptr<AST::Expression> Parser::parseMulDivMod() {
  auto expr = parseUnary();
  while (true) {
    if (matchOperator("*") || matchOperator("/") || matchOperator("//") || matchOperator("%") || matchOperator("**")) {
      std::string op = previous().lexeme;
      auto right = parseUnary();
      auto bin = std::make_unique<BinaryExpr>();
      bin->op = op;
      bin->left = std::move(expr);
      bin->right = std::move(right);
      if (bin->left) { bin->line = bin->left->line; bin->column = bin->left->column; }
      expr = std::move(bin);
      continue;
    }
    break;
  }
  return expr;
}

std::unique_ptr<AST::Expression> Parser::parseUnary() {
  if (matchOperator("~") || matchOperator("not") || matchOperator("-") || matchOperator("!")) {
    std::string op = previous().lexeme;
    auto right = parseUnary();
    auto u = std::make_unique<UnaryExpr>();
    u->op = op;
    u->operand = std::move(right);
    u->line = u->operand ? u->operand->line : previous().line;
    u->column = previous().column;
    return u;
  }
  return parsePrimary();
}

// parsePrimary + postfix loop (calls, member, index)
std::unique_ptr<AST::Expression> Parser::parsePrimary() {
  if (checkType(TokenType::IntegerLiteral) || checkType(TokenType::FloatLiteral)) {
    Token t = peek();
    auto lit = std::make_unique<Literal>();
    lit->raw = t.lexeme;
    lit->line = t.line; lit->column = t.column;
    advance();
    return lit;
  }
  if (checkType(TokenType::StringLiteral)) {
    Token t = peek();
    auto lit = std::make_unique<Literal>();
    lit->raw = t.literal; // interpreted string
    lit->line = t.line; lit->column = t.column;
    advance();
    return lit;
  }
  if (checkType(TokenType::Keyword, "true") || checkType(TokenType::Keyword, "false") || checkType(TokenType::Keyword, "null")) {
    Token t = peek();
    auto lit = std::make_unique<Literal>();
    lit->raw = t.lexeme;
    lit->line = t.line; lit->column = t.column;
    advance();
    return lit;
  }
  if (checkType(TokenType::Punctuator, "(")) {
    advance();
    auto expr = parseExpression();
    consumeExpect(TokenType::Punctuator, ")", "Expected ')' after expression");
    return expr;
  }

  // identifier -> identifier / call / member / index chain
  if (checkType(TokenType::Identifier)) {
    Token firstTok = peek();
    auto node = std::make_unique<Identifier>();
    node->name = firstTok.lexeme;
    node->line = firstTok.line; node->column = firstTok.column;
    advance();

    // 后缀循环，支持 (.id) ([expr]) ( (args) )
    std::unique_ptr<Expression> expr = std::move(node);
    while (true) {
      if (checkType(TokenType::Punctuator, "(")) {
        // call
        advance(); // consume '('
        auto call = std::make_unique<CallExpr>();
        call->callee = std::move(expr);
        call->line = previous().line; call->column = previous().column;
        if (!checkType(TokenType::Punctuator, ")")) {
          while (true) {
            call->args.push_back(parseExpression());
            if (matchPunct(")")) break;
            consumeExpect(TokenType::Punctuator, ",", "Expected ',' between arguments");
          }
        } else {
          advance(); // consume ')'
        }
        expr = std::move(call);
        continue;
      }
      if (checkType(TokenType::Punctuator, ".")) {
        advance();
        if (!checkType(TokenType::Identifier)) {
          errorToken(peek(), "Expected identifier after '.'");
          return expr;
        }
        Token memTok = peek();
        std::string member = memTok.lexeme;
        advance();
        auto me = std::make_unique<MemberExpr>();
        me->obj = std::move(expr);
        me->member = member;
        me->line = me->obj->line; me->column = me->obj->column;
        expr = std::move(me);
        continue;
      }
      // Handle pointer member access: ->
      if (checkType(TokenType::Operator, "->")) {
        advance(); // consume '->'
        if (!checkType(TokenType::Identifier)) {
          errorToken(peek(), "Expected identifier after '->'");
          return expr;
        }
        Token memTok = peek();
        std::string member = memTok.lexeme;
        advance();
        auto pme = std::make_unique<PointerMemberAccess>();
        pme->pointer = std::move(expr);
        pme->member = member;
        pme->line = pme->pointer->line; pme->column = pme->pointer->column;
        expr = std::move(pme);
        continue;
      }
      if (checkType(TokenType::Punctuator, "[")) {

        advance();

        auto idxExpr = parseExpression();

        consumeExpect(TokenType::Punctuator, "]", "Expected ']' after index");

        auto ie = std::make_unique<IndexExpr>();

        ie->obj = std::move(expr);

        ie->index = std::move(idxExpr);

        ie->line = ie->obj->line; ie->column = ie->obj->column;

        expr = std::move(ie);

        continue;

      }

      // Dictionary access using {}: a{"key"}

      if (checkType(TokenType::Punctuator, "{")) {

        advance();

        auto idxExpr = parseExpression();

        consumeExpect(TokenType::Punctuator, "}", "Expected '}' after dictionary key");

        auto ie = std::make_unique<IndexExpr>();

        ie->obj = std::move(expr);

        ie->index = std::move(idxExpr);

        ie->line = ie->obj->line; ie->column = ie->obj->column;

        expr = std::move(ie);

        continue;

      }

      break;

    }

    return expr;

  }

  // list/dict literals (keyword 'list')
  if (checkType(TokenType::Keyword, "list")) {
    Token t = peek();
    advance();
    if (checkType(TokenType::Punctuator, "[")) {
      advance();
      auto list = std::make_unique<ListExpr>();
      list->line = t.line; list->column = t.column;
      if (!checkType(TokenType::Punctuator, "]")) {
        while (true) {
          list->items.push_back(parseExpression());
          if (matchPunct("]")) break;
          consumeExpect(TokenType::Punctuator, ",", "Expected ',' in list");
        }
      } else advance();
      return list;
    }
    if (checkType(TokenType::Punctuator, "(")) {
      advance();
      auto list = std::make_unique<ListExpr>();
      list->line = t.line; list->column = t.column;
      if (!checkType(TokenType::Punctuator, ")")) {
        while (true) {
          list->items.push_back(parseExpression());
          if (matchPunct(")")) break;
          consumeExpect(TokenType::Punctuator, ",", "Expected ',' in tuple");
        }
      } else advance();
      return list;
    }
    if (checkType(TokenType::Punctuator, "{")) {
      advance();
      auto dict = std::make_unique<DictExpr>();
      dict->line = t.line; dict->column = t.column;
      if (!checkType(TokenType::Punctuator, "}")) {
        while (true) {
          auto k = parseExpression();
          consumeExpect(TokenType::Punctuator, ":", "Expected ':' in dict");
          auto v = parseExpression();
          dict->pairs.emplace_back(std::move(k), std::move(v));
          if (matchPunct("}")) break;
          consumeExpect(TokenType::Punctuator, ",", "Expected ',' in dict");
        }
      } else advance();
      return dict;
    }
  }

  // placeholder like s%
  if (checkType(TokenType::Placeholder)) {
    Token t = peek();
    auto lit = std::make_unique<Literal>();
    lit->raw = t.lexeme;
    lit->line = t.line; lit->column = t.column;
    advance();
    return lit;
  }

  errorToken(peek(), "Unexpected token in expression: " + peek().lexeme);
  advance();
  auto fallback = std::make_unique<Literal>();
  fallback->raw = "";
  return fallback;
}