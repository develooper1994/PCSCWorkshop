#ifndef PCSC_WORKSHOP1_PCSCUTILS_H
#define PCSC_WORKSHOP1_PCSCUTILS_H

#include <iostream>
#include <vector>
#include <string>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <windows.h>
#include <winscard.h>

#pragma comment(lib, "winscard.lib")

#include "../Cipher/CipherTypes.h"

// ============================================================
// Yardimci fonksiyonlar
// ============================================================

inline void printHex(const BYTE* data, DWORD len) {
    for (DWORD i = 0; i < len; ++i)
        std::cout << std::uppercase << std::hex
                  << std::setw(2) << std::setfill('0')
                  << static_cast<int>(data[i]) << " ";
    std::cout << std::dec << std::endl;
}

inline void printHex(const BYTEV& data) {
    if (data.empty()) { std::cout << "<empty>\n"; return; }
    printHex(data.data(), static_cast<DWORD>(data.size()));
}

inline void printHex(const std::string& data) {
    if (data.empty()) { std::cout << "<empty>\n"; return; }
    printHex(reinterpret_cast<const BYTE*>(data.data()), static_cast<DWORD>(data.size()));
}

inline std::string toHex(const BYTE* data, size_t len)
{
    if (!data || len == 0)
        return "<empty>";

    static constexpr char lut[] = "0123456789ABCDEF";

    std::string result;
    result.resize(len * 3 - 1);

    size_t j = 0;

    for (size_t i = 0; i < len; ++i)
    {
        BYTE c = data[i];
        result[j++] = lut[c >> 4];
        result[j++] = lut[c & 0x0F];

        if (i + 1 < len)
            result[j++] = ' ';
    }

    return result;
}
inline std::string toHex(const BYTEV& v) { return toHex(v.data(), v.size()); }
inline std::string toHex(const std::string& s) { return toHex(reinterpret_cast<const BYTE*>(s.data()),s.size()); }

#endif // PCSC_WORKSHOP1_PCSCUTILS_H
