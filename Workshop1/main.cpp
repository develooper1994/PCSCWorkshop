#include "PCSC.h"
#include "DESFire.h"
#include "Readers.h"
#include "ACR1281UReaderTests.h"

#define BUILD_MAIN_APP

#ifdef BUILD_MAIN_APP
int main() {
    PCSC pcsc;
    return pcsc.run([](PCSC& p) {
        // Workshop1 testleri
        // DESFire::testDESFire(p.cardConnection());
        // 
        // ACR1281UReader test moved to Tests project
        // testACR1281UReaderUltralight(p.cardConnection());
        BYTE keyA[6] = { (BYTE)0xFF, (BYTE)0xFF, (BYTE)0xFF, (BYTE)0xFF, (BYTE)0xFF, (BYTE)0xFF };
        ACR1281UReader acr1281u(p.cardConnection(), 16, true, KeyType::A, KeyStructure::NonVolatile, 0x01, keyA);
        // testACR1281UReaderUltralightUnsecured(acr1281u, 7);
		testACR1281UReaderMifareClassicUnsecured(acr1281u, 9);
    });
}
#endif