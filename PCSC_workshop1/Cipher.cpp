#include "Cipher.h"
#include <array>

// ============================================================
// XorCipher::Impl
// ============================================================
struct XorCipher::Impl {
    Key4 key;

    explicit Impl(const Key4& k) : key(k) {}

    BYTEV xorTransform(const BYTE* data, size_t len) const {
        BYTEV out(len);
        for (size_t i = 0; i < len; ++i) {
            out[i] = data[i] ^ key[i % 4];
        }
        return out;
    }
};

// ============================================================
// XorCipher
// ============================================================

XorCipher::XorCipher(const Key4& key)
    : pImpl(std::make_unique<Impl>(key))
{}

XorCipher::~XorCipher() = default;

XorCipher::XorCipher(XorCipher&&) noexcept = default;
XorCipher& XorCipher::operator=(XorCipher&&) noexcept = default;

BYTEV XorCipher::encrypt(const BYTE* data, size_t len) const {
    return pImpl->xorTransform(data, len);
}

BYTEV XorCipher::decrypt(const BYTE* data, size_t len) const {
    return pImpl->xorTransform(data, len);  // XOR simetriktir
}

const XorCipher::Key4& XorCipher::key() const {
    return pImpl->key;
}

bool XorCipher::test() const {
    std::cout << "\n--- XorCipher::test ---\n";
    bool allPassed = true;

    // Test 1: encrypt sonrasý decrypt orijinal veriyi döndürmeli
    {
        BYTE original[4] = { 'A', 'B', 'C', 'D' };
        auto enc = encrypt(original, 4);
        auto dec = decrypt(enc.data(), enc.size());

        bool pass = (dec.size() == 4);
        for (size_t i = 0; i < 4 && pass; ++i)
            pass = (dec[i] == original[i]);

        std::cout << "  [" << (pass ? "PASS" : "FAIL") << "] encrypt->decrypt roundtrip\n";
        if (!pass) {
            std::cout << "    Expected: "; printHex(original, 4);
            std::cout << "    Got:      "; printHex(dec.data(), (DWORD)dec.size());
        }
        allPassed = allPassed && pass;
    }

    // Test 2: þifreli veri orijinalden farklý olmalý (anahtar sýfýr deðilse)
    {
        BYTE original[4] = { 0x11, 0x22, 0x33, 0x44 };
        auto enc = encrypt(original, 4);

        bool keyNonZero = false;
        for (auto b : key()) if (b != 0) { keyNonZero = true; break; }

        bool different = false;
        for (size_t i = 0; i < 4; ++i)
            if (enc[i] != original[i]) { different = true; break; }

        bool pass = !keyNonZero || different;  // anahtar 0 ise ayný olabilir
        std::cout << "  [" << (pass ? "PASS" : "FAIL") << "] encrypted differs from original\n";
        allPassed = allPassed && pass;
    }

    // Test 3: boþ veri
    {
        auto enc = encrypt(nullptr, 0);
        auto dec = decrypt(nullptr, 0);
        bool pass = enc.empty() && dec.empty();
        std::cout << "  [" << (pass ? "PASS" : "FAIL") << "] empty data\n";
        allPassed = allPassed && pass;
    }

    // Test 4: 4 byte'tan büyük veri (anahtar tekrarý)
    {
        BYTE original[8] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08 };
        auto enc = encrypt(original, 8);
        auto dec = decrypt(enc.data(), enc.size());

        bool pass = (dec.size() == 8);
        for (size_t i = 0; i < 8 && pass; ++i)
            pass = (dec[i] == original[i]);

        std::cout << "  [" << (pass ? "PASS" : "FAIL") << "] multi-block roundtrip (8 bytes)\n";
        allPassed = allPassed && pass;
    }

    std::cout << "  XorCipher test " << (allPassed ? "PASSED" : "FAILED") << "\n";
    return allPassed;
}
