#include "ACR1281UReader.h"
#include "ACR1281UReaderTestHelpers.h"
#include "../../utils/PcscUtils.h"
#include "Ciphers.h"
#include <iostream>
#include <string>
#include <vector>
#include <stdexcept>

// Per-page encrypt/decrypt test: writes text page-by-page encrypted, reads back & compares
void runPerPageTest(ACR1281UReader& acr, const ICipher& cipher, const std::string& testName, const std::string& text) {
    std::cout << "\n--- " << testName << " per-page test ---\n";
    try {
        const BYTE startPage = 4;
        // Print original and original as hex
        std::cout << "Original " << text << " || Size: " << text.size() << " bytes (" << (int)startPage << '\n';
        std::cout << "Original (hex): ";
        printHex(reinterpret_cast<const BYTE*>(text.data()), static_cast<DWORD>(text.size()));

        // Pad text to multiple of 4
        std::string padded = text;
        while (padded.size() % 4 != 0) padded.push_back('\0');

        size_t totalPages = padded.size() / 4;
        std::cout << "Writing " << text.size() << " bytes (" << totalPages
                  << " pages) starting at page " << (int)startPage << '\n';

        // Accumulators for printing all pages on single lines
        BYTEV allEncrypted; // concatenated encrypted outputs per page
        BYTEV allRaw;       // concatenated raw page data read back from card
        std::vector<BYTE> readBack; // decrypted page contents concatenated

        // Write each page encrypted (compute encryption explicitly so we can collect it)
        for (size_t i = 0; i < totalPages; ++i) {
            const BYTE* src = reinterpret_cast<const BYTE*>(padded.data()) + i * 4;
            // compute full encryption output for this 4-byte chunk
            auto enc = cipher.encrypt(src, 4);
            // append to allEncrypted for single-line print later
            allEncrypted.insert(allEncrypted.end(), enc.begin(), enc.end());

            // previous behavior wrote only the first up-to-4 bytes of enc to card
            BYTE buf[4] = {0,0,0,0};
            for (size_t j = 0; j < 4 && j < enc.size(); ++j) buf[j] = enc[j];
            acr.writePage(static_cast<BYTE>(startPage + i), buf);
        }

        // Read back each page raw and decrypted, accumulate
        for (size_t i = 0; i < totalPages; ++i) {
            BYTE pageIdx = static_cast<BYTE>(startPage + i);
            auto raw = acr.readPage(pageIdx);
            if (!raw.empty()) allRaw.insert(allRaw.end(), raw.begin(), raw.end());

            auto dec = acr.readPageDecrypted(pageIdx, cipher);
            readBack.insert(readBack.end(), dec.begin(), dec.end());
        }

        // Print concatenated encrypted and raw data on single lines
        std::cout << "All pages encrypted (hex): ";
        if (!allEncrypted.empty()) printHex(allEncrypted.data(), static_cast<DWORD>(allEncrypted.size()));
        else std::cout << "<empty>\n";

        std::cout << "Card raw all pages (hex): ";
        if (!allRaw.empty()) printHex(allRaw.data(), static_cast<DWORD>(allRaw.size()));
        else std::cout << "<empty>\n";

        // Trim trailing nulls for display and produce final result
        std::string result;
        if (!readBack.empty()) {
            result.assign(reinterpret_cast<const char*>(readBack.data()), text.size());
        }

        std::cout << "Read back (hex): ";
        if (!readBack.empty()) printHex(reinterpret_cast<const BYTE*>(result.data()), static_cast<DWORD>(result.size()));
        else std::cout << "<empty>\n";
        std::cout << "Read back (text): " << result << '\n';

        if (result == text) {
            std::cout << testName << " per-page test PASSED\n";
        } else {
            std::cout << testName << " per-page test FAILED\n";
        }
    }
    catch (const std::exception& ex) {
        std::cerr << testName << " per-page test failed: " << ex.what() << std::endl;
    }
}

