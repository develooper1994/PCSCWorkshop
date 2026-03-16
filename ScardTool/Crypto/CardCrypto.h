/** @file CardCrypto.h
 * @brief Kart tipi bazlı şifreleme strateji seçimi ve detect-cipher.
 *
 * Kart blok boyutuna göre hangi cipher'ların kullanılabileceğini
 * ve hangisinin önerildiğini belirler.
 *
 * Tasarım kararları:
 * - Trailer blokları (Mifare Classic block 3, 7, 11...) asla şifrelenmez.
 * - IV karta yazılmaz: HKDF(key, "iv:" + block_addr) ile deterministik türetilir.
 * - GCM tag (16 byte) Classic/Ultralight bloklarına sığmaz → desteklenmez.
 * - Ultralight 4-byte page → sadece stream cipher (CTR, XOR) kullanılabilir.
 * - Kullanıcı explicit cipher seçmişse detect-cipher devre dışı kalır.
 */
#pragma once
#include "CryptoTypes.h"
#include "CardDataTypes.h"
#include <string>
#include <vector>
#include <optional>

// ── Cipher algorithm string identifier ──────────────────────────────────────

/** @brief Şifreleme algoritması seçenekleri. */
enum class CipherAlgo {
    None,        ///< Şifreleme yok (varsayılan)
    Detect,      ///< Kart tipine göre otomatik seç
    AesCtr,      ///< AES-128-CTR — stream, çıktı = giriş (Classic/UL için)
    AesCbc,      ///< AES-128-CBC — 16B blok, Classic için uygun
    AesGcm,      ///< AES-256-GCM — authenticated, DESFire için
    TripleDesCbc,///< 3DES-CBC — 8B blok, Classic için
    Xor,         ///< XOR 4-byte — sadece UL obfuscation, güvenlik minimal
};

/** @brief Tek bir cipher seçeneğinin karta uygunluk bilgisi. */
struct CipherOption {
    CipherAlgo  algo;
    std::string name;        ///< CLI adı (örn. "aes-ctr")
    std::string displayName; ///< İnsan okunur (örn. "AES-128-CTR")
    bool        suitable;    ///< Bu kart tipi için uygun mu?
    std::string reason;      ///< Uygun değilse neden
    bool        recommended; ///< Önerilen seçenek mi?
};

/** @brief detect-cipher sonucu. */
struct CipherDetectResult {
    CardType    cardType;
    int         blockSize;           ///< Byte cinsinden blok/page boyutu
    std::string uid;                 ///< Kart UID'si (hex)
    std::string ats;                 ///< ATS (hex, yoksa boş)
    CipherAlgo  recommended;         ///< Önerilen algoritma
    std::vector<CipherOption> options; ///< Tüm seçenekler ve uygunlukları
};

/** @brief Kart tipi → şifreleme strateji seçici. */
class CardCrypto {
public:

    // ── String dönüşümleri ───────────────────────────────────────────────────

    /** @brief CLI string → CipherAlgo. */
    static CipherAlgo fromString(const std::string& s) {
        if (s == "none")      return CipherAlgo::None;
        if (s == "detect")    return CipherAlgo::Detect;
        if (s == "aes-ctr")   return CipherAlgo::AesCtr;
        if (s == "aes-cbc")   return CipherAlgo::AesCbc;
        if (s == "aes-gcm")   return CipherAlgo::AesGcm;
        if (s == "3des-cbc")  return CipherAlgo::TripleDesCbc;
        if (s == "xor")       return CipherAlgo::Xor;
        return CipherAlgo::None; // bilinmeyen
    }

    /** @brief CipherAlgo → CLI string. */
    static std::string toString(CipherAlgo a) {
        switch (a) {
            case CipherAlgo::None:         return "none";
            case CipherAlgo::Detect:       return "detect";
            case CipherAlgo::AesCtr:       return "aes-ctr";
            case CipherAlgo::AesCbc:       return "aes-cbc";
            case CipherAlgo::AesGcm:       return "aes-gcm";
            case CipherAlgo::TripleDesCbc: return "3des-cbc";
            case CipherAlgo::Xor:          return "xor";
            default: return "none";
        }
    }

    /** @brief CipherAlgo → insan okunur isim. */
    static std::string displayName(CipherAlgo a) {
        switch (a) {
            case CipherAlgo::None:         return "None (plaintext)";
            case CipherAlgo::Detect:       return "Auto-detect";
            case CipherAlgo::AesCtr:       return "AES-128-CTR";
            case CipherAlgo::AesCbc:       return "AES-128-CBC";
            case CipherAlgo::AesGcm:       return "AES-256-GCM";
            case CipherAlgo::TripleDesCbc: return "3DES-CBC";
            case CipherAlgo::Xor:          return "XOR-4 (obfuscation only)";
            default: return "Unknown";
        }
    }

