#ifndef PCSC_RESULT_H
#define PCSC_RESULT_H

#include "StatusWordHandler.h"
#include <string>
#include <variant>
#include <cstdint>
#include <stdexcept>
#include <type_traits>
#include <utility>

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
enum class PcscErrorCode : uint8_t {
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

	Unknown = static_cast<uint8_t>(~0) //UINT8_MAX
};

inline std::string describe(PcscErrorCode code) {
	switch (code) {
		case PcscErrorCode::Success:                return "Success";
		case PcscErrorCode::NotConnected:           return "Not connected";
		case PcscErrorCode::ResponseTooShort:       return "Response too short";
		case PcscErrorCode::AuthRequired:           return "Authentication required";
		case PcscErrorCode::AuthFailed:             return "Authentication failed";
		case PcscErrorCode::AuthBlocked:            return "Authentication blocked";
		case PcscErrorCode::LoadKeyFailed:          return "Load key failed";
		case PcscErrorCode::ReadFailed:             return "Read failed";
		case PcscErrorCode::WriteFailed:            return "Write failed";
		case PcscErrorCode::WrongLength:            return "Wrong length";
		case PcscErrorCode::SecurityNotSatisfied:   return "Security status not satisfied";
		case PcscErrorCode::ConditionsNotSatisfied: return "Conditions of use not satisfied";
		case PcscErrorCode::FileNotFound:           return "File not found";
		case PcscErrorCode::IncorrectParameters:    return "Incorrect parameters";
		case PcscErrorCode::InsNotSupported:        return "INS not supported";
		case PcscErrorCode::ClaNotSupported:        return "CLA not supported";
		case PcscErrorCode::NotDesfire:             return "Not a DESFire card";
		case PcscErrorCode::NotAuthenticated:       return "Not authenticated";
		case PcscErrorCode::SessionExpired:         return "Session expired";
		case PcscErrorCode::ManufacturerBlock:      return "Manufacturer block protected";
		case PcscErrorCode::TrailerBlock:           return "Trailer block protected";
		case PcscErrorCode::InvalidData:            return "Invalid data";
		case PcscErrorCode::DesfireError:           return "DESFire error";
		case PcscErrorCode::DesfirePermissionDenied: return "DESFire permission denied";
		case PcscErrorCode::DesfireAppNotFound:     return "DESFire application not found";
		case PcscErrorCode::DesfireFileNotFound:    return "DESFire file not found";
		case PcscErrorCode::DesfireAuthMismatch:    return "DESFire auth verification failed";
		default:                                return "Unknown error";
	}
}

// ── kategori bazlı enum'lar (type-safe) ───────────────────────────────────
enum class ConnectionError : uint8_t {
	Success = 0,
	NotConnected,
	ResponseTooShort,
	Unknown = static_cast<uint8_t>(~0)
};
enum class AuthError : uint8_t {
	Success = 0,
	AuthRequired,
	AuthFailed,
	AuthBlocked,
	LoadKeyFailed,
	Unknown = static_cast<uint8_t>(~0)
};
enum class IoError : uint8_t {
	Success = 0,
	ReadFailed,
	WriteFailed,
	Unknown = static_cast<uint8_t>(~0)
};
enum class Iso7816Error : uint8_t {
	Success = 0,
	WrongLength,
	SecurityNotSatisfied,
	ConditionsNotSatisfied,
	FileNotFound,
	IncorrectParameters,
	InsNotSupported,
	ClaNotSupported,
	Unknown = static_cast<uint8_t>(~0)
};
enum class CardError : uint8_t {
	Success = 0,
	NotDesfire,
	NotAuthenticated,
	SessionExpired,
	ManufacturerBlock,
	TrailerBlock,
	InvalidData,
	Unknown = static_cast<uint8_t>(~0)
};
enum class DesfireError : uint8_t {
	Success = 0,
	Generic,
	PermissionDenied,
	AppNotFound,
	FileNotFound,
	AuthMismatch,
	Unknown = static_cast<uint8_t>(~0)
};

// Variant tipi: hangi kategori hatası olduğunu taşır
using PcscErrorKind = std::variant<
    std::monostate, // success
    ConnectionError,
    AuthError,
    IoError,
    Iso7816Error,
    CardError,
    DesfireError>;

