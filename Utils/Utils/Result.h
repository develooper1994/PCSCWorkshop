#ifndef PCSC_RESULT_H
#define PCSC_RESULT_H

#include "StatusWordHandler.h"
#include "PcscError.h"
#include "ErrorBuilder.h"
#include "BasicError.h"
#include <string>
#include <variant>
#include <cstdint>
#include <stdexcept>
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

// -----------------------------
// Result<T,E> generic and void specialization ([[nodiscard]])
// Works with BasicError that is copyable (deep-copy implemented above).
// -----------------------------
template<typename T, typename E>
class [[nodiscard]] Result {
	static_assert(!std::is_void<T>::value, "Use Result<void,E> specialization for void");

public:
	using value_type = T;
	using error_type = E;

private:
	std::variant<T, E> data;

public:
	Result() = delete;
	Result(const Result&) = default;
	Result(Result&&) = default;
	Result& operator=(const Result&) = default;
	Result& operator=(Result&&) = default;
	~Result() = default;

	explicit Result(const T& v): data(std::in_place_type<T>, v){}
	explicit Result(T&& v): data(std::in_place_type<T>, std::move(v)){}
	explicit Result(const E& e): data(std::in_place_type<E>, e){}
	explicit Result(E&& e): data(std::in_place_type<E>, std::move(e)){}

	// factory helpers
	static Result Ok(T v) { return Result(std::move(v)); }
	static Result Err(E e) { return Result(std::move(e)); }

	bool is_ok() const noexcept { return std::holds_alternative<T>(data); }
	explicit operator bool() const noexcept { return is_ok(); }

	T& unwrap() {
		if (!is_ok()) std::get<E>(data).throwIfError();
		return std::get<T>(data);
	}
	const T& unwrap() const {
		if (!is_ok()) std::get<E>(data).throwIfError();
		return std::get<T>(data);
	}

	E& unwrap_error() {
		if (is_ok()) throw std::runtime_error("Tried to unwrap_error on ok value");
		return error();
	}
	const E& unwrap_error() const {
		if (is_ok()) throw std::runtime_error("Tried to unwrap_error on ok value");
		return error();
	}

	// access error (const & non-const)
	E& error() { return std::get<E>(data); }
	const E& error() const { return std::get<E>(data); }

	// map: T -> U
	template<typename F>
	auto map(F&& f) const {
		using U = std::decay_t<decltype(f(std::declval<T>()))>;
		return is_ok() ? Result<U, E>::Ok(f(std::get<T>(data))) : Result<U, E>::Err(std::get<E>(data));
	}

	// map_err: E -> E2
	template<typename F>
	auto map_err(F&& f) const {
		using E2 = std::decay_t<decltype(f(std::declval<E>()))>;
		return is_ok() ? Result<T, E2>::Ok(std::get<T>(data)) : Result<T, E2>::Err(f(std::get<E>(data)));
	}

	// and_then: T -> Result<U,E>
	template<typename F>
	auto and_then(F&& f) const {
		using R = std::decay_t<decltype(f(std::declval<T>()))>; // R must be Result<U,E>
		return is_ok() ? f(std::get<T>(data)) : R::Err(std::get<E>(data));
	}

	// Fallbacks
	template<typename U>
	T unwrap_or(U&& default_value) const {
		return is_ok() ? std::get<T>(data) : static_cast<T>(std::forward<U>(default_value));
	}

	template<typename F>
	T unwrap_or_else(F&& f) const {
		return is_ok() ? std::get<T>(data) : f(std::get<E>(data));
	}

	template<typename F>
	auto or_else(F&& f) const {
		using R = std::decay_t<decltype(f(std::declval<E>()))>;
		return is_ok() ? Result<T, R>::Ok(std::get<T>(data)) : f(std::get<E>(data));
	}

	void expect(const char* msg) const {
		if (!is_ok()) {
			std::cerr << "Result::expect failed: " << msg << "\n";
			std::get<E>(data).throwIfError();
		}
	}
};

