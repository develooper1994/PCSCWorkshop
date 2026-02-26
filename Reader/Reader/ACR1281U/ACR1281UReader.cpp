#include "ACR1281UReader.h"
#include "PcscUtils.h"
#include "Ciphers.h"
#include "Utils.h"
#include <sstream>

using namespace std;
using pcsc::ReaderError;
using pcsc::AuthFailedError;

ACR1281UReader::ACR1281UReader(CardConnection& c) : Reader(c) {}
ACR1281UReader::ACR1281UReader(CardConnection& c, BYTE lc, bool authRequested, KeyType kt, KeyStructure ks, BYTE keyNumber, const BYTE key[6])
	: Reader(c, lc, authRequested, kt, ks, keyNumber, key) {}
ACR1281UReader::ACR1281UReader(CardConnection& c, BYTE lc, bool authRequested, KeyType kt, KeyStructure ks, BYTE keyNumber, const BYTE keyA[6], const BYTE keyB[6])
	: Reader(c, lc, authRequested, kt, ks, keyNumber, keyA, keyB) {}
ACR1281UReader::ACR1281UReader(ACR1281UReader&&) noexcept = default;
ACR1281UReader::~ACR1281UReader() = default;
ACR1281UReader& ACR1281UReader::operator=(ACR1281UReader&&) noexcept = default;

// Provide an overload that accepts explicit length and forwards to base writePage(len) default implementation
void ACR1281UReader::writePage(BYTE page, const BYTE* data, BYTE len) {
	// Adjust LE temporarily, call the void writePage overload and return an empty BYTEV.
	// The previous implementation attempted to assign the void overload's result to a BYTEV.
	BYTE old = getLE();
	if (len != old) setLE(len);
	// Call the void overload (customApdu defaults to nullptr)
	this->writePage(page, data);
	if (len != old) setLE(old);
}

// Provide an overload that accepts explicit length and forwards to base readPage(len) default implementation
BYTEV ACR1281UReader::readPage(BYTE page, BYTE len) {
	// Adjust LE temporarily and call the single-arg readPage
	BYTE old = getLE();
	if (len != old) setLE(len);
	BYTEV r = ACR1281UReader::readPage(page);
	if (len != old) setLE(old);
	return r;
}
