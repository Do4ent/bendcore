#include "step_preflight.hpp"

#include <cctype>
#include <cstdio>

namespace bendcore::kernel::detail {

namespace {

bool isTokenCharacter(unsigned char c) {
    if (std::isalnum(c) != 0) return true;
    switch (c) {
        case '_': case '#': case '$': case '&': case '(': case ')': case '*': case '+': case ',':
        case '-': case '.': case '/': case ';': case '=': case '!': case '"':
        // blanks and the characters step.lex skips: space, tab, CR, LF, NUL
        case ' ': case '\t': case '\r': case '\n': case '\0':
            return true;
        default:
            return false;
    }
}

bool startsWithNoCase(std::string_view data, std::size_t at, std::string_view word) {
    if (data.size() - at < word.size()) return false;
    for (std::size_t k = 0; k < word.size(); ++k) {
        if (std::toupper(static_cast<unsigned char>(data[at + k])) != word[k]) return false;
    }
    return true;
}

std::string describe(unsigned char c) {
    char buf[32];
    if (c >= 0x20 && c < 0x7f) {
        std::snprintf(buf, sizeof(buf), "'%c'", c);
    } else {
        std::snprintf(buf, sizeof(buf), "byte 0x%02X", c);
    }
    return buf;
}

}  // namespace

std::optional<std::string> findInvalidStepCharacter(std::string_view data) {
    enum class State { Initial, Comment, Text };
    State state = State::Initial;
    std::size_t line = 1;
    std::size_t lineStart = 0;
    const std::size_t n = data.size();
    for (std::size_t i = 0; i < n; ++i) {
        const unsigned char c = static_cast<unsigned char>(data[i]);
        if (c == '\n') {
            ++line;
            lineStart = i + 1;
            continue;
        }
        switch (state) {
            case State::Comment:
                // <Com>[*]+[/]: end of comment
                if (c == '*' && i + 1 < n && data[i + 1] == '/') {
                    state = State::Initial;
                    ++i;
                }
                break;
            case State::Text:
                // <Text>[']/[" "\n\r]*[\)\,]: the string ends only at an apostrophe followed by
                // blanks and a comma or a closing parenthesis; any other apostrophe is text.
                if (c == '\'') {
                    std::size_t j = i + 1;
                    while (j < n && (data[j] == ' ' || data[j] == '\n' || data[j] == '\r')) ++j;
                    if (j < n && (data[j] == ')' || data[j] == ',')) state = State::Initial;
                }
                break;
            case State::Initial:
                if (c == '/' && i + 1 < n && data[i + 1] == '*') {
                    state = State::Comment;
                    ++i;
                } else if (c == '\'') {
                    state = State::Text;
                } else if (startsWithNoCase(data, i, "END-ISO")) {
                    // (?i:END-ISO)[0-9\-]*; : everything after it is skipped by the lexer.
                    std::size_t j = i + 7;
                    while (j < n && (std::isdigit(static_cast<unsigned char>(data[j])) != 0 || data[j] == '-')) ++j;
                    if (j < n && data[j] == ';') return std::nullopt;
                } else if (!isTokenCharacter(c)) {
                    return "STEP input has an invalid character " + describe(c) + " outside strings at line " +
                           std::to_string(line) + ", column " + std::to_string(i - lineStart + 1) +
                           " (ISO 10303-21 allows only keywords, numbers, #names, .enums., \"binaries\" and "
                           "( ) , ; = $ * / & ! there)";
                }
                break;
        }
    }
    return std::nullopt;
}

std::optional<std::string> preflightStep(std::string_view data, std::size_t maxInputBytes) {
    if (data.size() > maxInputBytes) {
        return "STEP input of " + std::to_string(data.size()) + " bytes exceeds the limit of " +
               std::to_string(maxInputBytes) +
               " bytes (StepReadOptions::maxInputBytes; the reader needs about ten times the file size in memory)";
    }
    return findInvalidStepCharacter(data);
}

}  // namespace bendcore::kernel::detail
