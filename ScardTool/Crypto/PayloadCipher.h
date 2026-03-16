/** @file PayloadCipher.h
 * @brief Kart payload şifreleme/çözme katmanı.
 *
 * Deterministik IV türetme: karta IV/tag yazmak gerekmez.
 *
 * IV türetme:
 * @code
 *   master_key = PBKDF2-SHA256(password, salt=UID, iter=100000, len=32)
 *   block_key  = HKDF(master_key, salt="", info="key:" + hex(block_addr), len=16|32)
 *   block_iv   = HKDF(master_key, salt="", info="iv:"  + hex(block_addr), len=16)
 * @endcode
 *
 * Böylece her blok farklı key+IV alır, parola+UID değişirse tümü değişir,
 * karta ek veri yazmak gerekmez.
 *
 * Uyarı: AES-GCM 16-byte tag üretir — çıktı giriş boyutundan büyüktür.
 * Sadece DESFire (değişken dosya boyutu) için kullanın.
 */
#pragma once
#include "CardCrypto.h"
#include "Crypto.h"
#include "Kdf.h"
#include "Hash.h"
#include "Mcp/HexUtils.h"
#include <string>
#include <stdexcept>
#include <sstream>
#include <iomanip>

/// @brief PBKDF2 iterasyon sayısı — yüksek değer brute-force'u zorlaştırır.
/// NIST SP 800-132: minimum 1000, önerilen ≥ 100000 (2023).
static constexpr int PBKDF2_ITERATIONS = 100000;

/** @brief Tek bir blok/payload için şifreleme bağlamı. */
struct CryptoContext {
    CipherAlgo  algo;      ///< Kullanılan algoritma
    BYTEV       key;       ///< Türetilmiş blok anahtarı
    BYTEV       iv;        ///< Türetilmiş blok IV/nonce
};

/** @brief Kart payload şifreleyici/çözücü. */
class PayloadCipher {
public:
    /** @brief Şifreleme bağlamını başlat.
     *
     * @param password Kullanıcı parolası
     * @param uid      Kart UID'si (hex string, örn. "04A1B2C3")
     * @param algo     Kullanılacak algoritma
     */
    PayloadCipher(const std::string& password,
                  const std::string& uid,
                  CipherAlgo         algo)
        : algo_(algo), uid_(uid)
    {
        // Master key türet: PBKDF2-SHA256(password, salt=UID_bytes, iter=100000)
        BYTEV uidBytes;
        try { uidBytes = mcp::fromHex(uid); } catch (...) {}
        if (uidBytes.empty()) {
            // UID yoksa string'i direkt salt yap
            for (char c : uid) uidBytes.push_back(static_cast<BYTE>(c));
        }

        int keyLen = (algo == CipherAlgo::AesGcm) ? 32
                      : (algo == CipherAlgo::TripleDesCbc) ? 24 : 16;
        masterKey_ = crypto::pbkdf2Sha256(password, uidBytes, PBKDF2_ITERATIONS,
                                          static_cast<size_t>(keyLen));
    }

    /** @brief Belirtilen blok adresi için şifreleme bağlamı türet. */
    CryptoContext contextFor(int blockAddr) const {
        CryptoContext ctx;
        ctx.algo = algo_;

        // info string'leri
        std::string addrHex = blockAddrHex(blockAddr);
        std::string keyInfo = "key:" + addrHex;
        std::string ivInfo  = "iv:"  + addrHex;

        int keyLen = (algo_ == CipherAlgo::AesGcm)      ? 32
                   : (algo_ == CipherAlgo::TripleDesCbc) ? 24 : 16;

        // HKDF ile blok bazlı key + iv
        ctx.key = crypto::hkdf(masterKey_, {}, toBytes(keyInfo),
                               static_cast<size_t>(keyLen));
        // IV boyutu algoritmaya göre: AES=16, 3DES=8, CTR/GCM nonce=12
        size_t ivLen = 16;
        if (algo_ == CipherAlgo::TripleDesCbc) ivLen = 8;
        else if (algo_ == CipherAlgo::AesCtr)  ivLen = 16;
        ctx.iv = crypto::hkdf(masterKey_, {}, toBytes(ivInfo), ivLen);

        return ctx;
    }

    /** @brief Payload'ı şifrele.
     *
     * @param data      Ham veri
     * @param blockAddr Kart blok/page adresi (IV türetimi için)
     * @return Şifreli veri (boyut algo'ya göre değişebilir)
     * @throws std::runtime_error Şifreleme başarısız olursa
     */
    BYTEV encrypt(const BYTEV& data, int blockAddr) const {
        auto ctx = contextFor(blockAddr);
        return applyCipher(ctx, data, true);
    }

