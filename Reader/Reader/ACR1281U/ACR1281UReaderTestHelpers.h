#ifndef PCSC_WORKSHOP1_READER_ACR1281UREADER_TESTHELPERS_H
#define PCSC_WORKSHOP1_READER_ACR1281UREADER_TESTHELPERS_H

#include <string>

class ACR1281UReader;
class ICipher;
class ICipherAAD;

// Helper tests used by ACR1281UReader test routines
void runPerPageTest(ACR1281UReader& acr, const ICipher& cipher, const std::string& testName, const std::string& text);
void runBlobTest(ACR1281UReader& acr, const ICipher& cipher, const std::string& testName, const std::string& text);
void runBlobTestAAD(ACR1281UReader& acr, const ICipherAAD& cipher, const std::string& testName, const std::string& text, const std::string& aad);

#endif // PCSC_WORKSHOP1_READER_ACR1281UREADER_TESTHELPERS_H
