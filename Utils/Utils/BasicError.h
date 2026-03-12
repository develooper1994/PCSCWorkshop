#ifndef BASIC_ERROR_H
#define BASIC_ERROR_H

#include "MetaTypes.h"
#include <string>
#include <memory>

// ---------------------------------------------------------
// BasicError: generic error container
// Usage: using PcscError = BasicError<PcscErrorKind>;
// ---------------------------------------------------------
template<typename KindVariantT>
struct BasicError
{
	using kind_type = KindVariantT;
	using self_type = BasicError<KindVariantT>;

	KindVariantT kind{};                // type-safe variant
	std::string detail{};               // extra info
	MetaTypes::MetaMap meta{};          // generic metadata
	std::unique_ptr<self_type> cause{}; // error chain

	BasicError() = default;

	// Deep-copy ctor
	BasicError(const BasicError& other)
		: kind(other.kind)
		, detail(other.detail)
		, meta(other.meta)
	{
		if (other.cause)
			cause = std::make_unique<self_type>(*other.cause);
	}

	// Deep-copy assignment
	BasicError& operator=(const BasicError& other)
	{
		if (this == &other) return *this;

		kind = other.kind;
		detail = other.detail;
		meta = other.meta;

		if (other.cause)
			cause = std::make_unique<self_type>(*other.cause);
		else
			cause.reset();

		return *this;
	}

	BasicError(BasicError&&) noexcept = default;
	BasicError& operator=(BasicError&&) noexcept = default;

	// ----- Helper methods for building error chains -----
	void setCause(self_type e)
	{
		cause = std::make_unique<self_type>(std::move(e));
	}

	template<typename K>
	static self_type make(K&& k, std::string d = {})
	{
		self_type e;
		e.kind = std::forward<K>(k);
		e.detail = std::move(d);
		return e;
	}

	bool is_ok() const noexcept
	{
		using MS = std::monostate;
		if constexpr (std::is_same_v<KindVariantT, std::variant<MS>>)
			return true;

		return std::holds_alternative<MS>(kind);
	}

	explicit operator bool() const noexcept
	{
		return is_ok();
	}

	// Sizde describeKind(...) olmalı:
	// - ya aynı namespace'te overload
	// - ya da ADL ile bulunabilir bir free function
	std::string message() const
	{
		if (is_ok()) return "Success";

		std::string base = describeKind(kind);
		if (!detail.empty())
			base += ": " + detail;

		if (!meta.empty())
		{
			base += " {";
			bool first = true;

			for (auto const& [k, v] : meta)
			{
				if (!first) base += ", ";
				base += k + "=";

				std::visit([&base](auto&& val)
						   {
					using V = std::decay_t<decltype(val)>;

					if constexpr (std::is_same_v<V, std::string>) {
						base += val;
					} else if constexpr (std::is_same_v<V, int64_t> || std::is_same_v<V, uint64_t>) {
						base += std::to_string(val);
					} else if constexpr (std::is_same_v<V, StatusWord>) {
						base += val.toHexFormatted();
					} else {
						base += "<meta>";
					} }, v);

				first = false;
			}

			base += "}";
		}

		if (cause)
			base += "\n  Caused by: " + cause->message();

		return base;
	}

	void throwIfError() const
	{
		if (!is_ok())
			throw std::runtime_error(message());
	}
};

#endif // !BASIC_ERROR_H