    /** @brief Geçerli bir CLI algo string'i mi? */
    static bool isValidString(const std::string& s) {
        return s == "none"    || s == "detect"  || s == "aes-ctr" ||
               s == "aes-cbc" || s == "aes-gcm" || s == "3des-cbc" ||
               s == "xor";
    }

    // ── Kart tipi → uygunluk analizi ────────────────────────────────────────

    /** @brief Kart tipine göre tüm seçenekleri ve önerilen algoritmayı döndürür. */
    static CipherDetectResult analyze(CardType cardType,
                                       int blockSize,
                                       const std::string& uid = "",
                                       const std::string& ats = "") {
        CipherDetectResult r;
        r.cardType  = cardType;
        r.blockSize = blockSize;
        r.uid       = uid;
        r.ats       = ats;

        r.options = buildOptions(cardType, blockSize);

        // Önerilen seçeneği bul
        r.recommended = CipherAlgo::AesCtr; // güvenli varsayılan
        for (const auto& opt : r.options) {
            if (opt.recommended && opt.suitable) {
                r.recommended = opt.algo;
                break;
            }
        }
        return r;
    }

    /** @brief Kart tipi için belirtilen algo uygun mu? */
    static bool isSuitable(CipherAlgo algo, CardType cardType, int blockSize) {
        auto opts = buildOptions(cardType, blockSize);
        for (const auto& o : opts)
            if (o.algo == algo) return o.suitable;
        return false;
    }

    /** @brief Kart bloğu trailer mı? (asla şifrelenmez) */
    static bool isTrailerBlock(int blockAddr, CardType cardType) {
        switch (cardType) {
            case CardType::MifareClassic1K:
            case CardType::MifareClassic4K: {
                // Her 4. blok (3,7,11...) = trailer
                // Classic4K büyük sektörlerde 16 bloktan 1'i trailer
                if (cardType == CardType::MifareClassic1K)
                    return (blockAddr % 4) == 3;
                if (blockAddr < 128)
                    return (blockAddr % 4) == 3;
                return ((blockAddr - 128) % 16) == 15;
            }
            case CardType::MifareUltralight:
                // UL'de trailer konsepti yok ama OTP sayfası (3) write-once
                return blockAddr == 3; // OTP — yazmaktan kaçın
            default:
                return false;
        }
    }

    // ── Tüm geçerli algo adlarını döndür (--help için) ──────────────────────
    static std::vector<std::string> allNames() {
        return {"none","detect","aes-ctr","aes-cbc","aes-gcm","3des-cbc","xor"};
    }

private:
    static std::vector<CipherOption> buildOptions(CardType cardType, int blockSize) {
        std::vector<CipherOption> opts;

        // AES-128-CTR: stream, çıktı = giriş — her zaman çalışır (>=1 byte)
        opts.push_back({
            CipherAlgo::AesCtr, "aes-ctr", "AES-128-CTR",
            true, "",
            // Classic ve Unknown'da önerilen, DESFire hariç
            cardType != CardType::MifareDesfire
        });

        // AES-128-CBC: blok = 16, padding yok iff giriş % 16 == 0
        bool cbcSuitable = (blockSize % 16 == 0) && blockSize >= 16;
        opts.push_back({
            CipherAlgo::AesCbc, "aes-cbc", "AES-128-CBC",
            cbcSuitable,
            cbcSuitable ? "" : "Block size (" + std::to_string(blockSize) +
                               "B) not aligned to AES block (16B)",
            false
        });

        // AES-256-GCM: stream+16B tag, DESFire için ideal
        bool gcmSuitable = (cardType == CardType::MifareDesfire);
        opts.push_back({
            CipherAlgo::AesGcm, "aes-gcm", "AES-256-GCM",
            gcmSuitable,
            gcmSuitable ? "" :
                "GCM adds 16-byte authentication tag — would overflow card block.\n"
                "       Only suitable for DESFire (variable-size files).",
            cardType == CardType::MifareDesfire // DESFire'da önerilen
        });

        // 3DES-CBC: blok = 8B, Classic'te 16B = 2×8 → padding yok
        bool tdesSuitable = (blockSize % 8 == 0) && blockSize >= 8;
        opts.push_back({
            CipherAlgo::TripleDesCbc, "3des-cbc", "3DES-CBC",
            tdesSuitable,
            tdesSuitable ? "" : "Block size (" + std::to_string(blockSize) +
                               "B) not aligned to DES block (8B)",
            false
        });

        // XOR: 4 byte key, stream, her boyutta çalışır — güvenlik minimal
        opts.push_back({
            CipherAlgo::Xor, "xor", "XOR-4",
            true,
            "XOR provides obfuscation only, not cryptographic security.\n"
            "       Use only when stronger ciphers are not applicable.",
            false // hiçbir zaman önerilmez
        });

        return opts;
    }
};
