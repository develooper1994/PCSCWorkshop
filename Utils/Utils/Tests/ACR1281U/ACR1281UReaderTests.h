#ifndef TESTS_ACR1281UREADER_TESTS_H
#define TESTS_ACR1281UREADER_TESTS_H

class CardConnection;
class ACR1281UReader;

// Mifare Classic tests
void testACR1281UReaderMifareClassic(CardConnection& card, BYTE startPage);
void testACR1281UReaderMifareClassicUnsecured(ACR1281UReader& acr1281u, BYTE startPage);

// Ultralight tests
// Free-standing test entry points (moved out of ACR1281UReader class)
void testACR1281UReaderUltralight(CardConnection& card, BYTE startPage = 4);
void testACR1281UReaderUltralightUnsecured(ACR1281UReader& acr, BYTE startPage = 4);
void testACR1281UReaderUltralightSecured(ACR1281UReader& acr, BYTE startPage = 4);
void testACR1281UReaderXorCipher(ACR1281UReader& acr, BYTE startPage = 4);
void testACR1281UReaderCaesarCipher(ACR1281UReader& acr, BYTE startPage = 4);
#ifdef _WIN32
void testACR1281UReaderCngAES(ACR1281UReader& acr, BYTE startPage = 4);
void testACR1281UReaderCng3DES(ACR1281UReader& acr, BYTE startPage = 4);
void testACR1281UReaderCngAESGcm(ACR1281UReader& acr, BYTE startPage = 4);
#endif

#endif // TESTS_ACR1281UREADER_TESTS_H
