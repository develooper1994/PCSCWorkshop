#pragma once
#include "Types.h"
#include <string>
#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <cctype>

namespace mcp {

// ── BYTEV → hex string ("AABBCC") ────────────────────────────────────────────
inline std::string toHex(const BYTEV& bytes) {
    std::ostringstream oss;
    oss << std::uppercase << std::hex << std::setfill('0');
    for (auto b : bytes)
        oss << std::setw(2) << static_cast<int>(b);
    return oss.str();
}

// ── hex string → BYTEV ───────────────────────────────────────────────────────
inline BYTEV fromHex(const std::string& hex) {
    if (hex.size() % 2 != 0)
        throw std::invalid_argument("Hex string must have even length: " + hex);

    BYTEV result;
    result.reserve(hex.size() / 2);

    for (size_t i = 0; i < hex.size(); i += 2) {
        char c0 = hex[i], c1 = hex[i + 1];
        if (!std::isxdigit(static_cast<unsigned char>(c0)) ||
            !std::isxdigit(static_cast<unsigned char>(c1))) {
            throw std::invalid_argument(
                std::string("Invalid hex chars at pos ") + std::to_string(i));
        }
        BYTE byte = static_cast<BYTE>(std::stoi(hex.substr(i, 2), nullptr, 16));
        result.push_back(byte);
    }
    return result;
}

} // namespace mcp