    /** @brief Şifreli payload'ı çöz.
     *
     * @param data      Şifreli veri
     * @param blockAddr Kart blok/page adresi
     * @return Çözülmüş ham veri
     * @throws std::runtime_error Çözme başarısız (yanlış parola veya bozuk veri)
     */
    BYTEV decrypt(const BYTEV& data, int blockAddr) const {
        auto ctx = contextFor(blockAddr);
        return applyCipher(ctx, data, false);
    }

    /** @brief Şifreleme algoritması. */
    CipherAlgo algo() const { return algo_; }

    /** @brief Algoritma adı (CLI string). */
    std::string algoName() const { return CardCrypto::toString(algo_); }

private:
    CipherAlgo  algo_;
    std::string uid_;
    BYTEV       masterKey_;

    BYTEV applyCipher(const CryptoContext& ctx,
                      const BYTEV& data,
                      bool doEncrypt) const {
        try {
            switch (ctx.algo) {
                case CipherAlgo::AesCtr: {
                    AesCtrCipher cipher(ctx.key, ctx.iv);
                    return doEncrypt ? cipher.encrypt(data.data(), data.size()) : cipher.decrypt(data.data(), data.size());
                }
                case CipherAlgo::AesCbc: {
                    if (data.size() % 16 != 0)
                        throw std::runtime_error(
                            "AES-CBC requires data length to be multiple of 16 bytes. "
                            "Got " + std::to_string(data.size()) + " bytes. "
                            "Use aes-ctr for arbitrary sizes.");
                    AesCbcCipher cipher(ctx.key, ctx.iv);
                    return doEncrypt ? cipher.encrypt(data.data(), data.size()) : cipher.decrypt(data.data(), data.size());
                }
                case CipherAlgo::AesGcm: {
                    AesGcmCipher cipher(ctx.key);
                    // IV'yi AAD olarak kullan (bağlam doğrulama)
                    if (doEncrypt)
                        return cipher.encrypt(data.data(), data.size(),
                                              ctx.iv.data(), ctx.iv.size());
                    else
                        return cipher.decrypt(data.data(), data.size(),
                                              ctx.iv.data(), ctx.iv.size());
                }
                case CipherAlgo::TripleDesCbc: {
                    if (data.size() % 8 != 0)
                        throw std::runtime_error(
                            "3DES-CBC requires data length to be multiple of 8 bytes. "
                            "Got " + std::to_string(data.size()) + " bytes.");
                    TripleDesCbcCipher cipher(ctx.key, ctx.iv);
                    return doEncrypt ? cipher.encrypt(data.data(), data.size()) : cipher.decrypt(data.data(), data.size());
                }
                case CipherAlgo::Xor: {
                    if (ctx.key.size() < 4)
                        throw std::runtime_error("XOR key too short");
                    XorCipher::Key4 k4;
                    std::copy(ctx.key.begin(), ctx.key.begin() + 4, k4.begin());
                    XorCipher cipher(k4);
                    return doEncrypt ? cipher.encrypt(data.data(), data.size()) : cipher.decrypt(data.data(), data.size());
                }
                default:
                    throw std::runtime_error("Unsupported cipher: " +
                                             CardCrypto::toString(ctx.algo));
            }
        } catch (const std::runtime_error&) {
            throw;
        } catch (const std::exception& e) {
            throw std::runtime_error(
                std::string(doEncrypt ? "Encrypt" : "Decrypt") +
                " failed (" + CardCrypto::toString(ctx.algo) + "): " + e.what());
        }
    }

    static std::string blockAddrHex(int addr) {
        std::ostringstream oss;
        oss << std::hex << std::uppercase << std::setfill('0')
            << std::setw(4) << addr;
        return oss.str();
    }

    static BYTEV toBytes(const std::string& s) {
        return BYTEV(s.begin(), s.end());
    }
};

/** @brief XorCipher için 4-byte key extract (Kdf üzerinden). */
inline XorCipher::Key4 deriveXorKey(const BYTEV& masterKey, int blockAddr) {
    std::string info = "xor:" + std::to_string(blockAddr);
    BYTEV kBytes(info.begin(), info.end());
    auto derived = crypto::hkdf(masterKey, {}, kBytes, 4);
    XorCipher::Key4 k4;
    std::copy(derived.begin(), derived.end(), k4.begin());
    return k4;
}
