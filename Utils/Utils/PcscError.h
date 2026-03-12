#ifndef PCSC_ERROR_H
#define PCSC_ERROR_H

// ── Hata kodu kataloğu (variant tabanlı, geri uyumluluk kaldırıldı) ────────
#include "PcscErrorCode.h"
#include "BasicError.h"
#include <cstdint>
#include <string>
#include <variant>
#include <stdexcept>

// describeKind helper (variant-based message)
inline std::string describeKind(const PcscErrorKind& k){
	return std::visit([&](auto&& v) -> std::string {
        using V = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<V, std::monostate>) {
            return "Success";
        } else if constexpr (std::is_same_v<V, ConnectionError>) {
            switch (v) {
				case ConnectionError::Success: return "Success";
				case ConnectionError::NotConnected: return "Not connected";
				case ConnectionError::ResponseTooShort: return "Response too short";
				case ConnectionError::Unknown: return "Unknown connection error";
				default: return "Connection error";
            }
        } else if constexpr (std::is_same_v<V, AuthError>) {
            switch (v) {
				case AuthError::Success: return "Success";
				case AuthError::AuthRequired: return "Authentication required";
				case AuthError::AuthFailed: return "Authentication failed";
				case AuthError::AuthBlocked: return "Authentication blocked";
				case AuthError::LoadKeyFailed: return "Load key failed";
				case AuthError::Unknown: return "Unknown authentication error";
				default: return "Auth error";
            }
        } else if constexpr (std::is_same_v<V, IoError>) {
            switch (v) {
				case IoError::Success: return "Success";
				case IoError::ReadFailed: return "Read failed";
				case IoError::WriteFailed: return "Write failed";
				case IoError::Unknown: return "Unknown I/O error";
				default: return "I/O error";
            }
        } else if constexpr (std::is_same_v<V, Iso7816Error>) {
            switch (v) {
				case Iso7816Error::Success: return "Success";
				case Iso7816Error::WrongLength: return "Wrong length";
				case Iso7816Error::SecurityNotSatisfied: return "Security status not satisfied";
				case Iso7816Error::ConditionsNotSatisfied: return "Conditions of use not satisfied";
				case Iso7816Error::FileNotFound: return "File not found";
				case Iso7816Error::IncorrectParameters: return "Incorrect parameters";
				case Iso7816Error::InsNotSupported: return "INS not supported";
				case Iso7816Error::ClaNotSupported: return "CLA not supported";
				case Iso7816Error::Unknown: return "Unknown ISO7816 error";
				default: return "ISO7816 error";
            }
        } else if constexpr (std::is_same_v<V, CardError>) {
            switch (v) {
				case CardError::Success: return "Success";
				case CardError::NotDesfire: return "Not a DESFire card";
				case CardError::NotAuthenticated: return "Not authenticated";
				case CardError::SessionExpired: return "Session expired";
				case CardError::ManufacturerBlock: return "Manufacturer block protected";
				case CardError::TrailerBlock: return "Trailer block protected";
				case CardError::InvalidData: return "Invalid data";
				case CardError::Unknown: return "Unknown card error";
				default: return "Card error";
            }
        } else if constexpr (std::is_same_v<V, DesfireError>) {
            switch (v) {
				case DesfireError::Success: return "Success";
				case DesfireError::Generic: return "DESFire error";
				case DesfireError::PermissionDenied: return "DESFire permission denied";
				case DesfireError::AppNotFound: return "DESFire application not found";
				case DesfireError::FileNotFound: return "DESFire file not found";
				case DesfireError::AuthMismatch: return "DESFire auth verification failed";
				case DesfireError::Unknown: return "Unknown DESFire error";
				default: return "DESFire error";
            }
        }
        return "Unknown Error"; }, k);
}

// Define PcscError alias here to have a single point of definition and avoid
// cyclic includes. BasicError is already included above.
using PcscError = BasicError<PcscErrorKind>;

