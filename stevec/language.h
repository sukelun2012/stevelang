/*
 * Copyright (c) 2024 Kekun Su(苏科纶).
 * 
 * Refer to the LICENSE file for full license information.
 * SPDX-License-Identifier: MIT
 */

#ifndef STEVE_LANGUAGE_H
#define STEVE_LANGUAGE_H

#include <string>

namespace steve {

    enum class Language {
        English,
        Chinese
    };

    // 初始化：从当前工作目录读取 "language.txt"（内容包含 "Chinese" 则使用中文）
    void initLanguage();

    // 当前是否为中文模式
    bool isChinese();

    // 获取本地化消息（key->模板），可传入单个参数替换模板中的 {0}
    std::string localize(const std::string& key, const std::string& arg = "");

    // 报错并输出到 std::cerr；fatal=true 时会调用 std::exit(1)
    void reportError(const std::string& key, const std::string& arg = "", bool fatal = true);

} // namespace steve

#endif // STEVE_LANGUAGE_H