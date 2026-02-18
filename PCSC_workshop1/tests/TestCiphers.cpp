#include <iostream>
#include <vector>
#include <list>
#include <random>
#include <algorithm>
#include <cstdint>
#include "../Cipher/CaesarCipher.h"
#include "../Cipher/AffineCipher.h"

using namespace std;

bool testRoundtrip(ICipher& cipher) {
    // random generator
    std::mt19937 rng(12345);
    std::uniform_int_distribution<int> lenDist(0, 1024);
    std::uniform_int_distribution<int> byteDist(0, 255);

    // small edge cases
    vector<vector<uint8_t>> cases;
    cases.push_back({});
    cases.push_back({0});
    cases.push_back({255});
    cases.push_back(vector<uint8_t>(16, 0));
    cases.push_back(vector<uint8_t>(16, 255));

    for (int t = 0; t < 50; ++t) {
        int len = lenDist(rng);
        vector<uint8_t> data(len);
        for (int i = 0; i < len; ++i) data[i] = static_cast<uint8_t>(byteDist(rng));
        cases.push_back(std::move(data));
    }

    // test each case using various overloads
    for (const auto& c : cases) {
        // pointer overload
        BYTEV enc = cipher.encrypt(reinterpret_cast<const BYTE*>(c.data()), c.size());
        BYTEV dec = cipher.decrypt(enc.data(), enc.size());
        if (dec.size() != c.size()) return false;
        for (size_t i = 0; i < c.size(); ++i) if (dec[i] != c[i]) return false;

        // vector overload
        vector<BYTE> inVec(c.begin(), c.end());
        enc = cipher.encrypt(inVec);
        dec = cipher.decrypt(enc);
        if (dec.size() != inVec.size()) return false;
        for (size_t i = 0; i < inVec.size(); ++i) if (dec[i] != inVec[i]) return false;

        // iterator-range overload using std::list
        list<uint8_t> inList(c.begin(), c.end());
        enc = cipher.encrypt(inList.begin(), inList.end());
        dec = cipher.decrypt(enc.data(), enc.size());
        if (dec.size() != inList.size()) return false;
        size_t idx = 0;
        for (auto v : inList) { if (dec[idx++] != v) return false; }

        // encryptInto / decryptInto
        BYTEV outBuf(enc.size());
        cipher.encryptInto(reinterpret_cast<const BYTE*>(c.data()), c.size(), outBuf.data());
        if (outBuf != enc) return false;
        BYTEV outDec(c.size());
        cipher.decryptInto(outBuf.data(), outBuf.size(), outDec.data());
        if (outDec != BYTEV(c.begin(), c.end())) return false;
    }

    return true;
}

bool testFull() {
    // Caesar: shifts and multipliers to test
    vector<pair<uint8_t,uint8_t>> caesarParams = {{1,5},{3,7},{2,1},{0,0},{255,128}}; // includes non-invertible and edge values
    for (auto p : caesarParams) {
        try {
            CaesarCipher c(p.first, p.second);
            if (!testRoundtrip(c)) {
                cerr << "CaesarCipher failed for a=" << int(p.first) << " b=" << int(p.second) << "\n";
                return false;
            }
        } catch (const std::exception& e) {
            cerr << "Exception constructing/using CaesarCipher: " << e.what() << "\n";
            return false;
        }
    }

    // Affine cipher params: ensure invertible
    vector<pair<uint8_t,uint8_t>> affineParams = {{3,5},{5,7},{1,0},{255,10}};
    for (auto p : affineParams) {
        try {
            AffineCipher c(p.first, p.second);
            if (!testRoundtrip(c)) {
                cerr << "AffineCipher failed for a=" << int(p.first) << " b=" << int(p.second) << "\n";
                return false;
            }
        } catch (const std::exception& e) {
            cerr << "Exception constructing/using AffineCipher: " << e.what() << "\n";
            return false;
        }
    }

    // Additional cross-check: table vs formula for single bytes for a sample cipher
    CaesarCipher c1(7, 13);
    for (int x = 0; x < 256; ++x) {
        BYTE in = static_cast<BYTE>(x);
        BYTEV enc1 = c1.encrypt(&in, 1);
        // manual formula
        unsigned y = (static_cast<unsigned>(c1.multiplier()) * x + c1.shift()) & 0xFFu;
        if (enc1.size() != 1 || enc1[0] != static_cast<BYTE>(y)) {
            cerr << "Caesar table/formula mismatch for x=" << x << " got=" << int(enc1[0]) << " expect=" << y << "\n";
            return false;
        }
    }

    AffineCipher a1(5, 17);
    for (int x = 0; x < 256; ++x) {
        BYTE in = static_cast<BYTE>(x);
        BYTEV enc1 = a1.encrypt(&in, 1);
        unsigned y = (static_cast<unsigned>(a1.a()) * x + a1.b()) & 0xFFu;
        if (enc1.size() != 1 || enc1[0] != static_cast<BYTE>(y)) {
            cerr << "Affine table/formula mismatch for x=" << x << "\n";
            return false;
        }
    }

    return true;
}

#define BUILD_CIPHER_TEST_MAIN

#ifdef BUILD_CIPHER_TEST_MAIN
int main() {
    cout << "Running cipher tests...\n";
    bool ok = testFull();
    if (ok) cout << "All tests passed.\n";
    else cout << "Some tests failed.\n";
    return ok ? 0 : 1;
}
#endif
