// SHA-256 (FIPS 180-4) строки: шестнадцатеричная строка из 64 символов.
#pragma once

#include <string>

namespace bendcore {

std::string sha256_hex(const std::string& input);

}  // namespace bendcore
