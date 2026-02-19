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

using BYTEV = std::vector<BYTE>;

// ============================================================
// Yardýmcý fonksiyonlar
// ============================================================

inline void printHex(const BYTE* data, DWORD len) {
    for (DWORD i = 0; i < len; ++i)
        std::cout << std::uppercase << std::hex
                  << std::setw(2) << std::setfill('0')
                  << static_cast<int>(data[i]) << " ";
    std::cout << std::dec << std::endl;
}

// ============================================================
// Reader yönetimi
// ============================================================

struct ReaderListResult {
    LPWSTR rawBuffer;                // SCardFreeMemory ile serbest býrakýlacak
    std::vector<std::wstring> names; // okuyucu adlarý
    bool ok;                         // baþarýlý mý?
};

inline ReaderListResult listReaders(SCARDCONTEXT hContext) {
    ReaderListResult result = { nullptr, {}, false };

    DWORD readersLen = SCARD_AUTOALLOCATE;
    // When using SCARD_AUTOALLOCATE the API expects a LPWSTR cast of the address of
    // the LPWSTR variable (this is the documented usage pattern).
    LONG rc = SCardListReadersW(hContext, nullptr,
                               reinterpret_cast<LPWSTR>(&result.rawBuffer),
                               &readersLen);
    if (rc != SCARD_S_SUCCESS) {
        std::cerr << "SCardListReaders failed: 0x"
                  << std::hex << rc << std::dec << std::endl;
        return result;
    }

    wchar_t* p = result.rawBuffer;
    while (p && *p != L'\0') {
        result.names.emplace_back(p);
        std::wcout << L"[" << result.names.size() - 1
                   << L"] " << p << std::endl;
        p += wcslen(p) + 1;
    }

    if (result.names.empty()) {
        std::cerr << "No readers found\n";
        SCardFreeMemory(hContext, result.rawBuffer);
        result.rawBuffer = nullptr;
        return result;
    }

    result.ok = true;
    return result;
}

inline size_t selectReader(const std::vector<std::wstring>& readerList,
                           size_t defaultIndex = 1) {
    if (readerList.empty()) return 0;
    if (defaultIndex >= readerList.size()) defaultIndex = 0;

    while (true) {
        std::wcout << L"Select reader index (0-"
                   << (readerList.size() - 1)
                   << L", default " << defaultIndex << L"): ";
        std::wstring input;
        std::getline(std::wcin, input);
        if (input.empty()) return defaultIndex;
        try {
            size_t selected = std::stoul(input);
            if (selected < readerList.size()) return selected;
            std::wcout << L"Invalid index. Try again.\n";
        }
        catch (const std::exception&) {
            std::wcout << L"Invalid input. Try again.\n";
        }
    }
}

#endif // PCSC_WORKSHOP1_PCSCUTILS_H
