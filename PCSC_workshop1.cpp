#include "DESFire.h"
#include "Reader.h"

int main()
{
    // 1. Context oluştur
    SCARDCONTEXT hContext{};
    LONG result = SCardEstablishContext(SCARD_SCOPE_USER, nullptr, nullptr, &hContext);
    if (result != SCARD_S_SUCCESS) {
        std::cerr << "SCardEstablishContext failed: 0x" << std::hex << result << std::dec << std::endl;
        return 1;
    }

    // 2. Okuyucuları listele
    auto readers = listReaders(hContext);
    if (!readers.ok) {
        SCardReleaseContext(hContext);
        return 1;
    }

    // 3. Okuyucu seç
    size_t index = selectReader(readers.names, 1);

    // 4. Karta bağlan (kart gelene kadar bekle)
    CardConnection card(hContext, readers.names[index]);
    card.waitAndConnect(500);

    // Demo: use ACR1281U reader class
    ACR1281UReader acr1281u(card);

    // 5. Kart bilgilerini oku
    DESFire::printUid(card);
    DESFire::printVersionInfo(card);

    // Example write/read test on page 5
    try {
        int page = 4;
        BYTE data4[4] = { 'A','B','C','D' };
        acr1281u.writePage(page, data4);
        auto p = acr1281u.readPage(page);
        std::cout << "Read back: "; printHex(p.data(), (DWORD)p.size());
    }
    catch (const std::exception& ex) {
        std::cerr << "RW test failed: " << ex.what() << std::endl;
    }

    // 6. Temizlik
    card.disconnect();
    SCardFreeMemory(hContext, readers.rawBuffer);
    SCardReleaseContext(hContext);

    return 0;
}