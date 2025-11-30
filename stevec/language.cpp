/*
 * Copyright (c) 2024 Kekun Su(苏科纶).
 * 
 * Refer to the LICENSE file for full license information.
 * SPDX-License-Identifier: MIT
 */

#include "language.h"
#include <fstream>
#include <iostream>
#include <map>
#include <algorithm>
#include <cstdlib>

namespace steve {

    static Language g_lang = Language::English;

    static std::string toLower(std::string s) {
        std::transform(s.begin(), s.end(), s.begin(),
            [](unsigned char c) { return std::tolower(c); });
        return s;
    }

    void initLanguage() {
        std::ifstream in("language.txt");
        if (!in) {
            g_lang = Language::English;
            return;
        }
        std::string content;
        std::string line;
        while (std::getline(in, line)) {
            content += line + "\n";
        }
        std::string low = toLower(content);
        if (low.find("chinese") != std::string::npos || low.find("zh") != std::string::npos) {
            g_lang = Language::Chinese;
        }
        else {
            g_lang = Language::English;
        }
    }

    bool isChinese() {
        return g_lang == Language::Chinese;
    }

    static std::string replaceArg(const std::string& tmpl, const std::string& arg) {
        std::string out;
        size_t pos = 0;
        while (true) {
            size_t p = tmpl.find("{0}", pos);
            if (p == std::string::npos) {
                out += tmpl.substr(pos);
                break;
            }
            out += tmpl.substr(pos, p - pos);
            out += arg;
            pos = p + 3;
        }
        return out;
    }

    std::string localize(const std::string& key, const std::string& arg) {
        static const std::map<std::string, std::pair<std::string, std::string>> messages = {
            {"Usage", {"Usage: stevec filename.steve", "用法: stevec 文件名.steve"}},
            {"FileNotFound", {"File not found: {0}", "文件未找到: {0}"}},
            {"SyntaxError", {"Syntax error: {0}", "语法错误: {0}"}},
            {"UnexpectedToken", {"Unexpected token: {0}", "意外的标记: {0}"}},
            {"UnclosedString", {"Unclosed string literal", "字符串未闭合"}},
            {"UnknownKeyword", {"Unknown keyword: {0}", "未知关键字: {0}"}},
            {"InvalidNumber", {"Invalid numeric literal: {0}", "无效的数值字面量: {0}"}},
            {"InternalError", {"Internal compiler error: {0}", "内部编译器错误: {0}"}},
            {"Info_InitLang", {"Language set to English", "语言已设置为英文"}},
            {"Hint_PleaseCreateLangFile", {"Create language.txt with 'Chinese' to enable Chinese messages", "在language.txt中写入'Chinese'以启用中文消息"}},
            {"TypeError", {"Type error: {0}", "类型错误: {0}"}},
            {"UndefinedIdentifier", {"Undefined identifier: {0}", "未定义的标识符: {0}"}},
            {"ImportError", {"Import error: {0}", "导入错误: {0}"}},
            {"DecoratorError", {"Decorator error: {0}", "装饰器错误: {0}"}},
            {"TryError", {"Try-catch error: {0}", "Try-catch错误: {0}"}},
            {"BreakError", {"Break error: {0}", "Break错误: {0}"}},
            {"ContinueError", {"Continue error: {0}", "Continue错误: {0}"}},
            {"PassError", {"Pass error: {0}", "Pass错误: {0}"}}
        };

        auto it = messages.find(key);
        if (it == messages.end()) {
            // Unknown key, return the key itself
            return key;
        }
        const auto& pair = it->second;
        const std::string& tmpl = (g_lang == Language::Chinese) ? pair.second : pair.first;
        return replaceArg(tmpl, arg);
    }

    void reportError(const std::string& key, const std::string& arg, bool fatal) {
        std::string msg = localize(key, arg);
        std::cerr << msg << std::endl;
        if (fatal) std::exit(1);
    }

} // namespace steve