#include "bendcore/document/entity_id.hpp"
#include <random>
#include <sstream>
#include <iomanip>
#include <cctype>
#include <algorithm>

namespace BendCore::CadCore {

// Thread-local RNG for UUID generation
static thread_local std::mt19937_64 g_rng(std::random_device{}());
static thread_local std::uniform_int_distribution<uint32_t> g_dist(0, 0xFFFFFFFFU);

EntityId EntityId::generate() noexcept {
    UuidBytes bytes;

    try {
        // Generate 16 random bytes
        for (int i = 0; i < 16; i += 4) {
            uint32_t rand_val = g_dist(g_rng);
            bytes[i] = (rand_val >> 24) & 0xFF;
            bytes[i + 1] = (rand_val >> 16) & 0xFF;
            bytes[i + 2] = (rand_val >> 8) & 0xFF;
            bytes[i + 3] = rand_val & 0xFF;
        }

        // Set UUID version 4 (random) - bits 12-15 of time_hi_and_version
        bytes[6] = (bytes[6] & 0x0F) | 0x40;

        // Set variant to RFC 4122 - bits 6-7 of clock_seq_hi_and_reserved
        bytes[8] = (bytes[8] & 0x3F) | 0x80;

        return EntityId(bytes);
    } catch (...) {
        return EntityId(); // Return null ID on error
    }
}

EntityId EntityId::parse(const std::string& str) {
    if (!isValid(str)) {
        throw std::invalid_argument("Invalid EntityId format: " + str);
    }

    UuidBytes bytes;

    // Remove hyphens and parse hex
    std::string hex_str;
    for (char c : str) {
        if (c != '-') {
            hex_str += c;
        }
    }

    // Convert hex string to bytes
    for (size_t i = 0; i < 16; ++i) {
        bytes[i] = hexToByte(hex_str.substr(i * 2, 2));
    }

    return EntityId(bytes);
}

bool EntityId::isValid(const std::string& str) noexcept {
    return validateUuidFormat(str);
}

EntityId::EntityId() noexcept {
    uuid_.fill(0);
}

EntityId::EntityId(const UuidBytes& bytes) noexcept
    : uuid_(bytes)
{}

std::string EntityId::toString() const {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');

    for (int i = 0; i < 16; ++i) {
        if (i == 4 || i == 6 || i == 8 || i == 10) {
            oss << '-';
        }
        oss << std::setw(2) << static_cast<int>(uuid_[i]);
    }

    return oss.str();
}

bool EntityId::isNull() const noexcept {
    return std::all_of(uuid_.begin(), uuid_.end(),
                       [](uint8_t b) { return b == 0; });
}

bool EntityId::operator==(const EntityId& other) const noexcept {
    return uuid_ == other.uuid_;
}

bool EntityId::operator!=(const EntityId& other) const noexcept {
    return !(*this == other);
}

bool EntityId::operator<(const EntityId& other) const noexcept {
    return uuid_ < other.uuid_;
}

bool EntityId::validateUuidFormat(const std::string& str) noexcept {
    // RFC 4122 format: xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx
    // Length must be 36 (32 hex + 4 hyphens)
    if (str.length() != 36) {
        return false;
    }

    // Check hyphens at correct positions
    if (str[8] != '-' || str[13] != '-' || str[18] != '-' || str[23] != '-') {
        return false;
    }

    // Check version (1-5 in position 14)
    char version_char = str[14];
    if (version_char < '1' || version_char > '5') {
        return false;
    }

    // Check variant (8-b in position 19)
    char variant_char = str[19];
    if (variant_char != '8' && variant_char != '9' &&
        variant_char != 'a' && variant_char != 'b' &&
        variant_char != 'A' && variant_char != 'B') {
        return false;
    }

    // Check all other characters are hex digits
    for (size_t i = 0; i < str.length(); ++i) {
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            continue; // Skip hyphens
        }

        char c = str[i];
        if (!std::isxdigit(static_cast<unsigned char>(c))) {
            return false;
        }
    }

    return true;
}

uint8_t EntityId::hexToByte(const std::string& hex) {
    if (hex.length() != 2) {
        throw std::invalid_argument("Invalid hex byte: " + hex);
    }

    unsigned int byte_val;
    std::istringstream iss(hex);
    iss >> std::hex >> byte_val;

    if (iss.fail() || byte_val > 0xFF) {
        throw std::invalid_argument("Invalid hex byte: " + hex);
    }

    return static_cast<uint8_t>(byte_val);
}

} // namespace BendCore::CadCore

namespace std {
size_t hash<BendCore::CadCore::EntityId>::operator()(
    const BendCore::CadCore::EntityId& id) const noexcept {
    const auto& bytes = id.getBytes();

    // Simple hash combining bytes
    size_t hash_val = 0;
    for (size_t i = 0; i < bytes.size(); ++i) {
        hash_val ^= std::hash<uint8_t>()(bytes[i]) + 0x9e3779b9 +
                    (hash_val << 6) + (hash_val >> 2);
    }

    return hash_val;
}
} // namespace std
