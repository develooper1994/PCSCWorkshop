/**
 * @file ExitCode.h
 * @brief POSIX uyumlu çıkış kodları ve hata yardımcıları.
 */
#pragma once
#include <cstdlib>
#include <string>
#include <iostream>

// ════════════════════════════════════════════════════════════════════════════════
// ExitCode — POSIX uyumlu çıkış kodları
//
// 0  Success
// 1  GeneralError  — kart yok, okuyucu yok
// 2  InvalidArgs   — geçersiz argüman / eksik parametre
// 3  PcscError     — PC/SC API hatası
// 4  CardCommError — kart iletişim hatası (yanlış APDU, auth fail vb.)
// ════════════════════════════════════════════════════════════════════════════════

enum class ExitCode : int {
    Success       = 0,
    GeneralError  = 1,
    InvalidArgs   = 2,
    PcscError     = 3,
    CardCommError = 4,
};

inline int toInt(ExitCode c) { return static_cast<int>(c); }

// stderr'e hata yaz ve uygun exit code döndür
inline ExitCode fail(ExitCode code, const std::string& msg) {
    std::cerr << "Error: " << msg << "\n";
    return code;
}
