#pragma once
#include "CardConnection.h"

// ============================================================
// DESFire komutlarý ve parse
// ============================================================

namespace DESFire {

// --- Yardýmcý eþleme fonksiyonlarý ---

inline const char* vendorName(BYTE v) {
    switch (v) {
    case 0x04: return "NXP";
    default:   return "Unknown";
    }
}

inline std::string storageToText(BYTE s) {
    switch (s) {
    case 0x18: return "4096 bytes (4K)";
    case 0x10: return "1024 bytes (1K)";
    case 0x1A: return "8192 bytes (8K)";
    case 0x16: return "2048 bytes (2K)";
    case 0x0E: return "512 bytes";
    case 0x0C: return "256 bytes";
    default: {
        std::stringstream ss;
        ss << "Unknown (0x" << std::hex << static_cast<int>(s) << ")";
        return ss.str();
    }
    }
}

inline const char* protocolName(BYTE p) {
    switch (p) {
    case 0x05: return "ISO/IEC 14443-4";
    default:   return "Unknown";
    }
}

// --- Komutlar ---

inline BYTEV getVersion(const CardConnection& card) {
    BYTEV cmd = { 0x90, 0x60, 0x00, 0x00, 0x00 };
    return card.sendCommand(cmd);
}

inline BYTEV getUid(const CardConnection& card) {
    BYTEV cmd = { 0xFF, 0xCA, 0x00, 0x00, 0x00 };
    return card.sendCommand(cmd, false);
}

// --- Parse ve yazdýrma ---

inline void printFrame(const BYTEV& data, size_t& pos, const char* title) {
    if (data.size() < pos + 7) return;

    std::cout << "\n=== " << title << " ===\n";
    BYTE vendor  = data[pos++];
    BYTE type    = data[pos++];
    BYTE subtype = data[pos++];
    BYTE major   = data[pos++];
    BYTE minor   = data[pos++];
    BYTE storage = data[pos++];
    BYTE proto   = data[pos++];

    std::cout << "Vendor:   " << vendorName(vendor)
              << " (0x" << std::hex << static_cast<int>(vendor) << std::dec << ")\n";
    std::cout << "Type:     0x" << std::hex << static_cast<int>(type) << std::dec << "\n";
    std::cout << "Subtype:  0x" << std::hex << static_cast<int>(subtype) << std::dec << "\n";
    std::cout << "Version:  " << static_cast<int>(major) << "." << static_cast<int>(minor) << "\n";
    std::cout << "Storage:  " << storageToText(storage) << "\n";
    std::cout << "Protocol: " << protocolName(proto)
              << " (0x" << std::hex << static_cast<int>(proto) << std::dec << ")\n";
}

inline void parseAndPrintVersion(const BYTEV& data) {
    if (data.size() < 7) {
        std::cout << "Version data too short: ";
        printHex(data.data(), static_cast<DWORD>(data.size()));
        return;
    }

    size_t pos = 0;

    // Frame 1 — Hardware
    printFrame(data, pos, "Hardware");

    // Frame 2 — Software
    printFrame(data, pos, "Software");

    // Frame 3 — UID / Production
    if (pos < data.size()) {
        std::cout << "\n=== UID / Production ===\n";
        BYTEV rest(data.begin() + pos, data.end());
        std::cout << "Raw: ";
        printHex(rest.data(), static_cast<DWORD>(rest.size()));

        if (rest.size() >= 7) {
            std::cout << "UID: ";
            printHex(rest.data(), 7);
            if (rest.size() > 7) {
                std::cout << "Production / Batch / Date: ";
                printHex(rest.data() + 7, static_cast<DWORD>(rest.size() - 7));
            }
        }
    }
}

// Karta baðlan ? version al ? parse et ? yazdýr
inline void printVersionInfo(const CardConnection& card) {
    try {
        auto data = getVersion(card);
        std::cout << "\nDESFire Version Raw: ";
        printHex(data.data(), static_cast<DWORD>(data.size()));
        parseAndPrintVersion(data);
    }
    catch (const std::exception& ex) {
        std::cerr << "GetVersion failed: " << ex.what() << std::endl;
    }
}

inline void printUid(const CardConnection& card) {
    try {
        auto uid = getUid(card);
        std::cout << "UID: ";
        printHex(uid.data(), static_cast<DWORD>(uid.size()));
    }
    catch (const std::exception&) {
        std::cerr << "Failed to get UID\n";
    }
}

} // namespace DESFire
