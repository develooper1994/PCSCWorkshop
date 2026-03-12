#ifndef PCSC_RESULT_H
#define PCSC_RESULT_H

#include "StatusWordHandler.h"
#include "PcscError.h"
#include "BasicError.h"
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

// -----------------------------
// Result<T, E> generic and void specialization
// -----------------------------
template<typename T, typename E>
class [[nodiscard]]Result {
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

	explicit Result(const T& v)
	    : data(std::in_place_type_t<T>{}, v)
	{
	}

	explicit Result(T&& v)
	    : data(std::in_place_type_t<T>{}, std::move(v))
	{
	}

	explicit Result(const E& e)
	    : data(std::in_place_type_t<E>{}, e)
	{
	}

	explicit Result(E&& e)
	    : data(std::in_place_type_t<E>{}, std::move(e))
	{
	}

	// Result(T&& v, E&& e): data(std::move(v)){}
	// Result(const T& v, const E& e): data(std::move(v)){}

	// factory helpers
	static Result Ok(T v) { return Result(std::move(v)); }
	static Result Err(E e) { return Result(std::move(e)); }

    bool is_ok() const noexcept { return std::holds_alternative<T>(data); }
    explicit operator bool() const noexcept { return is_ok(); }

	T& unwrap() {
		if (!is_ok()) std::get<E>(data).throwIfError(); // throw std::logic_error("unwrap() on error");
		return std::get<T>(data);
	}
	const T& unwrap() const {
		if (!is_ok()) std::get<E>(data).throwIfError(); // throw std::logic_error("unwrap() on error");
		return std::get<T>(data);
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
	T unwrap_or(U&& default_value) const
	{
		return is_ok() ? std::get<T>(data) : static_cast<T>(std::forward<U>(default_value));
	}

	template<typename F>
	T unwrap_or_else(F&& f) const
	{
		return is_ok() ? std::get<T>(data) : f(std::get<E>(data));
	}

	template<typename F>
	auto or_else(F&& f) const
	{
		using R = std::decay_t<decltype(f(std::declval<E>()))>;
		return is_ok() ? Result<T, R>::Ok(std::get<T>(data)) : f(std::get<E>(data));
	}

	void expect(const char* msg) const
	{
		if (!is_ok())
		{
			std::cerr << "Result::expect failed: " << msg << "\n";
			std::get<E>(data).throwIfError();
		}
	}
};

// void işlemler için (özelleştirilmiş PcscError) — kullanım aynıdır
template<typename E>
class Result<void, E> {
public:
	using value_type = void;
	using error_type = E;

private:
	std::variant<std::monostate, E> data; // empty => ok

public:
	Result() = default;
	Result(const Result&) = default;
	Result(Result&&) = default;
	Result& operator=(const Result&) = default;
	Result& operator=(Result&&) = default;
	~Result() = default;

    explicit Result(const E& e)
	    : data(std::in_place_type_t<E>{}, e)
	{
	}

	explicit Result(E&& e)
	    : data(std::in_place_type_t<E>{}, std::move(e))
	{
	}

	// allow construction from Result<U,E> to propagate errors
	// propagate error from Result<U,E>
	template<typename U>
	Result(const Result<U, E>& other)
	{
		if (!other.is_ok()) data = other.error(); // assigns E alternative
	}


	static Result Ok() { return Result(); }
	static Result Err(E e) { return Result(std::move(e)); }

    bool is_ok() const noexcept { return std::holds_alternative<std::monostate>(data); }
    explicit operator bool() const noexcept { return is_ok(); }

	void unwrap() const {
		if (!is_ok()) std::get<E>(data).throwIfError();
	}

	E& error() {
		return std::get<E>(data);
	}

	const E& error() const {
		return std::get<E>(data);
	}

	template<typename F>
	auto and_then(F&& f) const {
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

// #define TRY(expr) ({ auto _r = (expr); if(!_r) return decltype(_r)::Err(_r.error()); _r.unwrap(); }) // GCC/CLANG specific
template<typename R>
constexpr auto TRY(R&& result) {
	return result ? result.unwrap() : typename std::decay_t<R>::template Err(result.error()); // Başarısızsa aynı tipin Err'ini döndür
}

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
