#include "ACR1281UReader.h"
#include "ACR1281UReaderTestHelpers.h"
#include "PcscUtils.h"
#include "Ciphers.h"
#include <sstream>

using namespace std;

ACR1281UReader::ACR1281UReader(CardConnection& c) : Reader(c) {}
ACR1281UReader::ACR1281UReader(ACR1281UReader&&) noexcept = default;
ACR1281UReader::~ACR1281UReader() = default;
ACR1281UReader& ACR1281UReader::operator=(ACR1281UReader&&) noexcept = default;

void ACR1281UReader::writePage(BYTE page, const BYTE* data4) {
    if (!card().isConnected()) throw runtime_error("Card not connected");
    BYTEV apdu{ 0xFF, 0xD6, 0x00, page, 0x04 };
    apdu.insert(apdu.end(), data4, data4 + 4);

    auto resp = card().transmit(apdu);
    if (resp.size() < 2) throw runtime_error("Invalid response for write");
    BYTE sw1 = resp[resp.size()-2], sw2 = resp[resp.size()-1];
    if (!((sw1 == 0x90 || sw1 == 0x91) && sw2 == 0x00)) {
        stringstream ss;
        ss << "Write failed SW=0x" << hex << (int)sw1 << " 0x" << (int)sw2;
        throw runtime_error(ss.str());
    }
}

BYTEV ACR1281UReader::readPage(BYTE page) {
    if (!card().isConnected()) throw runtime_error("Card not connected");
    BYTEV apdu{ 0xFF, 0xB0, 0x00, page, 0x04};

    auto resp = card().transmit(apdu);
    if (resp.size() < 2) throw runtime_error("Invalid response for read");
    BYTE sw1 = resp[resp.size()-2], sw2 = resp[resp.size()-1];
    if (!((sw1 == 0x90 || sw1 == 0x91) && sw2 == 0x00)) {
        stringstream ss;
        ss << "Read failed SW=0x" << hex << (int)sw1 << " 0x" << (int)sw2;
        throw runtime_error(ss.str());
    }
    return BYTEV(resp.begin(), resp.end()-2);
}
