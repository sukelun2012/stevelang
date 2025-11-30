/*
 * Copyright (c) 2024 Kekun Su(苏科纶).
 * 
 * Refer to the LICENSE file for full license information.
 * SPDX-License-Identifier: MIT
 */

#ifndef STEVE_LANGUAGE_H
#define STEVE_LANGUAGE_H
#include <string>

namespace language {
    // Language enum
    enum class Language {
        English,
        Chinese
    };

    // Initialize language system
    void initLanguage();

    // Check if current mode is Chinese
    bool isChinese();

    // Get localized message
    std::string localize(const std::string& key, const std::string& arg = "");

    // Report error
    void reportError(const std::string& key, const std::string& arg = "", bool fatal = true);

} // namespace language

#endif // STEVE_LANGUAGE_H