// ---------- PcscError: eski API'yi koruyan, içte variant kullanan yapı ----------
struct PcscError {
	// dışa açık (geri uyumluluk için) - eskiden kullanılan enum
	PcscErrorCode code = PcscErrorCode::Success;

	// iç temsil: variant ile hangi kategori hatası olduğu bilinir
	PcscErrorKind kind = std::monostate{};

	// opsiyonel ek bilgi
	StatusWord sw{};
	std::string detail;

	// --- ctor'lar / factory'ler ---
	PcscError() = default;
	PcscError(PcscErrorCode c, const StatusWord& s) : code(c) , kind(mapCodeToKind(c)) , sw(s) {}
	PcscError(PcscErrorCode c) : code(c), kind(mapCodeToKind(c)){}
	PcscError(PcscErrorCode c, std::string d) : code(c), detail(std::move(d)){}
	PcscError(PcscErrorCode c, const StatusWord& s, std::string d)
	    : code(c), kind(mapCodeToKind(c)), sw(s), detail(std::move(d)){}

	static PcscError make(PcscErrorCode c, std::string d = {}) {
		PcscError e;
		e.code = c;
		e.kind = mapCodeToKind(c);
		e.detail = std::move(d);
		return e;
	}
	static PcscError make(PcscErrorCode c, const StatusWord& s, std::string d = {}) {
		PcscError e;
		e.code = c;
		e.kind = mapCodeToKind(c);
		e.sw = s;
		e.detail = std::move(d);
		return e;
	}

	// tip-güvenli helperler:
	static PcscError from(ConnectionError c, std::string d = {}) {
		PcscError e;
		e.kind = c;
		e.code = mapKindToCode(c);
		e.detail = std::move(d);
		return e;
	}
	static PcscError from(AuthError c, std::string d = {}) {
		PcscError e;
		e.kind = c;
		e.code = mapKindToCode(c);
		e.detail = std::move(d);
		return e;
	}
	static PcscError from(IoError c, std::string d = {}) {
		PcscError e;
		e.kind = c;
		e.code = mapKindToCode(c);
		e.detail = std::move(d);
		return e;
	}
	static PcscError from(Iso7816Error c, const StatusWord& s = {}, std::string d = {}) {
		PcscError e;
		e.kind = c;
		e.code = mapKindToCode(c);
		e.sw = s;
		e.detail = std::move(d);
		return e;
	}
	static PcscError from(CardError c, std::string d = {}) {
		PcscError e;
		e.kind = c;
		e.code = mapKindToCode(c);
		e.detail = std::move(d);
		return e;
	}
	static PcscError from(DesfireError c, std::string d = {}) {
		PcscError e;
		e.kind = c;
		e.code = mapKindToCode(c);
		e.detail = std::move(d);
		return e;
	}

	// StatusWord → PcscError (compatibility)
	static PcscError fromStatusWord(const StatusWord& s) {
		if (s.isSuccess()) return make(PcscErrorCode::Success, s, {});
		else if (s.isAuthSentinel()) return make(PcscErrorCode::AuthRequired, s, {});
		else if (s.isAuthBlocked()) return make(PcscErrorCode::AuthBlocked, s, {});
		else if (s.isSecurityError()) return make(PcscErrorCode::SecurityNotSatisfied, s, {});
		else if (s.isWrongLength()) return make(PcscErrorCode::WrongLength, s, {});
		else if (s.isFileNotFound()) return make(PcscErrorCode::FileNotFound, s, {});
		else if (s.isIncorrectP1P2()) return make(PcscErrorCode::IncorrectParameters, s, {});
		else if (s.isINSNotSupported()) return make(PcscErrorCode::InsNotSupported, s, {});
		else if (s.isCLANotSupported()) return make(PcscErrorCode::ClaNotSupported, s, {});
		else return make(PcscErrorCode::Unknown, s, {});
	}

	// --- durum kontrolü (eskisiyle uyumlu) ---
	bool ok() const noexcept { return code == PcscErrorCode::Success; }
	explicit operator bool() const noexcept { return ok(); }

