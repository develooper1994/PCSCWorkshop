#include "DESFire.h"

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

    // 5. Kart bilgilerini oku
    DESFire::printUid(card);
    DESFire::printVersionInfo(card);

    // 6. Temizlik
    card.disconnect();
    SCardFreeMemory(hContext, readers.rawBuffer);
    SCardReleaseContext(hContext);

    return 0;
}