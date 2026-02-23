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

void ACR1281UReader::writePage(BYTE page, const BYTE* data4) {
	if (!card().isConnected()) throw runtime_error("Card not connected");
	BYTEV apdu{ 0xFF, 0xD6, 0x00, page, getLC() };
	apdu.insert(apdu.end(), data4, data4 + getLC());

	if (isAuthRequested()) {
		try {
			auth<ACR1281UReader>(page, keyType(), getKeyNumber()); // Authenticate before reading
		} catch (const AuthFailedError& _) {
			// Authentication failed; set flag. probably key not loaded or wrong key.
			setAuthRequested(true);
			try {
				if (!keyALoaded()) {
					BYTE keyA[6];
					getKey<ACR1281UReader>(KeyType::A, keyA); // Ensure key A is loaded in Impl
					loadKey<ACR1281UReader>(keyA, keyStructure(), getKeyNumber()); // Attempt to load key if not already loaded
					setKeyALoaded(true);
				}
				if (!keyBLoaded()) {
					BYTE keyB[6];
					getKey<ACR1281UReader>(KeyType::B, keyB); // Ensure key A is loaded in Impl
					loadKey<ACR1281UReader>(keyB, keyStructure(), getKeyNumber() + 1); // Attempt to load key if not already loaded
					setKeyBLoaded(true);
				}
				auth<ACR1281UReader>(page, keyType(), getKeyNumber()); // Authenticate before throwing; if this fails again, we know it's not a key loading issue
			}
			catch (const exception& ex) {
				throw ex;
			}
		}
		setAuthRequested(false);
	}

	auto resp = card().transmit(apdu);
	if (resp.size() < 2) throw pcsc::ReaderError("Invalid response for write");
	BYTE sw1 = resp[resp.size()-2], sw2 = resp[resp.size()-1];
	if (sw1 == 0x63 && sw2 == 0x00) {
		setAuthRequested(true);
		stringstream ss;
		ss << "Auth failed SW=0x" << hex << (int)sw1 << " 0x" << (int)sw2;
		throw pcsc::AuthFailedError(ss.str());
	}
	else if (!((sw1 == 0x90 || sw1 == 0x91) && sw2 == 0x00)) {
		setAuthRequested(true);
		stringstream ss;
		ss << "Write failed SW=0x" << hex << (int)sw1 << " 0x" << (int)sw2;
		throw pcsc::ReaderWriteError(ss.str());
	}
}

void ACR1281UReader::clearPage(BYTE page) {
	// Default implementation: write zeros to the page
	BYTEV zeros(getLC(), 0x00);
	writePage(page, zeros.data());
}

BYTEV ACR1281UReader::readPage(BYTE page) {
	if (!card().isConnected()) throw std::runtime_error("Card not connected");
	BYTEV apdu{ 0xFF, 0xB0, 0x00, page, getLC()};

	if (isAuthRequested()) {
		auth<ACR1281UReader>(page, keyType(), getKeyNumber()); // Authenticate before reading
		setAuthRequested(false);
	}

	auto resp = card().transmit(apdu);
	if (resp.size() < 2) throw std::runtime_error("Invalid response for read");
	BYTE sw1 = resp[resp.size()-2], sw2 = resp[resp.size()-1];
	if (sw1 == 0x63 && sw2 == 0x00) {
		setAuthRequested(true);
		stringstream ss;
		ss << "Auth failed SW=0x" << hex << (int)sw1 << " 0x" << (int)sw2;
		throw pcsc::AuthFailedError(ss.str());
	}
	else if (!((sw1 == 0x90 || sw1 == 0x91) && sw2 == 0x00)) {
		setAuthRequested(true);
		stringstream ss;
		ss << "Read failed SW=0x" << hex << (int)sw1 << " 0x" << (int)sw2;
		throw pcsc::ReaderReadError(ss.str());
	}
	return BYTEV(resp.begin(), resp.end()-2);
}

// Provide an overload that accepts explicit length and forwards to base readPage(len) default implementation
BYTEV ACR1281UReader::readPage(BYTE page, BYTE len) {
	// Adjust LC temporarily and call the single-arg readPage
	BYTE old = getLC();
	if (len != old) setLC(len);
	BYTEV r = ACR1281UReader::readPage(page);
	if (len != old) setLC(old);
	return r;
}
