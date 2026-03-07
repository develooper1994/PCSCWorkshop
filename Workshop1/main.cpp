#include "PCSC.h"
#include "CardUtils.h"
#include "Readers.h"
#include "Tests/ACR1281U/ACR1281UReaderTests.h"
#include "Log/Log.h"

#define BUILD_MAIN_APP

#ifdef BUILD_MAIN_APP
int main() {
	// ============================================================
	// LOG AYARLARINI YAPMANIZA
	// ============================================================
	
	// Option 1: Debug modunda baþla (tüm loglar açýk)
	pcsc::Log::getInstance().setLogLevel(pcsc::LogLevel::Debug);
	pcsc::Log::getInstance().enableAllLogTypes();
	pcsc::Log::getInstance().enableAllCategories();
	
	// Option 2: Production modunda baþla (sadece hatalar)
	// pcsc::Log::getInstance().setLogLevel(pcsc::LogLevel::Error);
	
	// Option 3: Detaylý debugging - sadece Connection ve Card loglarý
	// pcsc::Log::getInstance().setLogLevel(pcsc::LogLevel::Debug);
	// pcsc::Log::getInstance().disableAllCategories();
	// pcsc::Log::getInstance().enableCategory(pcsc::LogCategory::Connection);
	// pcsc::Log::getInstance().enableCategory(pcsc::LogCategory::Card);
	
	// Ayarlarý konsola yazdýr (isteðe baðlý)
	// pcsc::Log::getInstance().printSettings();
	
	// ============================================================
	// MAIN UYGULAMASI
	// ============================================================
	
	PCSC pcsc;
	if (!pcsc.establishContext()) return 1;
	if (!pcsc.chooseReader())     return 1;
	if (!pcsc.connectToCard(500)) return 1;

	auto uid = CardUtils::getUid(pcsc);
	std::cout << "UID: " << std::string(uid.begin(), uid.end()) << "\n";

	// Workshop1 testleri
	// CardUtils::testDESFire(p.cardConnection());
	// 
	// ACR1281UReader test moved to Tests project
	// testACR1281UReaderUltralight(p.cardConnection());
	BYTE key[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
	ACR1281UReader acr1281u(pcsc, 16);
	testACR1281UReaderMifareClassicUnsecured(acr1281u, 10);
	
	return 0;
}
#endif