#ifndef BASIC_ERROR_H
#define BASIC_ERROR_H

#include <type_traits>
#include <variant>

// -----------------------------
// BasicError: generic error container
// Usage: using PcscError = BasicError<PcscErrorKind>;
// -----------------------------
template<typename KindVariantT>
struct BasicError {
    // using code_type = CodeT;
    using kind_type = KindVariantT;

    // CodeT code{};                   // legacy / numeric code (optional)
    KindVariantT kind{};            // type-safe variant
    StatusWord sw{};                // optional protocol sw
    std::string detail{};           // extra info
	std::unique_ptr<BasicError<KindVariantT>> cause;

    BasicError() = default;

	void setCause(BasicError e) { cause = std::make_unique<BasicError>(std::move(e)); }

    // Construct from code (legacy)
	template<typename K>
    static BasicError make(K k, std::string d = {}, StatusWord s = {}) {
        BasicError e;
        e.kind = k; // mapCodeToKind(c); // requires mapCodeToKind(CodeT) free function
        e.detail = std::move(d);
        e.sw = s;
        return e;
    }

    // Construct from kind directly
    template<typename K>
    static BasicError from(K k, std::string d = {}, StatusWord s = {}) {
        BasicError e;
		e.kind = k; // KindVariantT(code_to_kind<std::decay_t<K>, KindVariantT>::map(k)); // code_to_kind<std::decay_t<K>, KindVariantT>::map(k);
        e.detail = std::move(d);
        e.sw = s;
        return e;
    }
    bool is_ok() const noexcept {
        // treat monostate / Success as ok
        using MS = std::monostate;
        if constexpr (std::is_same_v<KindVariantT, std::variant<MS>>) {
            // degenerate case (unlikely)
            return true;
        }
        return std::holds_alternative<MS>(kind);
    }

    explicit operator bool() const noexcept { return is_ok(); }

    std::string message() const {
        if (is_ok()) return "Success";
        std::string base = describeKind(kind);
        if (!detail.empty()) base += ": " + detail;
        if (sw.sw1 != 0 || sw.sw2 != 0) {
            base += " [" + sw.toHexFormatted();
            std::string swmsg = sw.getMessage();
            if (!swmsg.empty()) base += ": " + swmsg;
            base += "]";
        }
        return base;
    }

    void throwIfError() const {
        if (is_ok()) return;
        // default behavior: throw runtime_error (you can customize)
        throw std::runtime_error(message());
    }

private:
    // mapCodeToKind(CodeT) must be available in same namespace; we used free function earlier.
    // mapKindToCode(K) overloads are provided earlier for your enums.
};

#endif // !BASIC_ERROR_H
