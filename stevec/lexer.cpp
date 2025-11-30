/*
 * Copyright (c) 2024 Kekun Su(苏科纶).
 * 
 * Refer to the LICENSE file for full license information.
 * SPDX-License-Identifier: MIT
 */

#include "lexer.h"
#include <cctype>
#include <iostream>
#include <unordered_set>

namespace steve {

    // Keywords
    static const std::unordered_set<std::string> keywords = {
        "import","from","as","class","func","var","const","if","else","elif","do","while","then","for",
        "true","false","null","print","input","int","string","float","bool","double","long","short","byte",
        "break","continue","package","return","and","or","not","hash","bs","pass","del","append","list","try","catch",
        "open","close","extends","steve"
    };

    static const std::unordered_set<std::string> reserved = {
        "goto"
    };

    Lexer::Lexer(const std::string& source)
        : src(source), i(0), line(1), column(1) {
    }

    bool Lexer::isAtEnd() const {
        return i >= src.size();
    }

    char Lexer::peek() const {
        if (isAtEnd()) return '\0';
        return src[i];
    }

    char Lexer::peekNext() const {
        if (i + 1 >= src.size()) return '\0';
        return src[i + 1];
    }

    char Lexer::advance() {
        char c = peek();
        ++i;
        if (c == '\n') {
            ++line;
            column = 1;
        }
        else {
            ++column;
        }
        return c;
    }

    bool Lexer::match(char expected) {
        if (isAtEnd()) return false;
        if (src[i] != expected) return false;
        ++i;
        ++column;
        return true;
    }

    void Lexer::addToken(std::vector<Token>& out, TokenType type, const std::string& lexeme, const std::string& literal) {
        Token t;
        t.type = type;
        t.lexeme = lexeme;
        t.literal = literal;
        t.line = line;
        t.column = column - static_cast<int>(lexeme.size());
        out.push_back(std::move(t));
    }

    void Lexer::skipWhitespaceAndComments() {
        while (!isAtEnd()) {
            char c = peek();
            // whitespace
            if (c == ' ' || c == '\r' || c == '\t' || c == '\n') {
                advance();
                continue;
            }
            // Single-line comment starting with "//"
            if (c == '/' && peekNext() == '/') {
                // consume '//'
                advance(); advance();
                while (!isAtEnd() && peek() != '\n') advance();
                continue;
            }
            // document comment /** ... */
            if (c == '/' && peekNext() == '*' && src.length() > i + 2 && src[i + 2] == '*') {
                // consume /**
                advance(); advance(); advance();
                while (!isAtEnd()) {
                    if (peek() == '*' && peekNext() == '/') {
                        advance(); advance();
                        break;
                    }
                    advance();
                }
                continue;
            }
            // block comment /* ... */
            if (c == '/' && peekNext() == '*') {
                // consume /*
                advance(); advance();
                while (!isAtEnd()) {
                    if (peek() == '*' && peekNext() == '/') {
                        advance(); advance();
                        break;
                    }
                    advance();
                }
                continue;
            }
            break;
        }
    }

    bool Lexer::isAlpha(char c) {
        return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
    }
    bool Lexer::isAlphaNumeric(char c) {
        return isAlpha(c) || std::isdigit(static_cast<unsigned char>(c));
    }

    void Lexer::scanString(std::vector<Token>& out) {
        size_t start_i = i;
        std::string value;
        // consume opening "
        advance();
        while (!isAtEnd()) {
            char c = peek();
            if (c == '"') {
                // closing
                advance();
                std::string lexeme = src.substr(start_i - 1, i - (start_i - 1));
                addToken(out, TokenType::StringLiteral, lexeme, value);
                return;
            }
            if (c == '\\') {
                // escape
                advance(); // consume '\\'
                char esc = peek();
                if (esc == 'n') { value.push_back('\n'); advance(); }
                else if (esc == 't') { value.push_back('\t'); advance(); }
                else if (esc == '\\') { value.push_back('\\'); advance(); }
                else if (esc == '"') { value.push_back('"'); advance(); }
                else if (esc == 'r') { value.push_back('\r'); advance(); }
                else {
                    value.push_back(esc);
                    if (!isAtEnd()) advance();
                }
                continue;
            }
            value.push_back(c);
            advance();
        }

        // Unclosed string
        std::string lexeme = src.substr(start_i - 1, i - (start_i - 1));
        addToken(out, TokenType::Unknown, lexeme, "Unclosed string literal");
    }

