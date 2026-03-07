// Tests - Unified test runner for the PCSC Workshop project
// Supports: Cipher tests, Card System tests, Real Card Reader tests

#include "Log/Log.h"
#include <iostream>
#include <string>

using namespace std;

// Cipher tests
int  run_cipher_tests();
bool testFull();

// Card System tests
extern int runCardSystemTests();

// Real card reader test
int testRealCardReader();

// ════════════════════════════════════════════════════════════════════════════════
// Test Menu
// ════════════════════════════════════════════════════════════════════════════════

enum TestMode {
    ALL_TESTS = 1,
    CIPHER_ONLY = 2,
    CARD_SYSTEM_ONLY = 3,
    REAL_CARD_READER = 4,
    INTERACTIVE = 0
};

void printMenu() {
    cout << "\n╔════════════════════════════════════════════════════════╗\n";
    cout << "║          PCSC Workshop Test Suite                       ║\n";
    cout << "╚════════════════════════════════════════════════════════╝\n\n";
    cout << "Test Modes:\n";
    cout << "  1. All Tests (Cipher + Card System)\n";
    cout << "  2. Cipher Tests Only\n";
    cout << "  3. Card System Tests Only\n";
    cout << "  4. Real Card Reader Test (requires connected reader)\n";
    cout << "  0. Interactive Menu (default)\n\n";
}

TestMode getTestMode(int argc, char* argv[]) {
    if (argc > 1) {
        string arg = argv[1];
        if (arg == "1") return ALL_TESTS;
        if (arg == "2") return CIPHER_ONLY;
        if (arg == "3") return CARD_SYSTEM_ONLY;
        if (arg == "4") return REAL_CARD_READER;
    }
    return INTERACTIVE;
}

// ════════════════════════════════════════════════════════════════════════════════
// Test Execution
// ════════════════════════════════════════════════════════════════════════════════

int runAllTests() {
    cout << "========== Running ALL tests ==========\n\n";
    
    bool allPassed = true;
    
    // Cipher tests
    cout << "--- Cipher round-trip tests ---\n";
    bool cipherRoundtrip = testFull();
    cout << (cipherRoundtrip ? "PASSED" : "FAILED") << "\n\n";
    
    cout << "--- Crypto cipher tests ---\n";
    int cryptoResult = run_cipher_tests();
    cout << (cryptoResult == 0 ? "PASSED" : "FAILED") << "\n\n";
    
    // Card System tests
    int cardResult = runCardSystemTests();
    
    cout << "\n==========================================\n";
    bool success = cipherRoundtrip && (cryptoResult == 0) && (cardResult == 0);
    cout << (success ? "ALL TESTS PASSED" : "SOME TESTS FAILED") << "\n";
    cout << "==========================================\n";
    
    return success ? 0 : 1;
}

int runCipherTests() {
    cout << "========== Running Cipher tests ==========\n\n";
    
    cout << "--- Cipher round-trip tests ---\n";
    bool result1 = testFull();
    cout << (result1 ? "PASSED" : "FAILED") << "\n\n";
    
    cout << "--- Crypto cipher tests ---\n";
    int result2 = run_cipher_tests();
    cout << (result2 == 0 ? "PASSED" : "FAILED") << "\n";
    
    bool success = result1 && (result2 == 0);
    cout << (success ? "\nALL CIPHER TESTS PASSED" : "\nSOME CIPHER TESTS FAILED") << "\n";
    
    return success ? 0 : 1;
}

int runCardSystemTestsOnly() {
    return runCardSystemTests();
}

// ════════════════════════════════════════════════════════════════════════════════
// Main Entry Point
// ════════════════════════════════════════════════════════════════════════════════

int main(int argc, char* argv[]) {
    pcsc::Log::getInstance().setLogLevel(pcsc::LogLevel::Debug);
    pcsc::Log::getInstance().enableAllLogTypes();
    pcsc::Log::getInstance().enableAllCategories();
    TestMode mode = getTestMode(argc, argv);
    
    if (mode == INTERACTIVE) {
        printMenu();
        cout << "Select test mode (0-4): ";
        int choice;
        cin >> choice;
        mode = (TestMode)choice;
    }
    
    cout << "\n";
    
    int result = 0;
    
    switch (mode) {
        case ALL_TESTS: {
            result = runAllTests();
            break;
        }
        case CIPHER_ONLY: {
            result = runCipherTests();
            break;
        }
        case CARD_SYSTEM_ONLY: {
            result = runCardSystemTestsOnly();
            break;
        }
        case REAL_CARD_READER: {
            cout << "========== Real Card Reader Test ==========\n\n";
            result = testRealCardReader();
            break;
        }
        case INTERACTIVE:
        default: {
            cout << "Invalid mode. Using all tests.\n";
            result = runAllTests();
            break;
        }
    }
    
    return result;
}
