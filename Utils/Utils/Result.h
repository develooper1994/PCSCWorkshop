#ifndef PCSC_RESULT_H
#define PCSC_RESULT_H

#include "StatusWordHandler.h"
#include <string>

// ════════════════════════════════════════════════════════════════════════════════
// Result — Exception-free hata yönetimi altyapısı
// ════════════════════════════════════════════════════════════════════════════════
//
// Protokol seviyesindeki hatalar (yanlış SW, auth gerekli, okuma/yazma hatası)
// exception yerine Result<T> ile döndürülür. Gömülü sistemlerde exception
// kısıtlaması varsa bu altyapı doğrudan kullanılır.
//
// ─── Kullanım (exception-free) ─────────────────────────────────────────────
//
//   auto result = reader.tryReadPage(4);
//   if (result) {
//       process(result.value);
//   } else {
//       log(result.error.message());
//   }
//
// ─── Kullanım (exception bridge) ──────────────────────────────────────────
//
//   auto data = reader.tryReadPage(4).unwrap();   // hata varsa throw eder
//   // veya mevcut API:
//   auto data = reader.readPage(4);               // aynı davranış
//
// ════════════════════════════════════════════════════════════════════════════════

// ── Hata kodu kataloğu ────────────────────────────────────────────────────
enum class ErrorCode : uint8_t {
	Success = 0,

	// Bağlantı
	NotConnected,
	ResponseTooShort,

	// Kimlik doğrulama
	AuthRequired,
	AuthFailed,
	AuthBlocked,
	LoadKeyFailed,

	// I/O
	ReadFailed,
	WriteFailed,

	// Protokol (ISO 7816-4)
	WrongLength,
	SecurityNotSatisfied,
	ConditionsNotSatisfied,
	FileNotFound,
	IncorrectParameters,
	InsNotSupported,
	ClaNotSupported,

	// Kart seviyesi
	NotDesfire,
	NotAuthenticated,
	SessionExpired,
	ManufacturerBlock,
	TrailerBlock,
	InvalidData,

	// DESFire protokol
	DesfireError,
	DesfirePermissionDenied,
	DesfireAppNotFound,
	DesfireFileNotFound,
	DesfireAuthMismatch,

	Unknown
};

inline std::string describe(ErrorCode code) {
	switch (code) {
	case ErrorCode::Success:                return "Success";
	case ErrorCode::NotConnected:           return "Not connected";
	case ErrorCode::ResponseTooShort:       return "Response too short";
	case ErrorCode::AuthRequired:           return "Authentication required";
	case ErrorCode::AuthFailed:             return "Authentication failed";
	case ErrorCode::AuthBlocked:            return "Authentication blocked";
	case ErrorCode::LoadKeyFailed:          return "Load key failed";
	case ErrorCode::ReadFailed:             return "Read failed";
	case ErrorCode::WriteFailed:            return "Write failed";
	case ErrorCode::WrongLength:            return "Wrong length";
	case ErrorCode::SecurityNotSatisfied:   return "Security status not satisfied";
	case ErrorCode::ConditionsNotSatisfied: return "Conditions of use not satisfied";
	case ErrorCode::FileNotFound:           return "File not found";
	case ErrorCode::IncorrectParameters:    return "Incorrect parameters";
	case ErrorCode::InsNotSupported:        return "INS not supported";
	case ErrorCode::ClaNotSupported:        return "CLA not supported";
	case ErrorCode::NotDesfire:             return "Not a DESFire card";
	case ErrorCode::NotAuthenticated:       return "Not authenticated";
	case ErrorCode::SessionExpired:         return "Session expired";
	case ErrorCode::ManufacturerBlock:      return "Manufacturer block protected";
	case ErrorCode::TrailerBlock:           return "Trailer block protected";
	case ErrorCode::InvalidData:            return "Invalid data";
	case ErrorCode::DesfireError:           return "DESFire error";
	case ErrorCode::DesfirePermissionDenied: return "DESFire permission denied";
	case ErrorCode::DesfireAppNotFound:     return "DESFire application not found";
	case ErrorCode::DesfireFileNotFound:    return "DESFire file not found";
	case ErrorCode::DesfireAuthMismatch:    return "DESFire auth verification failed";
	default:                                return "Unknown error";
	}
}

// ── PcscError — Yapılandırılmış hata bilgisi ──────────────────────────────
struct PcscError {
	ErrorCode code = ErrorCode::Success;
	StatusWord sw;

	bool ok() const noexcept { return code == ErrorCode::Success; }

	std::string message() const {
		if (ok()) return "Success";
		std::string msg = describe(code);
		if (sw.sw1 != 0 || sw.sw2 != 0)
			msg += " [" + sw.toHexFormatted() + ": " + sw.getMessage() + "]";
		return msg;
	}

	// Exception bridge — mevcut exception tabanlı kodla uyumluluk
	void throwIfError() const {
		if (ok()) return;
		std::string msg = message();
		switch (code) {
		case ErrorCode::AuthRequired:
		case ErrorCode::AuthFailed:
		case ErrorCode::AuthBlocked:
		case ErrorCode::DesfireAuthMismatch:
			throw pcsc::AuthFailedError(msg);
		case ErrorCode::LoadKeyFailed:
			throw pcsc::LoadKeyFailedError(msg);
		case ErrorCode::ReadFailed:
			throw pcsc::ReaderReadError(msg);
		case ErrorCode::WriteFailed:
			throw pcsc::ReaderWriteError(msg);
		case ErrorCode::NotDesfire:
		case ErrorCode::ManufacturerBlock:
		case ErrorCode::TrailerBlock:
			throw std::logic_error(msg);
		case ErrorCode::InvalidData:
			throw std::invalid_argument(msg);
		default:
			throw pcsc::ReaderError(msg);
		}
	}

	// StatusWord → ErrorCode çevrimi
	static PcscError fromStatusWord(const StatusWord& s) {
		if (s.isSuccess())        return {ErrorCode::Success, s};
		if (s.isAuthSentinel())   return {ErrorCode::AuthRequired, s};
		if (s.isAuthBlocked())    return {ErrorCode::AuthBlocked, s};
		if (s.isSecurityError())  return {ErrorCode::SecurityNotSatisfied, s};
		if (s.isWrongLength())    return {ErrorCode::WrongLength, s};
		if (s.isFileNotFound())   return {ErrorCode::FileNotFound, s};
		if (s.isIncorrectP1P2())  return {ErrorCode::IncorrectParameters, s};
		if (s.isINSNotSupported()) return {ErrorCode::InsNotSupported, s};
		if (s.isCLANotSupported()) return {ErrorCode::ClaNotSupported, s};
		return {ErrorCode::Unknown, s};
	}
};

// ── Result<T> — Değer veya hata taşıyıcı ─────────────────────────────────
template<typename T>
struct Result {
	T value;
	PcscError error;

	bool ok() const noexcept { return error.ok(); }
	explicit operator bool() const noexcept { return ok(); }

	T& unwrap() { error.throwIfError(); return value; }
	const T& unwrap() const { error.throwIfError(); return value; }
};

// void işlemler için (write, auth, vb.)
template<>
struct Result<void> {
	PcscError error;

	bool ok() const noexcept { return error.ok(); }
	explicit operator bool() const noexcept { return ok(); }

	void unwrap() const { error.throwIfError(); }
};

#endif // PCSC_RESULT_H