    void Lexer::scanNumber(std::vector<Token>& out) {
        size_t start = i;
        bool isFloat = false;

        // integer part
        char c = peek();
        while (std::isdigit(static_cast<unsigned char>(c))) {
            advance();
            c = peek();
        }

        // fractional part
        char n = peek();
        if (c == '.' && std::isdigit(static_cast<unsigned char>(n))) {
            isFloat = true;
            advance(); // consume '.'
            c = peek();
            while (std::isdigit(static_cast<unsigned char>(c))) {
                advance();
                c = peek();
            }
        }

        std::string lexeme = src.substr(start, i - start);
        addToken(out, isFloat ? TokenType::FloatLiteral : TokenType::IntegerLiteral, lexeme, lexeme);
    }

    void Lexer::scanIdentifierOrKeyword(std::vector<Token>& out) {
        size_t start = i;
        while (isAlphaNumeric(peek())) advance();
        std::string lexeme = src.substr(start, i - start);

        // Placeholder format consists of letters and '%'
        if (lexeme.size() == 1 && peek() == '%') {
            advance();
            lexeme.push_back('%');
            addToken(out, TokenType::Placeholder, lexeme, lexeme);
            return;
        }

        if (reserved.find(lexeme) != reserved.end()) {
            addToken(out, TokenType::Reserved, lexeme);
            return;
        }
        if (keywords.find(lexeme) != keywords.end()) {
            addToken(out, TokenType::Keyword, lexeme);
            return;
        }
        addToken(out, TokenType::Identifier, lexeme);
    }

    void Lexer::scanToken(std::vector<Token>& out) {
        if (isAtEnd()) return;
        char c = peek();

        size_t start = i;

        // decorator
        if (c == '@') {
            advance();
            if (isAlpha(peek())) {
                while (isAlphaNumeric(peek())) advance();
                std::string lexeme = src.substr(start, i - start);
                addToken(out, TokenType::Decorator, lexeme);
                return;
            }
            else {
                addToken(out, TokenType::Operator, "@");
                return;
            }
        }

        // string
        if (c == '"') {
            scanString(out);
            return;
        }

        // number
        if (std::isdigit(static_cast<unsigned char>(c))) {
            scanNumber(out);
            return;
        }

        // identifier/keyword
        if (isAlpha(c)) {
            scanIdentifierOrKeyword(out);
            return;
        }

        // two-char operators

        char n = peekNext();

        std::string two;

        two.push_back(c);

        if (n != '\0') two.push_back(n);



        static const std::unordered_set<std::string> twoOps = {

            "//","**",">>","<<","==","!=",">=","<=","+=","-=","*=","/="

        };



        if (twoOps.find(two) != twoOps.end()) {

            advance(); advance();

            addToken(out, TokenType::Operator, two);

            return;

        }

        // single-char
        switch (c) {
        case '+': advance(); addToken(out, TokenType::Operator, "+"); return;
        case '-': advance(); addToken(out, TokenType::Operator, "-"); return;
        case '*': advance(); addToken(out, TokenType::Operator, "*"); return;
        case '/': advance(); addToken(out, TokenType::Operator, "/"); return;
        case '%': advance(); addToken(out, TokenType::Operator, "%"); return;
        case '=': advance(); addToken(out, TokenType::Operator, "="); return;
        case '>': advance(); addToken(out, TokenType::Operator, ">"); return;
        case '<': advance(); addToken(out, TokenType::Operator, "<"); return;
        case '~': advance(); addToken(out, TokenType::Operator, "~"); return;
        case '&': advance(); addToken(out, TokenType::Operator, "&"); return;
        case '^': advance(); addToken(out, TokenType::Operator, "^"); return;
        case '|': advance(); addToken(out, TokenType::Operator, "|"); return;
        case '!': advance(); addToken(out, TokenType::Operator, "!"); return;
        case ';': advance(); addToken(out, TokenType::Punctuator, ";"); return;
        case ',': advance(); addToken(out, TokenType::Punctuator, ","); return;
        case ':': advance(); addToken(out, TokenType::Punctuator, ":"); return;
        case '.': advance(); addToken(out, TokenType::Punctuator, "."); return;
        case '(': advance(); addToken(out, TokenType::Punctuator, "("); return;
        case ')': advance(); addToken(out, TokenType::Punctuator, ")"); return;
        case '{': advance(); addToken(out, TokenType::Punctuator, "{"); return;
        case '}': advance(); addToken(out, TokenType::Punctuator, "}"); return;
        case '[': advance(); addToken(out, TokenType::Punctuator, "["); return;
        case ']': advance(); addToken(out, TokenType::Punctuator, "]"); return;
        default:
            advance();
            std::string lexeme(1, c);
            addToken(out, TokenType::Unknown, lexeme);
            return;
        }
    }

    std::vector<Token> Lexer::tokenize() {
        std::vector<Token> out;
        while (!isAtEnd()) {
            skipWhitespaceAndComments();
            if (isAtEnd()) break;
            scanToken(out);
        }
        addToken(out, TokenType::EndOfFile, "");
        return out;
    }
}