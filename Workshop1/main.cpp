#include "PCSC.h"
#include "CardUtils.h"
#include "Readers.h"
#include "Tests/ACR1281U/ACR1281UReaderTests.h"

#define BUILD_MAIN_APP

#ifdef BUILD_MAIN_APP
int main() {
	PCSC pcsc;
	if (!pcsc.establishContext()) return 1;
	if (!pcsc.chooseReader())     return 1;
	if (!pcsc.connectToCard(500)) return 1;
	
	// Workshop1 testleri
	// CardUtils::testDESFire(p.cardConnection());
	// 
	// ACR1281UReader test moved to Tests project
	// testACR1281UReaderUltralight(p.cardConnection());
	BYTE keyA[6] = { (BYTE)0xFF };
	ACR1281UReader acr1281u(pcsc.cardConnection(), 16, true, KeyType::A, KeyStructure::NonVolatile, 0x01, keyA);
	testACR1281UReaderMifareClassicUnsecured(acr1281u, 13);
	
	return 0;
}
#endif