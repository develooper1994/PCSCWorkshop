#include "PCSC.h"
#include "DESFire.h"
#include "Readers.h"

#define BUILD_MAIN_APP

#ifdef BUILD_MAIN_APP
int main() {
    PCSC pcsc;
    return pcsc.run([](PCSC& p) {
        // Workshop1 testleri
        DESFire::testDESFire(p.cardConnection());
        // ACR1281UReader test moved to Tests project
        // ACR1281UReader::testACR1281UReader(p.cardConnection());
    });
}
#endif