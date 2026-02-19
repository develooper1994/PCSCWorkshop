#ifndef PCSC_WORKSHOP1_CIPHER_H
#define PCSC_WORKSHOP1_CIPHER_H

#include <vector>
#include <memory>
#include <iterator>
#include <type_traits>
#include <cstddef>
#include <algorithm>
#include <string>

#include "CipherTypes.h"

// Forward declare ICipherAAD so we can reference it in implementation file
class ICipherAAD;

// ============================================================
// ICipher — þifreleme stratejisi arayüzü
// ============================================================
class ICipher {
public:
    virtual ~ICipher() = default;

    // Veriyi þifrele — out buffer'ý en az len byte olmalý
    virtual BYTEV encrypt(const BYTE* data, size_t len) const = 0;

    // Þifreli veriyi çöz — out buffer'ý en az len byte olmalý
    virtual BYTEV decrypt(const BYTE* data, size_t len) const = 0;

    // Convenience overloads for common container types
    BYTEV encrypt(const BYTEV& data) const { return encrypt(data.data(), data.size()); }
    BYTEV decrypt(const BYTEV& data) const { return decrypt(data.data(), data.size()); }
    BYTEV encrypt(const std::string& s) const { return encrypt(reinterpret_cast<const BYTE*>(s.data()), s.size()); }
    BYTEV decrypt(const std::string& s) const { return decrypt(reinterpret_cast<const BYTE*>(s.data()), s.size()); }

    // Backwards-compatible output-into virtuals: cipher implementasyonlarý optimize edebilir
    // Default implementation uses old virtuals and copies result into out only when sizes match.
    virtual void encryptInto(const BYTE* data, size_t len, BYTE* out) const {
        BYTEV tmp = encrypt(data, len);
        if (tmp.size() == len && out) std::copy(tmp.begin(), tmp.end(), out);
    }
    virtual void decryptInto(const BYTE* data, size_t len, BYTE* out) const {
        BYTEV tmp = decrypt(data, len);
        if (tmp.size() == len && out) std::copy(tmp.begin(), tmp.end(), out);
    }

    // New sized-output virtuals returning the logical output size. Implementations should write
    // up to outCapacity bytes into 'out' and return the total number of bytes of the encrypted/decrypted result.
    // Default implementation uses encrypt()/decrypt() and copies up to outCapacity bytes.
    virtual size_t encryptIntoSized(const BYTE* data, size_t len, BYTE* out, size_t outCapacity) const {
        BYTEV tmp = encrypt(data, len);
        if (out && outCapacity > 0) {
            size_t toCopy = std::min<size_t>(tmp.size(), outCapacity);
            std::copy_n(tmp.begin(), toCopy, out);
        }
        return tmp.size();
    }
    // convenience overload assumes outCapacity == len
    virtual size_t encryptIntoSized(const BYTE* data, size_t len, BYTE* out) const {
        return encryptIntoSized(data, len, out, len);
    }

    virtual size_t decryptIntoSized(const BYTE* data, size_t len, BYTE* out, size_t outCapacity) const {
        BYTEV tmp = decrypt(data, len);
        if (out && outCapacity > 0) {
            size_t toCopy = std::min<size_t>(tmp.size(), outCapacity);
            std::copy_n(tmp.begin(), toCopy, out);
        }
        return tmp.size();
    }
    virtual size_t decryptIntoSized(const BYTE* data, size_t len, BYTE* out) const {
        return decryptIntoSized(data, len, out, len);
    }

    // Iterator-range overloads for arbitrary sequences (e.g. std::list)
    template<typename InputIt>
    BYTEV encrypt(InputIt first, InputIt last) const {
        using diff_t = typename std::iterator_traits<InputIt>::difference_type;
        diff_t n = std::distance(first, last);
        BYTEV in;
        in.reserve(static_cast<size_t>(n));
        for (; first != last; ++first) in.push_back(static_cast<BYTE>(*first));
        // fallback to encrypt which may expand the size
        return encrypt(in.data(), in.size());
    }

    template<typename InputIt>
    BYTEV decrypt(InputIt first, InputIt last) const {
        using diff_t = typename std::iterator_traits<InputIt>::difference_type;
        diff_t n = std::distance(first, last);
        BYTEV in;
        in.reserve(static_cast<size_t>(n));
        for (; first != last; ++first) in.push_back(static_cast<BYTE>(*first));
        return decrypt(in.data(), in.size());
    }

    // AAD-aware overloads: declared here, default implementation in CipherAadFallback.cpp
    virtual BYTEV encrypt(const BYTE* data, size_t len, const BYTE* aad, size_t aad_len) const;
    virtual BYTEV decrypt(const BYTE* data, size_t len, const BYTE* aad, size_t aad_len) const;

    // Convenience AAD overload for vector
    BYTEV encrypt(const BYTEV& data, const BYTEV& aad) const { return encrypt(data.data(), data.size(), aad.data(), aad.size()); }
    BYTEV decrypt(const BYTEV& data, const BYTEV& aad) const { return decrypt(data.data(), data.size(), aad.data(), aad.size()); }

    // Each cipher implements its own self-test
    virtual bool test() const = 0;
};

#endif // PCSC_WORKSHOP1_CIPHER_H
