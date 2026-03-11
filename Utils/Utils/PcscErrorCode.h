#ifndef PCSC_ERROR_CODE_H
#define PCSC_ERROR_CODE_H
#include <cstdint>
#include <string>
#include <variant>

// -----------------------------
// Global "legacy" PcscErrorCode (you already have this — kept for compatibility)
// -----------------------------
enum class PcscErrorCode : uint8_t
{
	Success = 0,

	// Connection
	NotConnected,
	ResponseTooShort,

	// Auth
	AuthRequired,
	AuthFailed,
	AuthBlocked,
	LoadKeyFailed,

	// I/O
	ReadFailed,
	WriteFailed,

	// ISO7816 / Protocol
	WrongLength,
	SecurityNotSatisfied,
	ConditionsNotSatisfied,
	FileNotFound,
	IncorrectParameters,
	InsNotSupported,
	ClaNotSupported,

	// Card level
	NotDesfire,
	NotAuthenticated,
	SessionExpired,
	ManufacturerBlock,
	TrailerBlock,
	InvalidData,

	// DESFire specific
	DesfireError,
	DesfirePermissionDenied,
	DesfireAppNotFound,
	DesfireFileNotFound,
	DesfireAuthMismatch,

	Unknown = static_cast<uint8_t>(~0)
};

inline std::string describe(PcscErrorCode code)
{
	switch (code)
	{
	case PcscErrorCode::Success:
		return "Success";
	case PcscErrorCode::NotConnected:
		return "Not connected";
	case PcscErrorCode::ResponseTooShort:
		return "Response too short";
	case PcscErrorCode::AuthRequired:
		return "Authentication required";
	case PcscErrorCode::AuthFailed:
		return "Authentication failed";
	case PcscErrorCode::AuthBlocked:
		return "Authentication blocked";
	case PcscErrorCode::LoadKeyFailed:
		return "Load key failed";
	case PcscErrorCode::ReadFailed:
		return "Read failed";
	case PcscErrorCode::WriteFailed:
		return "Write failed";
	case PcscErrorCode::WrongLength:
		return "Wrong length";
	case PcscErrorCode::SecurityNotSatisfied:
		return "Security status not satisfied";
	case PcscErrorCode::ConditionsNotSatisfied:
		return "Conditions of use not satisfied";
	case PcscErrorCode::FileNotFound:
		return "File not found";
	case PcscErrorCode::IncorrectParameters:
		return "Incorrect parameters";
	case PcscErrorCode::InsNotSupported:
		return "INS not supported";
	case PcscErrorCode::ClaNotSupported:
		return "CLA not supported";
	case PcscErrorCode::NotDesfire:
		return "Not a DESFire card";
	case PcscErrorCode::NotAuthenticated:
		return "Not authenticated";
	case PcscErrorCode::SessionExpired:
		return "Session expired";
	case PcscErrorCode::ManufacturerBlock:
		return "Manufacturer block protected";
	case PcscErrorCode::TrailerBlock:
		return "Trailer block protected";
	case PcscErrorCode::InvalidData:
		return "Invalid data";
	case PcscErrorCode::DesfireError:
		return "DESFire error";
	case PcscErrorCode::DesfirePermissionDenied:
		return "DESFire permission denied";
	case PcscErrorCode::DesfireAppNotFound:
		return "DESFire application not found";
	case PcscErrorCode::DesfireFileNotFound:
		return "DESFire file not found";
	case PcscErrorCode::DesfireAuthMismatch:
		return "DESFire auth verification failed";
	default:
		return "Unknown error";
	}
}

enum class ConnectionError : uint8_t
{
	Success = 0,
	NotConnected,
	ResponseTooShort,
	Unknown = static_cast<uint8_t>(~0)
};
enum class AuthError : uint8_t
{
	Success = 0,
	AuthRequired,
	AuthFailed,
	AuthBlocked,
	LoadKeyFailed,
	Unknown = static_cast<uint8_t>(~0)
};
enum class IoError : uint8_t
{
	Success = 0,
	ReadFailed,
	WriteFailed,
	Unknown = static_cast<uint8_t>(~0)
};
enum class Iso7816Error : uint8_t
{
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
enum class CardError : uint8_t
{
	Success = 0,
	NotDesfire,
	NotAuthenticated,
	SessionExpired,
	ManufacturerBlock,
	TrailerBlock,
	InvalidData,
	Unknown = static_cast<uint8_t>(~0)
};
enum class DesfireError : uint8_t
{
	Success = 0,
	Generic,
	PermissionDenied,
	AppNotFound,
	FileNotFound,
	AuthMismatch,
	Unknown = static_cast<uint8_t>(~0)
};

using PcscErrorKind = std::variant<
    std::monostate, // success
    ConnectionError,
    AuthError,
    IoError,
    Iso7816Error,
    CardError,
    DesfireError>;

// PcscError alias is defined in PcscError.h to avoid cyclic include dependencies

// forward declarations for mapping functions implemented in PcscError.h
inline PcscErrorCode mapKindToCode(ConnectionError e);
inline PcscErrorCode mapKindToCode(AuthError e);
inline PcscErrorCode mapKindToCode(IoError e);
inline PcscErrorCode mapKindToCode(Iso7816Error e);
inline PcscErrorCode mapKindToCode(CardError e);
inline PcscErrorCode mapKindToCode(DesfireError e);


// forward declaration for describeKind implemented in PcscError.h
std::string describeKind(const PcscErrorKind& k, PcscErrorCode fallback);

#endif // !PCSC_ERROR_CODE_H
