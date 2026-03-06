// Tests - Unified test runner for the PCSC Workshop project
// Collects all test routines from Cipher and Reader libraries.

#include <iostream>

using namespace std;

// Cipher tests (defined in cipher_test.cpp and TestCiphers.cpp)
int  run_cipher_tests();
bool testFull();

// Card system tests (defined in CardSystemTests.cpp)
extern bool testCardMemoryLayout1K();
extern bool testCardMemoryLayout4K();
extern bool testCardTopology();
extern bool testKeyManagement();
extern bool testAccessControl();
extern bool testAuthenticationState();
extern bool testCardInterface();

int main() {
    std::cout << "========== Running all tests ==========\n\n";

    // --- Cipher algorithmic tests ---
    std::cout << "--- Cipher round-trip tests ---\n";
    bool cipherOk = testFull();
    std::cout << (cipherOk ? "Cipher round-trip tests PASSED" : "Cipher round-trip tests FAILED") << "\n\n";

    std::cout << "--- CNG cipher tests ---\n";
    int cngResult = run_cipher_tests();
    std::cout << (cngResult == 0 ? "CNG cipher tests PASSED" : "CNG cipher tests FAILED") << "\n\n";

    // --- Card System Tests ---
    std::cout << "--- Card System Tests ---\n";
    bool cardTests = true;
    
    cout << "Card Memory Layout (1K)... ";
    if (testCardMemoryLayout1K()) { cout << "PASSED\n"; }
    else { cout << "FAILED\n"; cardTests = false; }
    
    cout << "Card Memory Layout (4K)... ";
    if (testCardMemoryLayout4K()) { cout << "PASSED\n"; }
    else { cout << "FAILED\n"; cardTests = false; }
    
    cout << "Card Topology... ";
    if (testCardTopology()) { cout << "PASSED\n"; }
    else { cout << "FAILED\n"; cardTests = false; }
    
    cout << "Key Management... ";
    if (testKeyManagement()) { cout << "PASSED\n"; }
    else { cout << "FAILED\n"; cardTests = false; }
    
    cout << "Access Control... ";
    if (testAccessControl()) { cout << "PASSED\n"; }
    else { cout << "FAILED\n"; cardTests = false; }
    
    cout << "Authentication State... ";
    if (testAuthenticationState()) { cout << "PASSED\n"; }
    else { cout << "FAILED\n"; cardTests = false; }
    
    cout << "Card Interface... ";
    if (testCardInterface()) { cout << "PASSED\n"; }
    else { cout << "FAILED\n"; cardTests = false; }
    
    cout << "\n";

    bool allOk = cipherOk && (cngResult == 0) && cardTests;
    std::cout << "==========================================\n";
    std::cout << (allOk ? "ALL TESTS PASSED" : "SOME TESTS FAILED") << std::endl;
    return allOk ? 0 : 1;
}
