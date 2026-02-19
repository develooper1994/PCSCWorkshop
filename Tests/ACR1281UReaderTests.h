#ifndef TESTS_ACR1281UREADER_TESTS_H
#define TESTS_ACR1281UREADER_TESTS_H

class CardConnection;
class ACR1281UReader;

// Free-standing test entry points (moved out of ACR1281UReader class)
void testACR1281UReader(CardConnection& card);
void testACR1281UReaderUnsecured(ACR1281UReader& acr);
void testACR1281UReaderSecured(ACR1281UReader& acr);
void testACR1281UReaderXorCipher(ACR1281UReader& acr);
void testACR1281UReaderCaesarCipher(ACR1281UReader& acr);
#ifdef _WIN32
void testACR1281UReaderCngAES(ACR1281UReader& acr);
void testACR1281UReaderCng3DES(ACR1281UReader& acr);
void testACR1281UReaderCngAESGcm(ACR1281UReader& acr);
#endif

#endif // TESTS_ACR1281UREADER_TESTS_H
