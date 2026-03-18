// include/cipher.h
// ─────────────────────────────────────────────────────────────────────────────
// Cipher (şifreleme) API başlığı.
//
// FEATURE_CIPHER_ENABLED == 0 ise:
//   • Fonksiyonlar derlenir, ancak çağrıldıklarında anlamlı çalışma zamanı
//     hatası (exception veya error kodu) fırlatırlar.
//   • Kullanım noktasında COMPILE_TIME_CIPHER_CHECK() makrosu ise
//     derleme zamanında static_assert ile hata üretir.
// ─────────────────────────────────────────────────────────────────────────────
#ifndef SCARD_TOOL_CIPHER_H
#define SCARD_TOOL_CIPHER_H

#include "build_info.h"   // FEATURE_CIPHER_ENABLED, REQUIRE_FEATURE

#include <string>
#include <stdexcept>

// ─────────────────────────────────────────────────────────────────────────────
// Derleme zamanı guard — cipher API'sini çağıran TÜM noktalar bu makroyu
// dosyanın BAŞINA ekleyebilir. Kapalı ise derleme hata verir, aç anlatır.
// ─────────────────────────────────────────────────────────────────────────────
#define COMPILE_TIME_CIPHER_CHECK() \
    REQUIRE_FEATURE(FEATURE_CIPHER_ENABLED, "cipher_encrypt / cipher_decrypt")

// ─────────────────────────────────────────────────────────────────────────────
// Çalışma zamanı exception — cipher kapalıyken ÇAĞRILIRSA
// ─────────────────────────────────────────────────────────────────────────────
class CipherDisabledException : public std::logic_error {
public:
    CipherDisabledException()
        : std::logic_error(
            "\n"
            "  ╔══════════════════════════════════════════════════════════════╗\n"
            "  ║  HATA: Cipher desteği bu derlemede DEVRE DIŞI bırakılmıştır.║\n"
            "  ║                                                              ║\n"
            "  ║  Bu özelliği kullanmak için aşağıdaki adımları izleyin:      ║\n"
            "  ║    1. CMake yapılandırmasını şu seçenekle yeniden çalıştırın:║\n"
            "  ║         -DENABLE_CIPHER=ON                                   ║\n"
            "  ║    2. Projeyi yeniden derleyin.                              ║\n"
            "  ║                                                              ║\n"
            "  ║  Visual Studio: Project Properties → CMake Settings →        ║\n"
            "  ║    CMake Variables → ENABLE_CIPHER = TRUE                    ║\n"
            "  ╚══════════════════════════════════════════════════════════════╝\n"
        )
    {}
};

// ─────────────────────────────────────────────────────────────────────────────
// İç yardımcı makro
// ─────────────────────────────────────────────────────────────────────────────
#if !FEATURE_CIPHER_ENABLED
#  define _CIPHER_GUARD() throw CipherDisabledException()
#else
#  define _CIPHER_GUARD() ((void)0)
#endif

// ─────────────────────────────────────────────────────────────────────────────
// API — her iki derleme modunda da tanımlıdır; kapalıdaysa exception fırlatır
// ─────────────────────────────────────────────────────────────────────────────
namespace cipher {

/// @brief Metni verilen anahtarla şifreler.
/// @throws CipherDisabledException  Eğer ENABLE_CIPHER=OFF ile derlendiyse
inline std::string encrypt(const std::string& plaintext, const std::string& key)
{
    _CIPHER_GUARD();
#if FEATURE_CIPHER_ENABLED
    // ── Gerçek implementasyon buraya ────────────────────────────────────────
    // Örnek: XOR basit cipher (gerçek projeye göre değiştirin)
    std::string result = plaintext;
    for (std::size_t i = 0; i < result.size(); ++i)
        result[i] ^= key[i % key.size()];
    return result;
#else
    return {};  // Erişilmez ama derleyiciyi mutlu eder
#endif
}

/// @brief Şifreli metni çözer.
/// @throws CipherDisabledException  Eğer ENABLE_CIPHER=OFF ile derlendiyse
inline std::string decrypt(const std::string& ciphertext, const std::string& key)
{
    _CIPHER_GUARD();
#if FEATURE_CIPHER_ENABLED
    return encrypt(ciphertext, key);   // XOR simetriktir
#else
    return {};
#endif
}

/// @brief Cipher desteğinin aktif olup olmadığını sorgular.
constexpr bool is_available() noexcept
{
    return static_cast<bool>(FEATURE_CIPHER_ENABLED);
}

} // namespace cipher

#endif // SCARD_TOOL_CIPHER_H