	// --- message ve describe ---
	std::string message() const {
		if (ok()) return "Success";
		// Öncelikle variant üzerinden daha anlamlı açıklama üretmeye çalış:
		std::string base = describeKind(kind, code);
		if (!detail.empty()) base += ": " + detail;
		if (sw.sw1 != 0 || sw.sw2 != 0) {
			base += " [" + sw.toHexFormatted();
			std::string swmsg = sw.getMessage();
			if (!swmsg.empty()) base += ": " + swmsg;
			base += "]";
		}
		return base;
	}

	// --- exception bridge: eski davranışı koru (code'a göre fırlat) ---
	void throwIfError() const {
		if (ok()) return;
		std::string msg = message();
		switch (code) {
			case PcscErrorCode::AuthRequired:
			case PcscErrorCode::AuthFailed:
			case PcscErrorCode::AuthBlocked:
			case PcscErrorCode::DesfireAuthMismatch: throw std::runtime_error(msg); // eskiden pcsc::AuthFailedError vb. varsa kullan
			case PcscErrorCode::LoadKeyFailed: throw std::runtime_error(msg); // pcsc::LoadKeyFailedError
			case PcscErrorCode::ReadFailed: throw std::runtime_error(msg); // pcsc::ReaderReadError
			case PcscErrorCode::WriteFailed: throw std::runtime_error(msg); // pcsc::ReaderWriteError
			case PcscErrorCode::NotDesfire:
			case PcscErrorCode::ManufacturerBlock:
			case PcscErrorCode::TrailerBlock: throw std::logic_error(msg);
			case PcscErrorCode::InvalidData: throw std::invalid_argument(msg);
			default: throw std::runtime_error(msg); // fallback
		}
	}

private:
	// --- mapping yardımcıları ---
	static PcscErrorKind mapCodeToKind(PcscErrorCode c) {
		switch (c) {
			case PcscErrorCode::Success: return std::monostate{};
			case PcscErrorCode::NotConnected: return ConnectionError::NotConnected;
			case PcscErrorCode::ResponseTooShort: return ConnectionError::ResponseTooShort;

			case PcscErrorCode::AuthRequired: return AuthError::AuthRequired;
			case PcscErrorCode::AuthFailed: return AuthError::AuthFailed;
			case PcscErrorCode::AuthBlocked: return AuthError::AuthBlocked;
			case PcscErrorCode::LoadKeyFailed: return AuthError::LoadKeyFailed;

			case PcscErrorCode::ReadFailed: return IoError::ReadFailed;
			case PcscErrorCode::WriteFailed: return IoError::WriteFailed;

			case PcscErrorCode::WrongLength: return Iso7816Error::WrongLength;
			case PcscErrorCode::SecurityNotSatisfied: return Iso7816Error::SecurityNotSatisfied;
			case PcscErrorCode::ConditionsNotSatisfied: return Iso7816Error::ConditionsNotSatisfied;
			case PcscErrorCode::FileNotFound: return Iso7816Error::FileNotFound;
			case PcscErrorCode::IncorrectParameters: return Iso7816Error::IncorrectParameters;
			case PcscErrorCode::InsNotSupported: return Iso7816Error::InsNotSupported;
			case PcscErrorCode::ClaNotSupported: return Iso7816Error::ClaNotSupported;

			case PcscErrorCode::NotDesfire: return CardError::NotDesfire;
			case PcscErrorCode::NotAuthenticated: return CardError::NotAuthenticated;
			case PcscErrorCode::SessionExpired: return CardError::SessionExpired;
			case PcscErrorCode::ManufacturerBlock: return CardError::ManufacturerBlock;
			case PcscErrorCode::TrailerBlock: return CardError::TrailerBlock;
			case PcscErrorCode::InvalidData: return CardError::InvalidData;

			case PcscErrorCode::DesfireError: return DesfireError::Generic;
			case PcscErrorCode::DesfirePermissionDenied: return DesfireError::PermissionDenied;
			case PcscErrorCode::DesfireAppNotFound: return DesfireError::AppNotFound;
			case PcscErrorCode::DesfireFileNotFound: return DesfireError::FileNotFound;
			case PcscErrorCode::DesfireAuthMismatch: return DesfireError::AuthMismatch;
			default: return ConnectionError::Unknown;
		}
	}