// void specialization
template<typename E>
class [[nodiscard]] Result<void, E>
{
public:
	using value_type = void;
	using error_type = E;

private:
	std::variant<std::monostate, E> data; // monostate => ok

public:
	Result() = default;
	Result(const Result&) = default;
	Result(Result&&) = default;
	Result& operator=(const Result&) = default;
	Result& operator=(Result&&) = default;
	~Result() = default;

	explicit Result(const E& e): data(std::in_place_type<E>, e){}
	explicit Result(E&& e): data(std::in_place_type<E>, std::move(e)){}

	// construct from Result<U,E> to propagate errors
	template<typename U>
	Result(const Result<U, E>& other) { if (!other.is_ok()) data = other.error(); }

	static Result Ok() { return Result(); }
	static Result Err(E e) { return Result(std::move(e)); }

	bool is_ok() const noexcept { return std::holds_alternative<std::monostate>(data); }
	explicit operator bool() const noexcept { return is_ok(); }

	void unwrap() const
	{
		if (!is_ok()) std::get<E>(data).throwIfError();
	}

	E& error()
	{
		return std::get<E>(data);
	}

	const E& error() const
	{
		return std::get<E>(data);
	}

	template<typename F>
	auto and_then(F&& f) const
	{
		using R = std::decay_t<decltype(f())>;
		return is_ok() ? f() : R(error());
	}

	template<typename F>
	auto or_else(F&& f) const
	{
		using R = std::decay_t<decltype(f(std::declval<E>()))>;
		return is_ok() ? Result<void, R>::Ok() : f(std::get<E>(data));
	}

	void expect(const char* msg) const
	{
		if (!is_ok())
		{
			std::cerr << "Result<void,E>::expect failed: " << msg << "\n";
			std::get<E>(data).throwIfError();
		}
	}
};

// -----------------------------
// Portable TRY / PROPAGATE helpers (macro-based but portable to MSVC/GCC/Clang).
// Two helpers:
//  - PROPAGATE_IF_ERR(expr)       => calls expr, if it is error returns Err(...) from current function
//  - TRY_ASSIGN(var, expr)        => evaluates expr, if ok assigns unwrapped value to 'var', otherwise returns Err(...)
// Usage:
//   PROPAGATE_IF_ERR(someFunc());              // for Result<void,E> or to just propagate
//   TRY_ASSIGN(value, someFuncReturningResult());
// -----------------------------
#define CONCAT_INTERNAL(x, y) x##y
#define CONCAT(x, y) CONCAT_INTERNAL(x, y)

#define PROPAGATE_IF_ERR(EXPR)                                                                            \
	do {                                                                                                  \
		auto CONCAT(_res_, __LINE__) = (EXPR);                                                            \
		if (!CONCAT(_res_, __LINE__))                                                                     \
			return std::decay_t<decltype(CONCAT(_res_, __LINE__))>::Err(CONCAT(_res_, __LINE__).error()); \
	} while (0)

#define TRY_ASSIGN(VAR, EXPR)                                                                           \
	auto CONCAT(_res2_, __LINE__) = (EXPR);                                                             \
	if (!CONCAT(_res2_, __LINE__))                                                                      \
		return std::decay_t<decltype(CONCAT(_res2_, __LINE__))>::Err(CONCAT(_res2_, __LINE__).error()); \
	auto VAR = CONCAT(_res2_, __LINE__).unwrap();

// TRY() helper - use PROPAGATE_IF_ERR or TRY_ASSIGN macros instead (cross-platform)
// GCC/Clang version (not used in production code - kept for reference only)
// #define TRY(expr) ({ auto _r = (expr); if(!_r) return decltype(_r)::Err(_r.error()); _r.unwrap(); })

template<typename E>
using ResultVoid = Result<void,E>;
using PcscResultVoid = ResultVoid<PcscError>;

template<typename T>
using PcscResult = Result<T, PcscError>;

using PcscResultString = PcscResult<std::string>;
using PcscResultByteV = PcscResult<BYTEV>;
using PcscResultStatusWord = PcscResult<StatusWord>;
using PcscResultGeneric = PcscResult<PcscErrorKind>;


#endif // PCSC_RESULT_H
