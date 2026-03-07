#ifndef PCSC_WORKSHOP1_STATUS_WORD_HANDLER_H
#define PCSC_WORKSHOP1_STATUS_WORD_HANDLER_H

#include "PcscUtils.h"
#include "Exceptions/GenericExceptions.h"
#include <sstream>
#include <string>
#include <utility>

// ============================================================
// StatusWordType — Status word kategorileri
// ============================================================
enum class StatusWordType {
	Success,
	AuthSentinel,
	WrongLength,
	SecurityError,
	AuthBlocked,
	FileNotFound,
	IncorrectP1P2,
	WrongLE,
	INSNotSupported,
	CLANotSupported,
	Unknown
};

// ============================================================
// StatusWord — Merkezi status word yönetimi (basit veri taşıyıcı)
// ============================================================
// Not: Enum ve Checker class'ı StatusWordHandler.h'de tanımlı
struct StatusWord {
	BYTE sw1;
	BYTE sw2;

	StatusWord(BYTE s1 = 0x00, BYTE s2 = 0x00) noexcept : sw1(s1), sw2(s2) {}

	// Initialization from pair (used by CardConnection)
	StatusWord(const std::pair<BYTE, BYTE>& sw) noexcept : sw1(sw.first), sw2(sw.second) {}

	// Assignment from pair
	StatusWord& operator=(const std::pair<BYTE, BYTE>& sw) noexcept {
		sw1 = sw.first;
		sw2 = sw.second;
		return *this;
	}

	// ════════════════════════════════════════════════════════
	//  Status word yorumlama metodları
	// ════════════════════════════════════════════════════════

	/// Normal başarı: 0x90 0x00 veya 0x91 0x00
	bool isSuccess() const noexcept {
		return (sw1 == 0x90 || sw1 == 0x91) && sw2 == 0x00;
	}

	/// Auth-failed sentinel: 0x63 0x00 (authentication gerekli, hata DEĞİL)
	bool isAuthSentinel() const noexcept {
		return sw1 == 0x63 && sw2 == 0x00;
	}

	/// Gerçek hata durumu: başarı veya sentinel değilse
	bool isError() const noexcept {
		return !isSuccess() && !isAuthSentinel();
	}

	// ════════════════════════════════════════════════════════
	//  ISO 7816-4 standart hata kodları
	// ════════════════════════════════════════════════════════

	/// Wrong length (0x67 0x00)
	bool isWrongLength() const noexcept {
		return sw1 == 0x67 && sw2 == 0x00;
	}

	/// Security status not satisfied (0x69 0x82)
	bool isSecurityError() const noexcept {
		return sw1 == 0x69 && sw2 == 0x82;
	}

	/// Authentication blocked (0x69 0x83)
	bool isAuthBlocked() const noexcept {
		return sw1 == 0x69 && sw2 == 0x83;
	}

	/// File not found / incorrect parameters (0x6A 0x82)
	bool isFileNotFound() const noexcept {
		return sw1 == 0x6A && sw2 == 0x82;
	}

	/// Incorrect parameters P1-P2 (0x6A 0x86)
	bool isIncorrectP1P2() const noexcept {
		return sw1 == 0x6A && sw2 == 0x86;
	}

	/// Wrong LE field (0x6C XX) - XX indicates correct length
	bool isWrongLE() const noexcept {
		return sw1 == 0x6C;
	}

	/// INS not supported (0x6D 0x00)
	bool isINSNotSupported() const noexcept {
		return sw1 == 0x6D && sw2 == 0x00;
	}

	/// CLA not supported (0x6E 0x00)
	bool isCLANotSupported() const noexcept {
		return sw1 == 0x6E && sw2 == 0x00;
	}

	/// Tutarlı formatta hex string: "0x90 0x00" formatında
	std::string toHexFormatted() const {
		BYTE bytes[2] = { sw1, sw2 };
		std::string hex = toHex(bytes, 2);  // toHex kullanılıyor: "90 00"
		return "0x" + hex.substr(0, 2) + " 0x" + hex.substr(3, 2);
	}

	/// Anlamlı hata mesajı üret
	std::string getMessage() const {
		if (isSuccess()) return "Success";
		else if (isAuthSentinel()) return "Authentication required (sentinel)";
		else if (isWrongLength()) return "Wrong length";
		else if (isSecurityError()) return "Security status not satisfied";
		else if (isAuthBlocked()) return "Authentication blocked";
		else if (isFileNotFound()) return "File not found / Incorrect parameters";
		else if (isIncorrectP1P2()) return "Incorrect P1-P2";
		else if (isWrongLE()) return "Wrong LE field (correct: 0x" + std::to_string(sw2) + ")";
		else if (isINSNotSupported()) return "INS not supported";
		else if (isCLANotSupported()) return "CLA not supported";
		else return "Unknown error";
	}

	/// Tam hata mesajı: "Operation failed SW=0x69 0x82: Security status not satisfied"
	std::string getFullMessage(const std::string& operation = "Operation") const {
		if (isSuccess() || isAuthSentinel()) {
			return operation + " completed with SW=" + toHexFormatted();
		}
		return operation + " failed SW=" + toHexFormatted() + ": " + getMessage();
	}
};

// ============================================================
// StatusWord kategorilendirme extension
// ============================================================
// StatusWord struct PcscUtils.h'de tanımlı, burada sadece
// type() metodu için kategorilendirme enum'u ekliyoruz

/// StatusWord'ü kategoriye çevir
inline StatusWordType typeOf(const StatusWord& sw) noexcept {
	if (sw.isSuccess()) return StatusWordType::Success;
	else if (sw.isAuthSentinel()) return StatusWordType::AuthSentinel;
	else if (sw.isWrongLength()) return StatusWordType::WrongLength;
	else if (sw.isSecurityError()) return StatusWordType::SecurityError;
	else if (sw.isAuthBlocked()) return StatusWordType::AuthBlocked;
	else if (sw.isFileNotFound()) return StatusWordType::FileNotFound;
	else if (sw.isIncorrectP1P2()) return StatusWordType::IncorrectP1P2;
	else if (sw.isWrongLE()) return StatusWordType::WrongLE;
	else if (sw.isINSNotSupported()) return StatusWordType::INSNotSupported;
	else if (sw.isCLANotSupported()) return StatusWordType::CLANotSupported;
	else return StatusWordType::Unknown;
}

/// StatusWord'ün kategorisini al
inline StatusWordType categoryOf(const StatusWord& sw) noexcept {
	return typeOf(sw);
}

#endif // PCSC_WORKSHOP1_STATUS_WORD_HANDLER_H

