#include <iostream>
#include <vector>
#include <memory>

#include "Ciphers.h"

int run_cipher_tests() {
#ifdef _WIN32
    try {
        std::vector<BYTE> key(16);
        for (int i = 0; i < 16; ++i) key[i] = static_cast<BYTE>(i+1);

        // AES-GCM
        CngAESGcm gcm(key);
        const char msg[] = "Hello AES-GCM test";
        auto enc = gcm.encrypt(reinterpret_cast<const BYTE*>(msg), sizeof(msg)-1);
        auto dec = gcm.decrypt(enc.data(), enc.size());
        std::string decs(dec.begin(), dec.end());
        std::cout << "AES-GCM decrypted: " << decs << std::endl;

        // AES-CBC (existing CngAES) - need iv
        std::vector<BYTE> iv(16, 0x00);
        CngAES cbc(key, iv);
        auto enc2 = cbc.encrypt(reinterpret_cast<const BYTE*>(msg), sizeof(msg)-1);
        auto dec2 = cbc.decrypt(enc2.data(), enc2.size());
        std::string decs2(dec2.begin(), dec2.end());
        std::cout << "AES-CBC decrypted: " << decs2 << std::endl;

        // 3DES test
        std::vector<BYTE> key3(24);
        for (int i = 0; i < 24; ++i) key3[i] = static_cast<BYTE>(i+1);
        std::vector<BYTE> iv3(8, 0x00);
        Cng3DES des3(key3, iv3);
        auto enc3 = des3.encrypt(reinterpret_cast<const BYTE*>(msg), sizeof(msg)-1);
        auto dec3 = des3.decrypt(enc3.data(), enc3.size());
        std::string decs3(dec3.begin(), dec3.end());
        std::cout << "3DES decrypted: " << decs3 << std::endl;
    }
    catch (const std::exception& ex) {
        std::cerr << "Cipher tests failed: " << ex.what() << std::endl;
        return 1;
    }
#else
    std::cout << "Cipher tests are Windows-only (BCrypt)." << std::endl;
#endif
    return 0;
}

//#define BUILD_CIPHER_TEST_MAIN

#ifdef BUILD_CIPHER_TEST_MAIN
int main() { return run_cipher_tests(); }
#endif
