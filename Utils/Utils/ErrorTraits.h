#ifndef ERROR_TRAITS_H
#define ERROR_TRAITS_H

#include "PcscErrorCode.h"
#include <type_traits>

// ---- helper: dependent_false (static_assert kullanmak istersen) ----
template<typename>
inline constexpr bool dependent_false_v = false;

// ---- default trait: kind -> code (genel fallback) ----
template<typename Kind, typename CodeT>
struct kind_to_code
{
	// default: dönüşümü desteklemiyor; güvenli fallback ver
	static CodeT map(Kind) noexcept
	{
		// i) fallback: CodeT::Unknown gibi bir değer dön
		using UT = std::underlying_type_t<CodeT>;
		return static_cast<CodeT>(static_cast<UT>(0)); // default 0 (Success) veya override et
		                                               // ii) istersen burada derleme hatası fırlat:
		                                               // static_assert(dependent_false_v<Kind>, "kind_to_code not specialized for this Kind");
	}
};

// Örnek: ConnectionError -> PcscErrorCode
template<>
struct kind_to_code<ConnectionError, PcscErrorCode>
{
	static PcscErrorCode map(ConnectionError k) noexcept
	{
		switch (k)
		{
		case ConnectionError::NotConnected:
			return PcscErrorCode::NotConnected;
		case ConnectionError::ResponseTooShort:
			return PcscErrorCode::ResponseTooShort;
		default:
			return PcscErrorCode::Unknown;
		}
	}
};

// Örnek: AuthError -> PcscErrorCode
template<>
struct kind_to_code<AuthError, PcscErrorCode>
{
	static PcscErrorCode map(AuthError k) noexcept
	{
		switch (k)
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
			return PcscErrorCode::Unknown;
		}
	}
};

// ---- default trait: code -> kindvariant (genel fallback) ----
template<typename CodeT, typename KindVariantT>
struct code_to_kind
{
	static KindVariantT map(CodeT) noexcept { return KindVariantT{}; }
};

// Örnek: PcscErrorCode -> PcscErrorKind
template<>
struct code_to_kind<PcscErrorCode, PcscErrorKind>
{
	static PcscErrorKind map(PcscErrorCode c) noexcept
	{
		switch (c)
		{
		case PcscErrorCode::NotConnected:
			return ConnectionError::NotConnected;
		case PcscErrorCode::ResponseTooShort:
			return ConnectionError::ResponseTooShort;
		case PcscErrorCode::AuthRequired:
			return AuthError::AuthRequired;
		case PcscErrorCode::AuthFailed:
			return AuthError::AuthFailed;
		// ... diğerleri ...
		case PcscErrorCode::Success:
			return std::monostate{};
		default:
			return AuthError::Unknown; // veya std::monostate/Unknown alternatif
		}
	}
};

#endif // !ERROR_TRAITS_H
