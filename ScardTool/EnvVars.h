/** @file EnvVars.h
 * @brief Çevre değişkeni tabanlı parametre okuma.
 *
 * Öncelik zinciri:
 *   1. CLI args       (en yüksek)
 *   2. Env variable   SCARDTOOL_*
 *   3. Session file   .scardtool_session
 *   4. Interactive prompt
 *
 * Desteklenen değişkenler:
 * @code
 *   SCARDTOOL_READER    → --reader / -r
 *   SCARDTOOL_PASSWORD  → --password / -P   (güvenli: shell history'de görünmez)
 *   SCARDTOOL_CIPHER    → --cipher
 *   SCARDTOOL_KEY       → --key / -k        (Mifare key hex)
 *   SCARDTOOL_JSON      → --json / -j       (1=aktif)
 *   SCARDTOOL_VERBOSE   → --verbose / -v    (1=aktif)
 *   SCARDTOOL_NO_WARN   → --no-warn / -q    (1=aktif)
 *   SCARDTOOL_ENCRYPT   → --encrypt / -E    (1=aktif)
 *   SCARDTOOL_DRY_RUN   → --dry-run         (1=aktif)
 * @endcode
 *
 * @par Güvenlik notu:
 *   SCARDTOOL_PASSWORD çevre değişkeni komut geçmişinde görünmez.
 *   Ancak `/proc/<pid>/environ` üzerinden okunabilir (Linux).
 *   En güvenli yöntem: session dosyasına şifreli kaydetmek (TODO).
 */
#pragma once
#include <string>
#include <optional>
#include <cstdlib>
#include <algorithm>
#include <cctype>

/** @brief Tüm SCARDTOOL_* çevre değişkeni adları. */
namespace EnvVar {
    inline constexpr const char* READER   = "SCARDTOOL_READER";
    inline constexpr const char* PASSWORD = "SCARDTOOL_PASSWORD";
    inline constexpr const char* CIPHER   = "SCARDTOOL_CIPHER";
    inline constexpr const char* KEY      = "SCARDTOOL_KEY";
    inline constexpr const char* JSON     = "SCARDTOOL_JSON";
    inline constexpr const char* VERBOSE  = "SCARDTOOL_VERBOSE";
    inline constexpr const char* NO_WARN  = "SCARDTOOL_NO_WARN";
    inline constexpr const char* ENCRYPT  = "SCARDTOOL_ENCRYPT";
    inline constexpr const char* DRY_RUN  = "SCARDTOOL_DRY_RUN";
}

/** @brief Çevre değişkeni okuma ve uzun parametre adı → env eşleme. */
class EnvVars {
public:
    /** @brief Çevre değişkenini oku. Tanımlı değilse nullopt. */
    static std::optional<std::string> get(const char* name) {
        const char* v = std::getenv(name);
        if (!v || v[0] == '\0') return std::nullopt;
        return std::string(v);
    }

    /** @brief Bool çevre değişkeni: "1", "true", "yes", "on" → true. */
    static bool getBool(const char* name) {
        auto v = get(name);
        if (!v) return false;
        std::string lv = *v;
        std::transform(lv.begin(), lv.end(), lv.begin(),
            [](unsigned char c){ return std::tolower(c); });
        return lv == "1" || lv == "true" || lv == "yes" || lv == "on";
    }

    /** @brief longName'e göre karşılık gelen env değişkenini oku.
     *
     * Örn: "reader" → SCARDTOOL_READER
     *      "password" → SCARDTOOL_PASSWORD
     */
    static std::optional<std::string> forParam(const std::string& longName) {
        if (longName == "reader")   return get(EnvVar::READER);
        if (longName == "password") return get(EnvVar::PASSWORD);
        if (longName == "cipher")   return get(EnvVar::CIPHER);
        if (longName == "key")      return get(EnvVar::KEY);
        return std::nullopt;
    }

    /** @brief Tüm global flag çevre değişkenlerini ekrana yaz (--verbose ile). */
    static void printActive() {
        auto print = [](const char* name, bool val) {
            if (val) std::cout << "  env: " << name << "=1\n";
        };
        auto printStr = [](const char* name) {
            if (auto v = EnvVars::get(name))
                std::cout << "  env: " << name << "=" << *v << "\n";
        };
        printStr(EnvVar::READER);
        printStr(EnvVar::CIPHER);
        printStr(EnvVar::KEY);
        if (EnvVars::get(EnvVar::PASSWORD))
            std::cout << "  env: " << EnvVar::PASSWORD << "=*** (set)\n";
        print(EnvVar::JSON,    getBool(EnvVar::JSON));
        print(EnvVar::VERBOSE, getBool(EnvVar::VERBOSE));
        print(EnvVar::NO_WARN, getBool(EnvVar::NO_WARN));
        print(EnvVar::ENCRYPT, getBool(EnvVar::ENCRYPT));
        print(EnvVar::DRY_RUN, getBool(EnvVar::DRY_RUN));
    }
};
