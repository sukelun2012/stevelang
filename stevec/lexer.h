/*
 * Copyright (c) 2024 Kekun Su(苏科纶).
 * 
 * Refer to the LICENSE file for full license information.
 * SPDX-License-Identifier: MIT
 */

#ifndef STEVE_LEXER_H
#define STEVE_LEXER_H
#include <string>
#include <vector>

namespace steve {

enum class TokenType {
    // Tokens
    Identifier,
    Keyword,
    Reserved,       // Reserved words, such as goto
    IntegerLiteral,
    FloatLiteral,
    StringLiteral,
    Placeholder,    // s% t% f% x% b% o%
    Decorator,      // @name
    Operator,
    Punctuator,     // ; , ( ) { } [ ] .
    Comment,
    EndOfFile,
    Unknown
};

struct Token {
    TokenType type;
    std::string lexeme;   // Original text
    std::string literal;  // Optional interpreted content, such as string values
    int line;
    int column;
};

class Lexer {
public:
    explicit Lexer(const std::string& source);
    std::vector<Token> tokenize();

private:
    const std::string src;
    size_t i;
    int line;
    int column;

    bool isAtEnd() const;
    char peek() const;
    char peekNext() const;
    char advance();
    bool match(char expected);

    void addToken(std::vector<Token>& out, TokenType type, const std::string& lexeme, const std::string& literal = "");
    void skipWhitespaceAndComments();
    void scanToken(std::vector<Token>& out);
    void scanString(std::vector<Token>& out);
    void scanNumber(std::vector<Token>& out);
    void scanIdentifierOrKeyword(std::vector<Token>& out);
    static bool isAlpha(char c);
    static bool isAlphaNumeric(char c);
};

} // namespace steve

#endif // STEVE_LEXER_H