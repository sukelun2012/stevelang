/*
 * Copyright (c) 2024 Kekun Su(苏科纶).
 * 
 * Refer to the LICENSE file for full license information.
 * SPDX-License-Identifier: MIT
 */

#ifndef STEVE_PARSER_H
#define STEVE_PARSER_H

#include "lexer.h"
#include "ast.h"
#include "language.h"
#include <vector>
#include <memory>
#include <string>

namespace steve {

class Parser {
public:
    explicit Parser(const std::vector<Token>& tokens);
    // parse   ѡָ   fatal=false  Է       ʽ     AST      ģ       
    std::unique_ptr<AST::Program> parse(bool fatal = true);

    // collected errors
    std::vector<std::string> errors;

private:
    const std::vector<Token>& tokens;
    size_t idx;

    const Token& peek() const;
    const Token& previous() const;
    bool isAtEnd() const;
    const Token& advance();
    bool matchOperator(const std::string& op);
    bool matchPunct(const std::string& p);
    bool checkType(TokenType t, const std::string& lexeme = "") const;
    void consumeExpect(TokenType t, const std::string& lexeme, const std::string& errMsg);

    void errorToken(const Token& tok, const std::string& msg);

    // parsing functions
    std::unique_ptr<AST::Statement> parseDeclaration();
    std::unique_ptr<AST::Statement> parseStatement();
    std::unique_ptr<AST::Statement> parseBlock();
    std::unique_ptr<AST::Statement> parseIf();
    std::unique_ptr<AST::Statement> parseWhile();
    std::unique_ptr<AST::Statement> parseDoWhile();
    std::unique_ptr<AST::Statement> parseFor();
    std::unique_ptr<AST::Statement> parseReturn();
    std::unique_ptr<AST::Statement> parseVarOrConst();
    std::unique_ptr<AST::Statement> parseFunc();
    std::unique_ptr<AST::Statement> parseClass();
    std::unique_ptr<AST::Statement> parsePackage();

    std::unique_ptr<AST::Statement> parseTry();
    std::unique_ptr<AST::Statement> parseBreak();
    std::unique_ptr<AST::Statement> parseContinue();
    std::unique_ptr<AST::Statement> parsePass();

    std::unique_ptr<AST::ExprStmt> parseExpressionStatement();
    std::unique_ptr<AST::Expression> parseExpression();
    std::unique_ptr<AST::Expression> parseAssignment();
    std::unique_ptr<AST::Expression> parseOr();
    std::unique_ptr<AST::Expression> parseAnd();
    std::unique_ptr<AST::Expression> parseEquality();
    std::unique_ptr<AST::Expression> parseComparison();
    std::unique_ptr<AST::Expression> parseBitwise();
    std::unique_ptr<AST::Expression> parseShift();
    std::unique_ptr<AST::Expression> parseAddSub();
    std::unique_ptr<AST::Expression> parseMulDivMod();
    std::unique_ptr<AST::Expression> parseUnary();
    std::unique_ptr<AST::Expression> parsePrimary();
};

} // namespace steve

#endif // STEVE_PARSER_H