static PcscError fromStatusWord(const StatusWord& s) {
	if (s.isSuccess()) return PcscError::from(ConnectionError::Success, {});
	else if (s.isAuthSentinel()) return PcscError::from(AuthError::AuthRequired, {});
	else if (s.isAuthBlocked()) return PcscError::from(AuthError::AuthBlocked, {});
	else if (s.isSecurityError()) return PcscError::from(Iso7816Error::SecurityNotSatisfied, {}, s);
	else if (s.isWrongLength()) return PcscError::from(Iso7816Error::WrongLength, {}, s);
	else if (s.isFileNotFound()) return PcscError::from(Iso7816Error::FileNotFound, {}, s);
	else if (s.isIncorrectP1P2()) return PcscError::from(Iso7816Error::IncorrectParameters, {}, s);
	else if (s.isINSNotSupported()) return PcscError::from(Iso7816Error::InsNotSupported, {}, s);
	else if (s.isCLANotSupported()) return PcscError::from(Iso7816Error::ClaNotSupported, {}, s);
	else return PcscError::from(std::monostate{}, {}, s); // Hata durumunda bilinmeyen bir kod ve kind için Success (0) ve std::monostate kullan
}

/*
// -----------------------------
// Helpers: map Kind -> PcscErrorCode (specializations for each enum)
// and map Code -> Kind (for backward compatibility)
// -----------------------------
inline PcscErrorCode mapKindToCode(ConnectionError e)
{
	switch (e)
	{
	case ConnectionError::NotConnected:
		return PcscErrorCode::NotConnected;
	case ConnectionError::ResponseTooShort:
		return PcscErrorCode::ResponseTooShort;
	default:
		break;
	}
	return PcscErrorCode::Unknown;
}
inline PcscErrorCode mapKindToCode(AuthError e)
{
	switch (e)
	{
	case AuthError::AuthRequired:
		return PcscErrorCode::AuthRequired;
	case AuthError::AuthFailed:
		return PcscErrorCode::AuthFailed;
	case AuthError::AuthBlocked:
		return PcscErrorCode::AuthBlocked;
	case AuthError::LoadKeyFailed:
		return PcscErrorCode::LoadKeyFailed;
	default:
		break;
	}
	return PcscErrorCode::Unknown;
}
inline PcscErrorCode mapKindToCode(IoError e)
{
	switch (e)
	{
	case IoError::ReadFailed:
		return PcscErrorCode::ReadFailed;
	case IoError::WriteFailed:
		return PcscErrorCode::WriteFailed;
	default:
		break;
	}
	return PcscErrorCode::Unknown;
}
inline PcscErrorCode mapKindToCode(Iso7816Error e)
{
	switch (e)
	{
	case Iso7816Error::WrongLength:
		return PcscErrorCode::WrongLength;
	case Iso7816Error::SecurityNotSatisfied:
		return PcscErrorCode::SecurityNotSatisfied;
	case Iso7816Error::ConditionsNotSatisfied:
		return PcscErrorCode::ConditionsNotSatisfied;
	case Iso7816Error::FileNotFound:
		return PcscErrorCode::FileNotFound;
	case Iso7816Error::IncorrectParameters:
		return PcscErrorCode::IncorrectParameters;
	case Iso7816Error::InsNotSupported:
		return PcscErrorCode::InsNotSupported;
	case Iso7816Error::ClaNotSupported:
		return PcscErrorCode::ClaNotSupported;
	default:
		break;
	}
	return PcscErrorCode::Unknown;
}
inline PcscErrorCode mapKindToCode(CardError e)
{
	switch (e)
	{
	case CardError::NotDesfire:
		return PcscErrorCode::NotDesfire;
	case CardError::NotAuthenticated:
		return PcscErrorCode::NotAuthenticated;
	case CardError::SessionExpired:
		return PcscErrorCode::SessionExpired;
	case CardError::ManufacturerBlock:
		return PcscErrorCode::ManufacturerBlock;
	case CardError::TrailerBlock:
		return PcscErrorCode::TrailerBlock;
	case CardError::InvalidData:
		return PcscErrorCode::InvalidData;
	default:
		break;
	}
	return PcscErrorCode::Unknown;
}
inline PcscErrorCode mapKindToCode(DesfireError e)
{
	switch (e)
	{
	case DesfireError::Generic:
		return PcscErrorCode::DesfireError;
	case DesfireError::PermissionDenied:
		return PcscErrorCode::DesfirePermissionDenied;
	case DesfireError::AppNotFound:
		return PcscErrorCode::DesfireAppNotFound;
	case DesfireError::FileNotFound:
		return PcscErrorCode::DesfireFileNotFound;
	case DesfireError::AuthMismatch:
		return PcscErrorCode::DesfireAuthMismatch;
	default:
		break;
	}
	return PcscErrorCode::Unknown;
}
inline PcscErrorKind mapCodeToKind(PcscErrorCode c)
{
	switch (c)
	{
	case PcscErrorCode::Success:
		return std::monostate{};
	case PcscErrorCode::NotConnected:
		return ConnectionError::NotConnected;
	case PcscErrorCode::ResponseTooShort:
		return ConnectionError::ResponseTooShort;

	case PcscErrorCode::AuthRequired:
		return AuthError::AuthRequired;
	case PcscErrorCode::AuthFailed:
		return AuthError::AuthFailed;
	case PcscErrorCode::AuthBlocked:
		return AuthError::AuthBlocked;
	case PcscErrorCode::LoadKeyFailed:
		return AuthError::LoadKeyFailed;

	case PcscErrorCode::ReadFailed:
		return IoError::ReadFailed;
	case PcscErrorCode::WriteFailed:
		return IoError::WriteFailed;

	case PcscErrorCode::WrongLength:
		return Iso7816Error::WrongLength;
	case PcscErrorCode::SecurityNotSatisfied:
		return Iso7816Error::SecurityNotSatisfied;
	case PcscErrorCode::ConditionsNotSatisfied:
		return Iso7816Error::ConditionsNotSatisfied;
	case PcscErrorCode::FileNotFound:
		return Iso7816Error::FileNotFound;
	case PcscErrorCode::IncorrectParameters:
		return Iso7816Error::IncorrectParameters;
	case PcscErrorCode::InsNotSupported:
		return Iso7816Error::InsNotSupported;
	case PcscErrorCode::ClaNotSupported:
		return Iso7816Error::ClaNotSupported;

	case PcscErrorCode::NotDesfire:
		return CardError::NotDesfire;
	case PcscErrorCode::NotAuthenticated:
		return CardError::NotAuthenticated;
	case PcscErrorCode::SessionExpired:
		return CardError::SessionExpired;
	case PcscErrorCode::ManufacturerBlock:
		return CardError::ManufacturerBlock;
	case PcscErrorCode::TrailerBlock:
		return CardError::TrailerBlock;
	case PcscErrorCode::InvalidData:
		return CardError::InvalidData;

	case PcscErrorCode::DesfireError:
		return DesfireError::Generic;
	case PcscErrorCode::DesfirePermissionDenied:
		return DesfireError::PermissionDenied;
	case PcscErrorCode::DesfireAppNotFound:
		return DesfireError::AppNotFound;
	case PcscErrorCode::DesfireFileNotFound:
		return DesfireError::FileNotFound;
	case PcscErrorCode::DesfireAuthMismatch:
		return DesfireError::AuthMismatch;

	default:
		return ConnectionError::Unknown;
	}
}




// PcscError: artık sadece variant tabanlı modern API
struct PcscError {
	// iç temsil: hangi kategori hatası olduğu
	PcscErrorKind kind = std::monostate{};

	// opsiyonel ek bilgi
	StatusWord sw{};
	std::string detail;

	// ctor'lar / factory'ler
	PcscError() = default;
	explicit PcscError(PcscErrorKind k, std::string d = {})
		: kind(k)
		, detail(std::move(d))
	{
	}
	PcscError(PcscErrorKind k, const StatusWord& s, std::string d = {})
		: kind(k)
		, sw(s)
		, detail(std::move(d))
	{
	}

	static PcscError from(ConnectionError c, std::string d = {})
	{
		return PcscError{ c, std::move(d) };
	}
	static PcscError from(AuthError c, std::string d = {})
	{
		return PcscError{ c, std::move(d) };
	}
	static PcscError from(IoError c, std::string d = {})
	{
		return PcscError{ c, std::move(d) };
	}
	static PcscError from(Iso7816Error c, const StatusWord& s = {}, std::string d = {})
	{
		return PcscError{ c, s, std::move(d) };
	}
	static PcscError from(CardError c, std::string d = {})
	{
		return PcscError{ c, std::move(d) };
	}
	static PcscError from(DesfireError c, std::string d = {})
	{
		return PcscError{ c, std::move(d) };
	}

	// StatusWord → PcscError (yeni API ile uyumlu)
	static PcscError fromStatusWord(const StatusWord& s)
	{
		if (s.isSuccess())
			return PcscError::from(ConnectionError::Success, {});
		else if (s.isAuthSentinel())
			return PcscError::from(AuthError::AuthRequired, {});
		else if (s.isAuthBlocked())
			return PcscError::from(AuthError::AuthBlocked, {});
		else if (s.isSecurityError())
			return PcscError::from(Iso7816Error::SecurityNotSatisfied, s, {});
		else if (s.isWrongLength())
			return PcscError::from(Iso7816Error::WrongLength, s, {});
		else if (s.isFileNotFound())
			return PcscError::from(Iso7816Error::FileNotFound, s, {});
		else if (s.isIncorrectP1P2())
			return PcscError::from(Iso7816Error::IncorrectParameters, s, {});
		else if (s.isINSNotSupported())
			return PcscError::from(Iso7816Error::InsNotSupported, s, {});
		else if (s.isCLANotSupported())
			return PcscError::from(Iso7816Error::ClaNotSupported, s, {});
		else
			return PcscError{ std::monostate{}, s, {} };
	}

	// durum kontrolü
	bool ok() const noexcept { return std::holds_alternative<std::monostate>(kind); }
	explicit operator bool() const noexcept { return ok(); }

	// message
	std::string message() const
	{
		if (ok()) return "Success";
		std::string base = describeKind(kind);
		if (!detail.empty()) base += ": " + detail;
		if (sw.sw1 != 0 || sw.sw2 != 0)
		{
			base += " [" + sw.toHexFormatted();
			std::string swmsg = sw.getMessage();
			if (!swmsg.empty()) base += ": " + swmsg;
			base += "]";
		}
		return base;
	}

	// exception bridge (basit, kategoriye göre tür seçimi)
	void throwIfError() const
	{
		if (ok()) return;
		std::string msg = message();

		std::visit([&](auto&& v)
		           {
			using V = std::decay_t<decltype(v)>;
			if constexpr (std::is_same_v<V, std::monostate>) {
				// success zaten handled
			} else if constexpr (std::is_same_v<V, ConnectionError>) {
				throw std::runtime_error(msg);
			} else if constexpr (std::is_same_v<V, AuthError>) {
				throw std::runtime_error(msg);
			} else if constexpr (std::is_same_v<V, IoError>) {
				throw std::runtime_error(msg);
			} else if constexpr (std::is_same_v<V, Iso7816Error>) {
				throw std::runtime_error(msg);
			} else if constexpr (std::is_same_v<V, CardError>) {
				switch (v) {
				case CardError::NotDesfire:
				case CardError::ManufacturerBlock:
				case CardError::TrailerBlock:
					throw std::logic_error(msg);
				case CardError::InvalidData:
					throw std::invalid_argument(msg);
				default:
					throw std::runtime_error(msg);
				}
			} else if constexpr (std::is_same_v<V, DesfireError>) {
				throw std::runtime_error(msg);
			} else {
				throw std::runtime_error(msg);
			}
		}, kind);
	}
};
*/

#endif // PCSC_ERROR_H
