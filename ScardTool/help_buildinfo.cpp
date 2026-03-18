// src/commands/help_buildinfo.cpp
// ─────────────────────────────────────────────────────────────────────────────
// --help ve --buildinfo komut çıktıları.
// build_info.h'dan gelen makrolar sayesinde derleme bilgisi otomatik güncellenir.
// ─────────────────────────────────────────────────────────────────────────────

#include "help_buildinfo.h"
#include "build_info.h"
#include "cipher_helper.h"

#include <iostream>
#include <iomanip>
#include <string>

// Windows'ta UTF-8 çıktı için konsol code page'i ayarla
#if defined(_WIN32)
#  include <Windows.h>
#  include <io.h>
#  include <fcntl.h>
#endif

namespace {

// Bant genişliği sabit — ortalı çıktı için
constexpr int kLineWidth = 62;

void print_separator(char ch = '-') {
    std::cout << std::string(kLineWidth, ch) << '\n';
}

void print_header(const char* title) {
    print_separator('=');
    int pad = (kLineWidth - static_cast<int>(std::string(title).size())) / 2;
    std::cout << std::string(pad > 0 ? pad : 0, ' ') << title << '\n';
    print_separator('=');
}

/// Feature durumunu "[ACIK]" / "[KAPALI *]" formatında döndürür
std::string feature_status(bool enabled) {
    return enabled ? "[ ACIK  ]" : "[ KAPALI - bu derlemede kullanılamaz ]";
}

} // anonymous namespace

// ─────────────────────────────────────────────────────────────────────────────
void print_help()
{
    print_header("ScardTool - Yardim");
    std::cout
        << '\n'
        << "  Kullanim: ScardTool [secenekler]\n"
        << '\n'
        << "  Genel Secenekler:\n"
        << "    --help        Bu yardim mesajini goster\n"
        << "    --buildinfo   Derleme bilgilerini goster\n"
        << "    --version     Surum bilgisini goster\n"
        << '\n'
        << "  Kart Komutlari:\n"
        << "    --list        Bagli okuyuculari listele\n"
        << "    --connect     Kart baglantisi kur\n"
        << "    --send <apdu> APDU komutu gonder\n"
        << '\n';

    print_separator();
    std::cout << "  Derleme Ozellikleri:\n";
    std::cout << "    cipher (sifreleme)  : " << feature_status(cipher::is_available()) << '\n';

#if !FEATURE_CIPHER_ENABLED
    std::cout
        << '\n'
        << "  UYARI: Cipher destegi bu derlemede DEVRE DISI.\n"
        << "         Etkinlestirmek icin:\n"
        << "           cmake -DENABLE_CIPHER=ON ...\n";
#endif

    print_separator();
    std::cout
        << "  Versiyon : " BUILD_VERSION_STR  " (" BUILD_GIT_HASH ")\n"
        << "  Derleyen : " BUILD_COMPILER_ID  "\n"
        << '\n';
}

// ─────────────────────────────────────────────────────────────────────────────
void print_buildinfo()
{
    print_header("ScardTool - Derleme Bilgisi");
    std::cout
        << '\n'
        << "  Surum          : " BUILD_VERSION_STR                     "\n"
        << "  Git commit     : " BUILD_GIT_HASH                        "\n"
        << "  Git describe   : " BUILD_GIT_DESCRIBE                    "\n"
        << "  Tarih          : " BUILD_DATE " " BUILD_TIME              "\n"
        << '\n';

    print_separator();
    std::cout << "  Derleyici Ayarlari:\n"
        << "    Derleyici    : " BUILD_COMPILER_ID                      "\n"
        << "    C++ Std.     : C++" BUILD_CXX_STANDARD                  "\n"
        << "    Generator    : " BUILD_CMAKE_GENERATOR                  "\n"
        << "    Runtime      : " BUILD_RUNTIME_TYPE                     "\n"
        << '\n';

    print_separator();
    std::cout << "  Ozellik Durumu:\n";
    std::cout << "    cipher        : " << feature_status(cipher::is_available()) << '\n';
    // Yeni feature flag'ları buraya eklenir

    print_separator();
    // Ham makro çıktısı — BUILD_FEATURES_SUMMARY satır satır içerir
    std::cout << "  Ozet:\n" BUILD_FEATURES_SUMMARY << '\n';

#if !FEATURE_CIPHER_ENABLED
    std::cout
        << "  [!] KAPALI OZELLIK UYARISI\n"
        << "      ENABLE_CIPHER=OFF ile derlendi.\n"
        << "      cipher_encrypt() / cipher_decrypt() kullanilamaz.\n"
        << "      Yeniden derlemek icin: cmake -DENABLE_CIPHER=ON ..\n"
        << '\n';
#endif
}

// ─────────────────────────────────────────────────────────────────────────────
void setup_unicode_console()
{
#if defined(_WIN32)
    // Konsolu UTF-8'e ayarla — wcout/cout karışıklığını önler
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    // stdout'u binary modda aç — UTF-8 BOM'u engelle
    _setmode(_fileno(stdout), _O_BINARY);
    _setmode(_fileno(stderr), _O_BINARY);
#endif
}