// Blob encrypt/decrypt test: encrypts entire data as a blob, writes, reads back & compares
void runBlobTest(ACR1281UReader& acr, const ICipher& cipher, const std::string& testName, const std::string& text) {
    std::cout << "\n--- " << testName << " blob test ---\n";
    try {
        const BYTE startPage = 4;
        // Print original and original as hex
        std::cout << "Original " << text << " || Size: " << text.size() << " bytes (" << (int)startPage << '\n';
        std::cout << "Original (hex): ";
        printHex(reinterpret_cast<const BYTE*>(text.data()), static_cast<DWORD>(text.size()));

        std::cout << "Writing " << text.size() << " bytes starting at page "
                  << (int)startPage << '\n';

        // Encrypt whole blob explicitly so we can print encrypted bytes
        BYTEV data(text.begin(), text.end());
        auto encrypted = cipher.encrypt(data);
        std::cout << "Encrypted blob (hex): ";
        if (!encrypted.empty()) printHex(encrypted.data(), static_cast<DWORD>(encrypted.size()));
        else std::cout << "<empty>\n";

        // Write encrypted bytes raw to card
        acr.writeData(startPage, encrypted);

        // Read raw encrypted bytes back from card (exact encrypted size)
        auto raw = acr.readData(startPage, encrypted.size());
        std::cout << "Card raw (hex): ";
        if (!raw.empty()) printHex(raw.data(), static_cast<DWORD>(raw.size()));
        else std::cout << "<empty>\n";

        // Decrypt raw
        auto decrypted = cipher.decrypt(raw);
        size_t len = decrypted.size() < text.size() ? decrypted.size() : text.size();
        std::string result(decrypted.begin(), decrypted.begin() + len);
        std::cout << "Read back (hex): ";
        if (!decrypted.empty()) printHex(decrypted.data(), static_cast<DWORD>(len));
        else std::cout << "<empty>\n";
        std::cout << "Read back (text): " << result << '\n';

        if (result == text) {
            std::cout << testName << " blob test PASSED\n";
        } else {
            std::cout << testName << " blob test FAILED\n";
        }
    }
    catch (const std::exception& ex) {
        std::cerr << testName << " blob test failed: " << ex.what() << std::endl;
    }
}

// AAD blob encrypt/decrypt test
void runBlobTestAAD(ACR1281UReader& acr, const ICipherAAD& cipher, const std::string& testName, const std::string& text, const std::string& aad) {
    std::cout << "\n--- " << testName << " blob AAD test ---\n";
    try {
        const BYTE startPage = 4;
        // Print original and original as hex
        std::cout << "Original " << text << " || Size: " << text.size() << " bytes (" << (int)startPage << '\n';
        std::cout << "Original (hex): ";
        printHex(reinterpret_cast<const BYTE*>(text.data()), static_cast<DWORD>(text.size()));

        std::cout << "Writing " << text.size() << " bytes (AAD=\"" << aad
                  << "\") starting at page " << (int)startPage << '\n';

        const BYTE* aadPtr = reinterpret_cast<const BYTE*>(aad.data());
        size_t aadLen = aad.size();

        // Encrypt whole blob with AAD explicitly so we can print encrypted bytes
        BYTEV data(text.begin(), text.end());
        auto encrypted = cipher.encrypt(data.data(), data.size(), aadPtr, aadLen);
        std::cout << "Encrypted blob (hex): ";
        if (!encrypted.empty()) printHex(encrypted.data(), static_cast<DWORD>(encrypted.size()));
        else std::cout << "<empty>\n";

        // Write encrypted bytes raw to card
        acr.writeData(startPage, encrypted);

        // Read raw encrypted bytes back from card (exact encrypted size)
        auto raw = acr.readData(startPage, encrypted.size());
        std::cout << "Card raw (hex): ";
        if (!raw.empty()) printHex(raw.data(), static_cast<DWORD>(raw.size()));
        else std::cout << "<empty>\n";

        // Decrypt raw with AAD
        auto decrypted = cipher.decrypt(raw.data(), raw.size(), aadPtr, aadLen);
        size_t len = decrypted.size() < text.size() ? decrypted.size() : text.size();
        std::string result(decrypted.begin(), decrypted.begin() + len);
        std::cout << "Read back (hex): ";
        if (!decrypted.empty()) printHex(decrypted.data(), static_cast<DWORD>(len));
        else std::cout << "<empty>\n";
        std::cout << "Read back (text): " << result << '\n';

        if (result == text) {
            std::cout << testName << " blob AAD test PASSED\n";
        } else {
            std::cout << testName << " blob AAD test FAILED\n";
        }
    }
    catch (const std::exception& ex) {
        std::cerr << testName << " blob AAD test failed: " << ex.what() << std::endl;
    }
}
