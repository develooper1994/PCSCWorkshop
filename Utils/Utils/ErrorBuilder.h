#ifndef ERROR_BUILDER_H
#define ERROR_BUILDER_H

#include "MetaTypes.h"

template<typename ErrorT>
using ErrorBuilderKind = typename ErrorT::kind_type;

// ---------------------------------------------------------
// ErrorBuilder: fluent construction helper
// Works with BasicError-like types.
// Requirement:
//   ErrorT must have:
//     using kind_type = ...
//     member: kind
//     member: detail
//     member: meta
//     member: cause
// ---------------------------------------------------------
template<typename ErrorT>
class ErrorBuilder
{
public:
	using Kind = ErrorBuilderKind<ErrorT>;

	explicit ErrorBuilder(Kind k)
	{
		err.kind = std::move(k);
	}

	// Return &&, not & — chain stays an rvalue
	ErrorBuilder&& detail(std::string d) &&
	{
		err.detail = std::move(d);
		return std::move(*this);
	}

	template<typename T>
	ErrorBuilder&& meta(std::string key, T&& value) &&
	{
		err.meta.emplace(std::move(key),
		                 MetaTypes::MetaValue{std::forward<T>(value)});
		return std::move(*this);
	}

	ErrorBuilder&& because(ErrorT e) &&
	{
		err.cause = std::make_unique<ErrorT>(std::move(e));
		return std::move(*this);
	}

	ErrorT build() &&
	{
		return std::move(err);
	}

	// Now reliably fires — chain end is always &&
	operator ErrorT() &&
	{
		return std::move(err);
	}

private:
	ErrorT err{};
};

// helper factory
template<typename ErrorT, typename K>
ErrorBuilder<ErrorT> Error(K&& k)
{
	using Kind = typename ErrorT::kind_type;
	return ErrorBuilder<ErrorT>(Kind{std::forward<K>(k)});
}



#endif // !ERROR_BUILDER_H
