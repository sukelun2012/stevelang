/*
 * Copyright (c) 2024 Kekun Su(苏科纶).
 * 
 * Refer to the LICENSE file for full license information.
 * SPDX-License-Identifier: MIT
 */

#include "language.h"
#include <iostream>
#include <fstream>
#include <unordered_map>
#include <string>

namespace language {

static Language currentLanguage = Language::English;

// Read data from "language.txt" in current directory, use Chinese if it contains "Chinese"
void initLanguage() {
    std::ifstream file("language.txt");
    std::string content;
    if (file.is_open()) {
        std::getline(file, content);
        file.close();
    }
    
    if (content.find("Chinese") != std::string::npos) {
        currentLanguage = Language::Chinese;
    } else {
        currentLanguage = Language::English;
    }
}

// Whether current mode is Chinese
bool isChinese() {
    return currentLanguage == Language::Chinese;
}

// Get localized message (key -> template), optionally use {0} to replace placeholders in template
std::string localize(const std::string& key, const std::string& arg) {
    static std::unordered_map<std::string, std::string> localization = {
        {"Usage", "Usage: steve <filename>"},
        {"FileNotFound", "Error: File not found"},
        {"InternalError", "Internal Error: {0}"},
        {"RuntimeError", "Runtime Error: {0}"},
        {"TypeError", "Type Error: {0}"},
        {"true", "true"},
        {"false", "false"}
    };

    std::unordered_map<std::string, std::string> currentLocalization = localization;

    if (currentLanguage == Language::Chinese) {
        currentLocalization = {
            {"Usage", "Usage: steve <filename>"},
            {"FileNotFound", "Error: File not found"},
            {"InternalError", "Internal Error: {0}"},
            {"RuntimeError", "Runtime Error: {0}"},
            {"TypeError", "Type Error: {0}"},
            {"ImportError", "Import Error: {0}"},
            {"SyntaxError", "Syntax Error: {0}"},
            {"UndefinedIdentifier", "Undefined identifier: {0}"},
            {"true", "true"},
            {"false", "false"}
        };
    }

    auto it = currentLocalization.find(key);
    if (it != currentLocalization.end()) {
        std::string result = it->second;
        size_t pos = result.find("{0}");
        if (!arg.empty() && pos != std::string::npos) {
            result.replace(pos, 3, arg);
        }
        return result;
    }
    
    return key; // If not found, return the key itself
}

// Report error to std::cerr, call std::exit(1) when fatal=true
void reportError(const std::string& key, const std::string& arg, bool fatal) {
    std::string errorMsg = localize(key, arg);
    std::cerr << errorMsg << std::endl;
    
    if (fatal) {
        std::exit(1);
    }
}

} // namespace language