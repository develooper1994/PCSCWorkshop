#ifndef PCSC_WORKSHOP1_CIPHER_H
#define PCSC_WORKSHOP1_CIPHER_H

#include <vector>
#include <memory>
#include <iterator>

typedef unsigned char       BYTE;
using BYTEV = std::vector<BYTE>;

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

    // Output-into virtuals: cipher implementasyonlarý optimize edebilir
    // Default implementation uses old virtuals and copies result into out.
    virtual void encryptInto(const BYTE* data, size_t len, BYTE* out) const {
        BYTEV tmp = encrypt(data, len);
        if (tmp.size() == len && out) std::copy(tmp.begin(), tmp.end(), out);
    }
    virtual void decryptInto(const BYTE* data, size_t len, BYTE* out) const {
        BYTEV tmp = decrypt(data, len);
        if (tmp.size() == len && out) std::copy(tmp.begin(), tmp.end(), out);
    }

    // Generic contiguous container overloads (vector, array, string, C-array via wrapper)
    template<typename Container>
    BYTEV encrypt(const Container& c) const {
        return encrypt(reinterpret_cast<const BYTE*>(c.data()), c.size());
    }
    template<typename Container>
    BYTEV decrypt(const Container& c) const {
        return decrypt(reinterpret_cast<const BYTE*>(c.data()), c.size());
    }

    // Iterator-range overloads for arbitrary sequences (e.g. std::list)
    template<typename InputIt>
    BYTEV encrypt(InputIt first, InputIt last) const {
        using diff_t = typename std::iterator_traits<InputIt>::difference_type;
        diff_t n = std::distance(first, last);
        BYTEV in;
        in.reserve(static_cast<size_t>(n));
        for (; first != last; ++first) in.push_back(static_cast<BYTE>(*first));
        BYTEV out(static_cast<size_t>(n));
        encryptInto(in.data(), out.size(), out.data());
        return out;
    }

    template<typename InputIt>
    BYTEV decrypt(InputIt first, InputIt last) const {
        using diff_t = typename std::iterator_traits<InputIt>::difference_type;
        diff_t n = std::distance(first, last);
        BYTEV in;
        in.reserve(static_cast<size_t>(n));
        for (; first != last; ++first) in.push_back(static_cast<BYTE>(*first));
        BYTEV out(static_cast<size_t>(n));
        decryptInto(in.data(), out.size(), out.data());
        return out;
    }

    // AAD-aware overloads: declared here, default implementation in CipherAadFallback.cpp
    virtual BYTEV encrypt(const BYTE* data, size_t len, const BYTE* aad, size_t aad_len) const;
    virtual BYTEV decrypt(const BYTE* data, size_t len, const BYTE* aad, size_t aad_len) const;

    // Convenience AAD overloads for vector
    BYTEV encrypt(const BYTEV& data, const BYTEV& aad) const { return encrypt(data.data(), data.size(), aad.data(), aad.size()); }
    BYTEV decrypt(const BYTEV& data, const BYTEV& aad) const { return decrypt(data.data(), data.size(), aad.data(), aad.size()); }

    // Her cipher kendi testini implemente eder
    virtual bool test() const = 0;
};

#endif // PCSC_WORKSHOP1_CIPHER_H