	// mapKindToCode: kategori bazlı enum -> PcscErrorCode
	template<typename E>
	static PcscErrorCode mapKindToCode(E e) {
		// Genel kalıp: her enum için specialize eden overload'lar aşağıda mevcut.
		return PcscErrorCode::Unknown;
	}

	// Özel eşlemeler:
	static PcscErrorCode mapKindToCode(ConnectionError e) {
		switch (e) {
			case ConnectionError::NotConnected: return PcscErrorCode::NotConnected;
			case ConnectionError::ResponseTooShort: return PcscErrorCode::ResponseTooShort;
			default: break;
		}
		return PcscErrorCode::Unknown;
	}
	static PcscErrorCode mapKindToCode(AuthError e) {
		switch (e) {
			case AuthError::AuthRequired: return PcscErrorCode::AuthRequired;
			case AuthError::AuthFailed: return PcscErrorCode::AuthFailed;
			case AuthError::AuthBlocked: return PcscErrorCode::AuthBlocked;
			case AuthError::LoadKeyFailed: return PcscErrorCode::LoadKeyFailed;
			default: break;
		}
		return PcscErrorCode::Unknown;
	}
	static PcscErrorCode mapKindToCode(IoError e) {
		switch (e) {
			case IoError::ReadFailed: return PcscErrorCode::ReadFailed;
			case IoError::WriteFailed: return PcscErrorCode::WriteFailed;
			default: break;
		}
		return PcscErrorCode::Unknown;
	}
	static PcscErrorCode mapKindToCode(Iso7816Error e) {
		switch (e) {
			case Iso7816Error::WrongLength: return PcscErrorCode::WrongLength;
			case Iso7816Error::SecurityNotSatisfied: return PcscErrorCode::SecurityNotSatisfied;
			case Iso7816Error::ConditionsNotSatisfied: return PcscErrorCode::ConditionsNotSatisfied;
			case Iso7816Error::FileNotFound: return PcscErrorCode::FileNotFound;
			case Iso7816Error::IncorrectParameters: return PcscErrorCode::IncorrectParameters;
			case Iso7816Error::InsNotSupported: return PcscErrorCode::InsNotSupported;
			case Iso7816Error::ClaNotSupported: return PcscErrorCode::ClaNotSupported;
			default: break;
		}
		return PcscErrorCode::Unknown;
	}
	static PcscErrorCode mapKindToCode(CardError e) {
		switch (e) {
			case CardError::NotDesfire: return PcscErrorCode::NotDesfire;
			case CardError::NotAuthenticated: return PcscErrorCode::NotAuthenticated;
			case CardError::SessionExpired: return PcscErrorCode::SessionExpired;
			case CardError::ManufacturerBlock: return PcscErrorCode::ManufacturerBlock;
			case CardError::TrailerBlock: return PcscErrorCode::TrailerBlock;
			case CardError::InvalidData: return PcscErrorCode::InvalidData;
			default: break;
		}
		return PcscErrorCode::Unknown;
	}
	static PcscErrorCode mapKindToCode(DesfireError e) {
		switch (e) {
			case DesfireError::Generic: return PcscErrorCode::DesfireError;
			case DesfireError::PermissionDenied: return PcscErrorCode::DesfirePermissionDenied;
			case DesfireError::AppNotFound: return PcscErrorCode::DesfireAppNotFound;
			case DesfireError::FileNotFound: return PcscErrorCode::DesfireFileNotFound;
			case DesfireError::AuthMismatch: return PcscErrorCode::DesfireAuthMismatch;
			default: break;
		}
		return PcscErrorCode::Unknown;
	}

