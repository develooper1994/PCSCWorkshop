#include <iostream>
#include <vector>
#include <memory>
#include <algorithm>
#include <cstring>

#include "Crypto.h"

int run_cipher_tests() {
    int failures = 0;
    try {
        std::vector<BYTE> key(16);
        for (int i = 0; i < 16; ++i) key[i] = static_cast<BYTE>(i + 1);

        // AES-GCM (runtime)
        {
            auto gcm = crypto::create(CipherType::AES_128_GCM, key);
            const char msg[] = "Hello AES-GCM test";
            auto enc = gcm->encrypt(reinterpret_cast<const BYTE*>(msg), sizeof(msg) - 1);
            auto dec = gcm->decrypt(enc.data(), enc.size());
            std::string decs(dec.begin(), dec.end());
            std::cout << "AES-GCM decrypted: " << decs << std::endl;
            if (decs != msg) { std::cerr << "  FAIL\n"; ++failures; }
        }

        // AES-CBC (compile-time template)
        {
            std::vector<BYTE> iv(16, 0x00);
            const char msg[] = "Hello AES-CBC test";
            auto enc = crypto::encrypt<CipherType::AES_128_CBC>(key, iv,
                           reinterpret_cast<const BYTE*>(msg), sizeof(msg) - 1);
            auto dec = crypto::decrypt<CipherType::AES_128_CBC>(key, iv,
                           enc.data(), enc.size());
            std::string decs(dec.begin(), dec.end());
            std::cout << "AES-CBC decrypted: " << decs << std::endl;
            if (decs != msg) { std::cerr << "  FAIL\n"; ++failures; }
        }

        // 3DES (runtime enum)
        {
            std::vector<BYTE> key3(24);
            for (int i = 0; i < 24; ++i) key3[i] = static_cast<BYTE>(i + 1);
            std::vector<BYTE> iv3(8, 0x00);
            const char msg[] = "Hello AES-GCM test";
            auto enc = crypto::encrypt(CipherType::TripleDES_CBC, key3, iv3,
                           reinterpret_cast<const BYTE*>(msg), sizeof(msg) - 1);
            auto dec = crypto::decrypt(CipherType::TripleDES_CBC, key3, iv3,
                           enc.data(), enc.size());
            std::string decs(dec.begin(), dec.end());
            std::cout << "3DES decrypted: " << decs << std::endl;
            if (decs != msg) { std::cerr << "  FAIL\n"; ++failures; }
        }

        // AES-CTR
        {
            std::vector<BYTE> nonce(16, 0x42);
            const char msg[] = "CTR mode test data";
            auto enc = crypto::encrypt<CipherType::AES_128_CTR>(key, nonce,
                           reinterpret_cast<const BYTE*>(msg), sizeof(msg) - 1);
            auto dec = crypto::decrypt<CipherType::AES_128_CTR>(key, nonce,
                           enc.data(), enc.size());
            std::string decs(dec.begin(), dec.end());
            std::cout << "AES-CTR decrypted: " << decs << std::endl;
            if (decs != msg) { std::cerr << "  FAIL\n"; ++failures; }
        }

        // BlockCipher raw CBC round-trip
        {
            std::vector<BYTE> iv(16, 0x00);
            std::vector<BYTE> plain(16, 0x55);
            auto enc = crypto::block::encryptAesCbc(key, iv, plain);
            auto dec = crypto::block::decryptAesCbc(key, iv, enc);
            std::cout << "BlockCipher AES raw: " << (dec == plain ? "OK" : "FAIL") << std::endl;
            if (dec != plain) ++failures;
        }

        // CMAC
        {
            std::vector<BYTE> data = {0x01, 0x02, 0x03, 0x04};
            auto mac = crypto::block::cmacAes128(key, data);
            std::cout << "CMAC-AES128 len: " << mac.size() << " (expected 16)" << std::endl;
            if (mac.size() != 16) ++failures;
        }

        // SHA-256
        {
            const char msg[] = "hello";
            auto hash = crypto::sha256(reinterpret_cast<const BYTE*>(msg), 5);
            std::cout << "SHA-256 len: " << hash.size() << " (expected 32)" << std::endl;
            if (hash.size() != 32) ++failures;
        }

        // HMAC-SHA256
        {
            auto mac = crypto::hmacSha256(key, reinterpret_cast<const BYTE*>("msg"), 3);
            std::cout << "HMAC-SHA256 len: " << mac.size() << " (expected 32)" << std::endl;
            if (mac.size() != 32) ++failures;
        }

        // Random
        {
            auto r = crypto::randomBytes(32);
            std::cout << "Random 32 bytes: len=" << r.size() << std::endl;
            if (r.size() != 32) ++failures;
        }

        // PBKDF2
        {
            BYTEV salt = {0xDE, 0xAD, 0xBE, 0xEF};
            auto dk = crypto::pbkdf2Sha256("password", salt, 1000, 32);
            std::cout << "PBKDF2 derived key len: " << dk.size() << " (expected 32)" << std::endl;
            if (dk.size() != 32) ++failures;
        }

    }
    catch (const std::exception& ex) {
        std::cerr << "Cipher tests exception: " << ex.what() << std::endl;
        return 1;
    }

    if (failures > 0) {
        std::cerr << failures << " cipher test(s) FAILED\n";
        return 1;
    }
    return 0;
}
