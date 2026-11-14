#pragma once

// Private helper of bendcore_kernel: checks on STEP text made before OCCT's reader sees it
// (docs/kernel/KERNEL_CORE.md, "Повреждённые файлы"; docs/kernel/OCCT_BUILD.md, раздел 6).
//
// 1. Size limit: the reader needs about ten times the file size in memory
//    (docs/qa/PERFORMANCE.md); a larger input is refused before any copy.
// 2. Lexical check (ISO 10303-21, clause 5): outside strings and comments an exchange
//    structure consists only of keywords, numbers, entity names (#n), enumerations (.X.),
//    binaries ("..."), and the punctuation ( ) , ; = $ * / & !. OCCT's lexer (step.lex, last
//    rule `[^)]`) turns every other character into a separate "misc" parameter instead of an
//    error; a line of 200 000 such characters makes the reader allocate more than 8 GiB.
//    The scanner follows the start conditions of step.lex exactly (comment, text string with
//    OCCT's end rule "' followed by blanks and , or )", the tail after END-ISO-10303-21;), so a
//    file OCCT reads as intended is never refused.

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace bendcore::kernel::detail {

// Error message (with line and column) for the first character that no STEP token outside a
// string or comment can contain; nullopt when there is none.
std::optional<std::string> findInvalidStepCharacter(std::string_view data);

// Size limit and lexical check together; the message is ready for Error::message.
std::optional<std::string> preflightStep(std::string_view data, std::size_t maxInputBytes);

}  // namespace bendcore::kernel::detail
