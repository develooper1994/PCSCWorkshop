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

#endif // PCSC_WORKSHOP1_PCSCUTILS_H