	// describeKind: variant veya fallback PcscErrorCode'a göre metin üretir
	static std::string describeKind(const PcscErrorKind& k, PcscErrorCode fallback) {
		return std::visit([&](auto&& v) -> std::string
		                  {
            using V = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<V, std::monostate>) {
                return describe(fallback);
            } else if constexpr (std::is_same_v<V, ConnectionError>) {
                switch (v) {
					case ConnectionError::NotConnected: return "Not connected";
					case ConnectionError::ResponseTooShort: return "Response too short";
					default: break;
                }
            } else if constexpr (std::is_same_v<V, AuthError>) {
                switch (v) {
					case AuthError::AuthRequired: return "Authentication required";
					case AuthError::AuthFailed: return "Authentication failed";
					case AuthError::AuthBlocked: return "Authentication blocked";
					case AuthError::LoadKeyFailed: return "Load key failed";
					default: break;
                }
            } else if constexpr (std::is_same_v<V, IoError>) {
                switch (v) {
					case IoError::ReadFailed: return "Read failed";
					case IoError::WriteFailed: return "Write failed";
					default: break;
                }
            } else if constexpr (std::is_same_v<V, Iso7816Error>) {
                switch (v) {
					case Iso7816Error::WrongLength: return "Wrong length";
					case Iso7816Error::SecurityNotSatisfied: return "Security status not satisfied";
					case Iso7816Error::ConditionsNotSatisfied: return "Conditions of use not satisfied";
					case Iso7816Error::FileNotFound: return "File not found";
					case Iso7816Error::IncorrectParameters: return "Incorrect parameters";
					case Iso7816Error::InsNotSupported: return "INS not supported";
					case Iso7816Error::ClaNotSupported: return "CLA not supported";
					default: break;
                }
            } else if constexpr (std::is_same_v<V, CardError>) {
                switch (v) {
					case CardError::NotDesfire: return "Not a DESFire card";
					case CardError::NotAuthenticated: return "Not authenticated";
					case CardError::SessionExpired: return "Session expired";
					case CardError::ManufacturerBlock: return "Manufacturer block protected";
					case CardError::TrailerBlock: return "Trailer block protected";
					case CardError::InvalidData: return "Invalid data";
					default: break;
                }
            } else if constexpr (std::is_same_v<V, DesfireError>) {
                switch (v) {
					case DesfireError::Generic: return "DESFire error";
					case DesfireError::PermissionDenied: return "DESFire permission denied";
					case DesfireError::AppNotFound: return "DESFire application not found";
					case DesfireError::FileNotFound: return "DESFire file not found";
					case DesfireError::AuthMismatch: return "DESFire auth verification failed";
					default: break;
                }
            }
            // Eğer variant'tan bilgi alınamazsa fallback (eski enum)
            return describe(fallback); }, k);
	}
}; // struct PcscError

// ---------- Result<T, E=PcscError> (mevcut kullanım korunuyor) ----------
template<typename T>
class Result {
private:
	std::variant<T, PcscError> data;
public:
	Result(T v) : data(std::move(v)) {}
	Result(PcscError e) : data(std::move(e)) {}
	Result(T v, PcscError e) : data(std::move(v)){}

	bool ok() const noexcept { return std::holds_alternative<T>(data); }
	explicit operator bool() const noexcept { return ok(); }

	T& unwrap() {
		std::get<PcscError>(data).throwIfError();
		return std::get<T>(data);
	}

	const PcscError& error() const {
		static const PcscError success = PcscError::make(PcscErrorCode::Success);
		return ok() ? success : std::get<PcscError>(data);
	}

	template<typename F>
	auto map(F&& f) {
		using U = decltype(f(std::declval<T>()));
		return ok() ? Result<U>(f(std::get<T>(data))) : Result<U>(std::get<PcscError>(data));
	}

	template<typename F>
	auto and_then(F&& f) {
		using R = decltype(f(std::get<T>(data)));
		return ok() ? f(std::get<T>(data)) : R(std::get<PcscError>(data));
	}
};

// void işlemler için (özelleştirilmiş PcscError) — kullanım aynıdır
template<>
class Result<void> {
private:
	PcscError err;
public:
	Result() = default;
	Result(PcscError e) : err(std::move(e)) {}

	bool ok() const noexcept { return err.ok(); }
	explicit operator bool() const noexcept { return ok(); }

	void unwrap() const { err.throwIfError(); }
	const PcscError& error() const {
		static const PcscError success = PcscError::make(PcscErrorCode::Success);
		return ok() ? success : err;
	}
};

#endif // PCSC_RESULT_H
