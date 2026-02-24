#include "PCSC.h"
#include "CardUtils.h"
#include "Readers.h"
#include "Tests/ACR1281U/ACR1281UReaderTests.h"

#define BUILD_MAIN_APP

#ifdef BUILD_MAIN_APP
int main() {
	PCSC pcsc;
	return pcsc.run([](PCSC& p) {
		// Workshop1 testleri
		// CardUtils::testDESFire(p.cardConnection());
		// 
		// ACR1281UReader test moved to Tests project
		// testACR1281UReaderUltralight(p.cardConnection());
		BYTE keyA[6] = { (BYTE)0xFF };
		ACR1281UReader acr1281u(p.cardConnection(), 16, true, KeyType::A, KeyStructure::NonVolatile, 0x01, keyA);
		testACR1281UReaderMifareClassicUnsecured(acr1281u, 10);
	});
}
#endif