/** @file ApduMacro.h
 * @brief APDU makro tanımı ve parametreli resolver.
 *
 * Makrolar isimli APDU şablonlarıdır. Parametreler `{paramN}` veya
 * `{isim}` placeholder'ları ile tanımlanır.
 *
 * Sözdizimi:
 * @code
 *   MAKRO_ADI                → sabit APDU
 *   MAKRO_ADI:p1             → tek parametreli
 *   MAKRO_ADI:p1:p2          → çift parametreli
 *   MAKRO_ADI:p1:p2:...      → N parametreli
 * @endcode
 *
 * @par Örnek:
 * @code
 *   ApduMacro m{"READ_BINARY", "FF B0 00 {page} {len}",
 *               {"page","len"}, "Read page/block", "PCSC"};
 *   auto hex = m.resolve({"04","10"});  // "FFB0000410"
 * @endcode
 */
#pragma once
#include <string>
#include <vector>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <cctype>
#include <iomanip>

/** @brief Tek bir APDU makro tanımı. */
struct ApduMacro {
    std::string              name;        ///< Makro adı (örn. "READ_BINARY")
    std::string              apduTemplate;///< APDU şablonu (örn. "FF B0 00 {page} {len}")
    std::vector<std::string> params;      ///< Parametre isimleri sırayla
    std::string              description; ///< Kısa açıklama
    std::string              group;       ///< Grup: "General","PCSC","DESFire","User"
    std::string              notes;       ///< Ek notlar / uyarılar
    std::string              example;     ///< Kullanım örneği
    std::string              standard;    ///< Referans standart
    std::vector<std::string> seealso;     ///< İlgili makro adları

    /** @brief Parametreleri şablona yerleştirerek hex APDU döndürür.
     *
     * @param args Sıralı parametre değerleri (hex veya decimal)
     * @return Boşluksuz büyük harf hex APDU
     * @throws std::invalid_argument Parametre sayısı uyuşmuyorsa
     */
    std::string resolve(const std::vector<std::string>& args) const {
        if (args.size() != params.size()) {
            throw std::invalid_argument(
                "Macro '" + name + "' expects " + std::to_string(params.size()) +
                " parameter(s), got " + std::to_string(args.size()) +
                ". Usage: " + name +
                (params.empty() ? "" : ":" + join(params, ":")));
        }

        std::string result = apduTemplate;

        for (size_t i = 0; i < params.size(); ++i) {
            std::string placeholder = "{" + params[i] + "}";
            std::string value = resolveParam(args[i], params[i]);
            auto pos = result.find(placeholder);
            while (pos != std::string::npos) {
                result.replace(pos, placeholder.size(), value);
                pos = result.find(placeholder, pos + value.size());
            }
        }

        // {len} ve {dlen3} placeholder'larını otomatik hesapla
        result = resolveAutoLen(result);

        // Boşlukları kaldır, uppercase yap
        return stripAndUpper(result);
    }

    /** @brief Parametresiz resolve (sabit APDU). */
    std::string resolveFixed() const {
        if (!params.empty())
            throw std::invalid_argument("Macro '" + name + "' requires parameters.");
        return stripAndUpper(apduTemplate);
    }

    /** @brief Parametre sayısı. */
    size_t paramCount() const { return params.size(); }

    /** @brief Usage string (örn. "READ_BINARY:<page>:<len>"). */
    std::string usage() const {
        if (params.empty()) return name;
        std::string s = name;
        for (const auto& p : params) s += ":<" + p + ">";
        return s;
    }

