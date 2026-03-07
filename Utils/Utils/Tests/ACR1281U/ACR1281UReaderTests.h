#ifndef TESTS_ACR1281UREADER_TESTS_H
#define TESTS_ACR1281UREADER_TESTS_H

class PCSC;
class ACR1281UReader;

// Mifare Classic tests
void testACR1281UReaderMifareClassic(PCSC& pcsc, BYTE startPage);
void testACR1281UReaderMifareClassicUnsecured(ACR1281UReader& acr1281u, BYTE startPage);

// Ultralight tests
void testACR1281UReaderUltralight(PCSC& pcsc, BYTE startPage = 4);
void testACR1281UReaderUltralightUnsecured(ACR1281UReader& acr, BYTE startPage = 4);
void testACR1281UReaderUltralightSecured(ACR1281UReader& acr, BYTE startPage = 4);
void testACR1281UReaderXorCipher(ACR1281UReader& acr, BYTE startPage = 4);
void testACR1281UReaderCaesarCipher(ACR1281UReader& acr, BYTE startPage = 4);
void testACR1281UReaderAesCbc(ACR1281UReader& acr, BYTE startPage = 4);
void testACR1281UReaderTripleDes(ACR1281UReader& acr, BYTE startPage = 4);
void testACR1281UReaderAesGcm(ACR1281UReader& acr, BYTE startPage = 4);

#endif // TESTS_ACR1281UREADER_TESTS_H
