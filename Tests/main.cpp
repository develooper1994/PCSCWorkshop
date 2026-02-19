// Tests - Unified test runner for the PCSC Workshop project
// Collects all test routines from Cipher and Reader libraries.

#include <iostream>

// Cipher tests (defined in cipher_test.cpp and TestCiphers.cpp)
int  run_cipher_tests();
bool testFull();

// Reader / ACR1281U tests are card-dependent; they are compiled into
// the library but only invoked when a physical card is present.
// For offline CI we only run the pure-algorithmic cipher tests here.

int main() {
    std::cout << "========== Running all tests ==========\n\n";

    // --- Cipher algorithmic tests ---
    std::cout << "--- Cipher round-trip tests ---\n";
    bool cipherOk = testFull();
    std::cout << (cipherOk ? "Cipher round-trip tests PASSED" : "Cipher round-trip tests FAILED") << "\n\n";

    std::cout << "--- CNG cipher tests ---\n";
    int cngResult = run_cipher_tests();
    std::cout << (cngResult == 0 ? "CNG cipher tests PASSED" : "CNG cipher tests FAILED") << "\n\n";

    bool allOk = cipherOk && (cngResult == 0);
    std::cout << "==========================================\n";
    std::cout << (allOk ? "ALL TESTS PASSED" : "SOME TESTS FAILED") << std::endl;
    return allOk ? 0 : 1;
}