    /** @brief APDU şablonunun insan-okunur açıklaması (boşluklu). */
    std::string templateFormatted() const {
        std::string t = apduTemplate;
        // Çoklu boşlukları tek boşluğa indir
        bool prevSpace = false;
        std::string out;
        for (char c : t) {
            if (c == ' ') {
                if (!prevSpace) out += ' ';
                prevSpace = true;
            } else {
                out += static_cast<char>(std::toupper(c));
                prevSpace = false;
            }
        }
        return out;
    }

private:
    // Parametre değerini hex byte string'e dönüştür
    // Öncelik sırası:
    //   "0x04" → hex explicit → "04"
    //   "FFFFFFFFFFFF" → multi-byte hex passthrough
    //   "4", "16", "255" → decimal → "04","10","FF"
    //   "FF","0A" → 1-2 hex char → hex → "FF","0A"
    static std::string resolveParam(const std::string& val,
                                    const std::string& paramName) {
        if (val.empty())
            throw std::invalid_argument("Parameter '" + paramName + "' is empty");

        std::string v = val;

        // 0x prefix → explicit hex
        bool explicitHex = false;
        if (v.size() >= 2 && v[0] == '0' && (v[1] == 'x' || v[1] == 'X')) {
            v = v.substr(2);
            explicitHex = true;
        }

        // Multi-byte hex passthrough (>2 hex chars, even length, no decimal chars)
        if (!explicitHex && v.size() > 2) {
            bool allHex = std::all_of(v.begin(), v.end(),
                [](char c){ return std::isxdigit(static_cast<unsigned char>(c)); });
            if (allHex && v.size() % 2 == 0) {
                // Verify it's really multi-byte (contains A-F → definitely hex)
                bool hasAlpha = std::any_of(v.begin(), v.end(),
                    [](char c){ return std::isalpha(static_cast<unsigned char>(c)); });
                if (hasAlpha) {
                    // Uppercase passthrough
                    std::string upper;
                    for (char c : v) upper += static_cast<char>(std::toupper(c));
                    return upper;
                }
                // All decimal digits, >2 chars → could be decimal number
                // Fall through to decimal parse
            }
        }

        // Explicit hex (0x prefix) or 1-2 hex chars that contain A-F
        if (explicitHex) {
            try {
                int n = std::stoi(v, nullptr, 16);
                if (n < 0 || n > 255)
                    throw std::invalid_argument("Out of byte range: 0x" + v);
                std::ostringstream oss;
                oss << std::hex << std::uppercase << std::setfill('0') << std::setw(2) << n;
                return oss.str();
            } catch (const std::invalid_argument&) {
                throw std::invalid_argument("Invalid hex parameter '" + paramName + "': 0x" + v);
            }
        }

        // 1-2 chars with alpha → hex byte (e.g. "FF", "0A", "CA")
        if (v.size() <= 2) {
            bool hasAlpha = std::any_of(v.begin(), v.end(),
                [](char c){ return std::isalpha(static_cast<unsigned char>(c)); });
            if (hasAlpha) {
                bool allHex = std::all_of(v.begin(), v.end(),
                    [](char c){ return std::isxdigit(static_cast<unsigned char>(c)); });
                if (allHex) {
                    int n = std::stoi(v, nullptr, 16);
                    std::ostringstream oss;
                    oss << std::hex << std::uppercase << std::setfill('0') << std::setw(2) << n;
                    return oss.str();
                }
            }
        }

        // Default: parse as decimal
        try {
            int n = std::stoi(v, nullptr, 10);
            if (n < 0 || n > 255)
                throw std::invalid_argument("Value out of byte range (0-255): " + v);
            std::ostringstream oss;
            oss << std::hex << std::uppercase << std::setfill('0') << std::setw(2) << n;
            return oss.str();
        } catch (const std::invalid_argument&) {
            // Not decimal either
            throw std::invalid_argument("Invalid parameter '" + paramName +
                "': '" + val + "' (expected decimal 0-255 or hex)");
        }
    }

    // {len} placeholder'unu otomatik hesapla (önceki data byte sayısı)
    static std::string resolveAutoLen(const std::string& tmpl) {
        // Şimdilik passthrough — gelişmiş auto-len ileriki versiyonda
        return tmpl;
    }

    static std::string stripAndUpper(const std::string& s) {
        std::string result;
        for (char c : s) {
            if (c != ' ' && c != '\t')
                result += static_cast<char>(std::toupper(c));
        }
        return result;
    }

    static std::string join(const std::vector<std::string>& v,
                            const std::string& sep) {
        std::string r;
        for (size_t i = 0; i < v.size(); ++i) {
            if (i > 0) r += sep;
            r += v[i];
        }
        return r;
    }

    // iomanip için
};

/** @brief Makro string'ini parse eder: "GETUID" veya "READ_BINARY:4:16"
 *
 * @param input Makro adı ve parametreler
 * @param[out] macroName Makro adı
 * @param[out] args Parametre listesi
 */
inline void parseMacroCall(const std::string& input,
                            std::string& macroName,
                            std::vector<std::string>& args) {
    macroName.clear();
    args.clear();

    // ':' ile ayır
    size_t pos = 0;
    std::vector<std::string> parts;
    while (pos <= input.size()) {
        size_t next = input.find(':', pos);
        if (next == std::string::npos) next = input.size();
        parts.push_back(input.substr(pos, next - pos));
        pos = next + 1;
    }

    if (parts.empty()) return;
    macroName = parts[0];
    for (size_t i = 1; i < parts.size(); ++i)
        args.push_back(parts[i]);
}

/** @brief Verilen string'in geçerli hex APDU olup olmadığını kontrol eder. */
inline bool isHexApdu(const std::string& s) {
    if (s.empty() || s.size() % 2 != 0) return false;
    for (char c : s) {
        if (c == ' ') continue;
        if (!std::isxdigit(static_cast<unsigned char>(c))) return false;
    }
    return true;
}
