#ifndef BASIC_ERROR_H
#define BASIC_ERROR_H

#include "ErrorTraits.h"
#include <type_traits>
#include <variant>

// -----------------------------
// BasicError: generic error container
// Usage: using PcscError = BasicError<PcscErrorCode, PcscErrorKind>;
// -----------------------------
template<typename CodeT, typename KindVariantT>
struct BasicError {
    using code_type = CodeT;
    using kind_type = KindVariantT;

    CodeT code{};                   // legacy / numeric code (optional)
    KindVariantT kind{};            // type-safe variant
    StatusWord sw{};                // optional protocol sw
    std::string detail{};           // extra info

    BasicError() = default;

    // Construct from code (legacy)
    static BasicError make(CodeT c, std::string d = {}, StatusWord s = {}) {
        BasicError e;
        e.code = c;
        e.kind = mapCodeToKind(c); // requires mapCodeToKind(CodeT) free function
        e.detail = std::move(d);
        e.sw = s;
        return e;
    }

    // Construct from kind directly
    template<typename K>
    static BasicError from(K k, std::string d = {}, StatusWord s = {}) {
        BasicError e;
		e.kind = KindVariantT(code_to_kind<std::decay_t<K>, KindVariantT>::map(k)); // code_to_kind<std::decay_t<K>, KindVariantT>::map(k);
        e.detail = std::move(d);
        e.sw = s;
        // try to map variant-alternative -> code using mapKindToCode(K)
		e.code = kind_to_code<K, CodeT>::map(k);
        return e;
    }
    bool ok() const noexcept {
        // treat monostate / Success as ok
        using MS = std::monostate;
        if constexpr (std::is_same_v<KindVariantT, std::variant<MS>>) {
            // degenerate case (unlikely)
            return true;
        }
        return std::holds_alternative<MS>(kind)
               || code == static_cast<CodeT>(0); // CodeT::Success == 0 expected
    }

    explicit operator bool() const noexcept { return ok(); }

    std::string message() const {
        if (ok()) return "Success";
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

    void throwIfError() const {
        if (ok()) return;
        // default behavior: throw runtime_error (you can customize)
        throw std::runtime_error(message());
    }

private:
    // mapCodeToKind(CodeT) must be available in same namespace; we used free function earlier.
    // mapKindToCode(K) overloads are provided earlier for your enums.
};

#endif // !BASIC_ERROR_H
