/** @file ApduCatalog.h
 * @brief APDU katalog sistemi için tek include noktası.
 *
 * Bu header şunları dahil eder:
 *   - StatusWordCatalog (ISO 7816-4 + DESFire SW)
 *   - InsCatalog (ISO/PC/SC/DESFire INS)
 *   - ApduMacro (makro tanımı ve resolver)
 *   - MacroRegistry (built-in + kullanıcı makroları)
 *
 * @par Kullanım:
 * @code
 *   #include "Catalog/ApduCatalog.h"
 *
 *   // SW açıklama
 *   auto sw = StatusWordCatalog::lookup(0x69, 0x82);
 *   std::cout << sw->name;   // "Security status not satisfied"
 *
 *   // INS açıklama
 *   auto ins = InsCatalog::lookup(0xFF, 0xCA);
 *   std::cout << ins->name;  // "GET DATA"
 *
 *   // Makro çöz
 *   std::string hex;
 *   MacroRegistry::instance().resolve("READ_BINARY:4:16", hex);
 *   // hex = "FFB0000410"
 * @endcode
 */
#pragma once
#include "StatusWordCatalog.h"
#include "InsCatalog.h"
#include "ApduMacro.h"
#include "MacroRegistry.h"

/** @brief APDU'yu parse ederek insan-okunur açıklama üretir.
 *
 * @param hexApdu Boşluksuz veya boşluklu hex APDU
 * @return Açıklama string'i
 */
inline std::string describeApdu(const std::string& hexApdu) {
    // Boşlukları kaldır
    std::string h;
    for (char c : hexApdu)
        if (c != ' ') h += static_cast<char>(std::toupper(c));

    if (h.size() < 8) return "Too short to parse (need at least 4 bytes)";

    try {
        uint8_t cla = static_cast<uint8_t>(std::stoi(h.substr(0,2), nullptr, 16));
        uint8_t ins = static_cast<uint8_t>(std::stoi(h.substr(2,2), nullptr, 16));
        uint8_t p1  = static_cast<uint8_t>(std::stoi(h.substr(4,2), nullptr, 16));
        uint8_t p2  = static_cast<uint8_t>(std::stoi(h.substr(6,2), nullptr, 16));

        auto insEntry = InsCatalog::lookup(cla, ins);
        std::string insName = insEntry ? insEntry->name : "Unknown INS";

        auto claStr = [](uint8_t c) -> std::string {
            if (c == 0xFF) return "FF (PC/SC)";
            if (c == 0x90) return "90 (DESFire native)";
            if ((c & 0xF0) == 0x00) return std::string("0") + std::to_string(c & 0x0F) + " (ISO 7816-4)";
            return std::string("?");
        };

        std::ostringstream oss;
        oss << "CLA=" << std::hex << std::uppercase << std::setw(2) << std::setfill('0')
            << (int)cla << "  INS=" << std::setw(2) << (int)ins
            << " (" << insName << ")  "
            << "P1=" << std::setw(2) << (int)p1
            << "  P2=" << std::setw(2) << (int)p2;

        if (h.size() > 8) {
            uint8_t lc_or_le = static_cast<uint8_t>(std::stoi(h.substr(8,2), nullptr, 16));
            if (h.size() == 10)
                oss << "  Le=" << std::setw(2) << (int)lc_or_le;
            else
                oss << "  Lc=" << std::setw(2) << (int)lc_or_le;
        }

        return oss.str();
    } catch (...) {
        return "Parse error";
    }
}

/** @brief SW bytes'ı açıklar (verbose çıktısı için). */
inline std::string describeSw(uint8_t sw1, uint8_t sw2) {
    std::ostringstream oss;
    oss << std::hex << std::uppercase << std::setfill('0')
        << std::setw(2) << (int)sw1 << std::setw(2) << (int)sw2;
    std::string code = oss.str();

    auto e = StatusWordCatalog::lookup(sw1, sw2);
    if (!e) return code + "  (unknown status word)";

    std::string desc = StatusWordCatalog::shortDesc(sw1, sw2);
    return code + "  (" + desc + ")";
}

#include <sstream>
#include <iomanip>
