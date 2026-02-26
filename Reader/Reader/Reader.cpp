#include "Reader.h"
#include <sstream>
#include <vector>
#include <cstring>

// ============================================================
// Reader::Impl — hidden implementation of the base Reader
// ============================================================
struct Reader::Impl {
	CardConnection& cardConnection;
	BYTE LE = 0x04;
	bool isAuthRequested = true;
	KeyType keyType = KeyType::A; // default to Key A
	KeyStructure keyStructure = KeyStructure::NonVolatile; // default to non-volatile
	BYTE keyNumber = 0x01;
	BYTE key[16] = { (BYTE)0xFF }; // default key for Mifare Classic [keyA(6 bytes)|accessbits(4 bytes)|keyB(6 bytes)]
	bool keyLoaded = false;

	// Primary ctor: initialize with CardConnection reference and optional parameters
	explicit Impl(CardConnection& c,
			  BYTE le = 0x04,
			  bool authRequested = false,
			  KeyType kt = KeyType::A,
			  KeyStructure ks = KeyStructure::NonVolatile,
			  BYTE keyNum = 0x01,
			  const BYTE* KEY = nullptr)
		: cardConnection(c), LE(le), isAuthRequested(authRequested), keyType(kt), keyStructure(ks), keyNumber(keyNum)
	{
		if (isAuthRequested && KEY!=nullptr) std::memcpy(key, KEY, 16);
		if (KEY!=nullptr) keyLoaded = true;
	}

	// Non-copyable because of reference member
	Impl(const Impl&) = delete;
	Impl& operator=(const Impl&) = delete;

	// Movable: allow move construction/assignment
	Impl(Impl&& other) noexcept
		: cardConnection(other.cardConnection), LE(other.LE), isAuthRequested(other.isAuthRequested), keyType(other.keyType), keyStructure(other.keyStructure), keyNumber(other.keyNumber), keyLoaded(other.keyLoaded)
	{
		std::memcpy(key, other.key, 16);
	}

	Impl& operator=(Impl&& other) noexcept {
		if (this != &other) {
			// card is a reference — cannot rebind; this assignment should not be used in practice
			LE = other.LE;
			isAuthRequested = other.isAuthRequested;
			keyType = other.keyType;
			keyStructure = other.keyStructure;
			keyNumber = other.keyNumber;
			keyLoaded = other.keyLoaded;
			std::memcpy(key, other.key, 16);
		}
		return *this;
	}
};

// ============================================================
// Reader — base class implementation
// ============================================================
Reader::Reader(CardConnection& c) : pImpl(std::make_unique<Impl>(c)) {}
Reader::Reader(CardConnection& c, BYTE lc, bool authRequested, KeyType kt, KeyStructure ks, BYTE keyNumber, const BYTE key[6])
	: pImpl(std::make_unique<Impl>(c, lc, authRequested, kt, ks, keyNumber, key)) {}
Reader::Reader(CardConnection& c, BYTE lc, bool authRequested, KeyType kt, KeyStructure ks, BYTE keyNumber, const BYTE keyA[6], const BYTE keyB[6])
	: pImpl(std::make_unique<Impl>(c, lc, authRequested, kt, ks, keyNumber, nullptr)) {
	// If both keyA and keyB are provided, combine into 16-byte blob with default access bits (0xFF)
	if (keyA && keyB) {
		BYTE tmp[16];
		std::memcpy(tmp, keyA, 6);
		// std::memset(tmp + 6, 0xFF, 4); // Don't assume default access bits; leave as zeros or require explicit setting via setKeyAll or the 3-arg constructor
		std::memcpy(tmp + 10, keyB, 6);
		setKeyAll(tmp);
	}
}

// New overload: accept explicit accessBits (4 bytes) between keyA and keyB
Reader::Reader(CardConnection& c, BYTE lc, bool authRequested, KeyType kt, KeyStructure ks, BYTE keyNumber, const BYTE keyA[6], const BYTE accessBits[4], const BYTE keyB[6])
	: pImpl(std::make_unique<Impl>(c, lc, authRequested, kt, ks, keyNumber, nullptr)) {
	if (keyA && accessBits && keyB) {
		BYTE tmp[16];
		std::memcpy(tmp, keyA, 6);
		std::memcpy(tmp + 6, accessBits, 4);
		std::memcpy(tmp + 10, keyB, 6);
		setKeyAll(tmp);
	}
}
Reader::Reader(Reader&&) noexcept = default;
Reader::~Reader() = default;
Reader& Reader::operator=(Reader&&) noexcept = default;

CardConnection& Reader::cardConnection() { return pImpl->cardConnection; }
const CardConnection& Reader::cardConnection() const { return pImpl->cardConnection; }

// New accessors to expose Impl members
bool Reader::isAuthRequested() const { return pImpl->isAuthRequested; }
void Reader::setAuthRequested(bool v) { pImpl->isAuthRequested = v; }

BYTE Reader::getLE() const { return pImpl->LE; }
void Reader::setLE(BYTE le) { pImpl->LE = le; }

KeyType Reader::keyType() const { return pImpl->keyType; }
void Reader::setKeyType(KeyType kt) { pImpl->keyType = kt; }

KeyStructure Reader::keyStructure() const { return pImpl->keyStructure; }
void Reader::setKeyStructure(KeyStructure ks) { pImpl->keyStructure = ks; }

BYTE Reader::getKeyNumber() const { return pImpl->keyNumber; }
void Reader::setKeyNumber(BYTE key) { pImpl->keyNumber = key; }

// Full 16-byte key storage accessors: [keyA(6 bytes)|accessbits(4 bytes)|keyB(6 bytes)]
void Reader::getKeyAll(BYTE out16[16]) const {
	if (!out16) return;
	std::memcpy(out16, pImpl->key, 16);
}
void Reader::setKeyAll(const BYTE* key16) {
	if (!key16) return;
	std::memcpy(pImpl->key, key16, 16);
	pImpl->keyLoaded = true;
}

// partial key accessors: get/set 6-byte Key A or Key B from the 16-byte blob
void Reader::getKey(KeyType kt, BYTE out[16]) const {
	if (!out) return;
	switch (kt) {
	case KeyType::A:
		std::memcpy(out, pImpl->key, 6);
		break;
	case KeyType::B:
		std::memcpy(out, pImpl->key + 10, 6);
		break;
	case KeyType::ACCESS:
		std::memcpy(out, pImpl->key + 6, 4);
		break;
	case KeyType::AB:
		std::memcpy(out, pImpl->key, 6); // Return Key A for BOTH
		std::memcpy(out, pImpl->key + 10, 6);
		break;
	case KeyType::ALL:
		std::memcpy(out, pImpl->key, 16);
		break;
	default:
		break;
	}
}
void Reader::setKey(KeyType kt, const BYTE* key) {
	if (!key) return;
	switch (kt) {
	case KeyType::A:
		std::memcpy(pImpl->key, key, 6);
		break;
	case KeyType::B:
		std::memcpy(pImpl->key + 10, key, 6);
		break;
	case KeyType::ACCESS:
		std::memcpy(pImpl->key + 6, key, 4);
		break;
	case KeyType::AB:
		std::memcpy(pImpl->key, key, 6); // Set Key A for BOTH
		std::memcpy(pImpl->key + 10, key, 6);
		break;
	case KeyType::ALL:
		std::memcpy(pImpl->key, key, 16);
		break;
	default:
		break;
	}
	pImpl->keyLoaded = true;
}

bool Reader::keyLoaded() const { return pImpl->keyLoaded; }
void Reader::setKeyLoaded(bool loaded) { pImpl->keyLoaded = loaded; }

// Provide default implementation that forwards to the simple readPage
BYTEV Reader::readPage(BYTE page, BYTE len) {
	// Adjust LE temporarily if different from default
	BYTE oldLE = getLE();
	if (len != oldLE) setLE(len);
	BYTEV res = readPage(page);
	if (len != oldLE) setLE(oldLE);
	return res;
}

// --- Encrypted single-page write/read ---
void Reader::writePageEncrypted(BYTE page, const BYTE* data, const ICipher& cipher) {
	auto enc = cipher.encrypt(data, getLE());
	BYTEV buf(getLE());
	for (size_t i = 0; i < getLE() && i < enc.size(); ++i) buf[i] = enc[i];
	writePage(page, buf.data());
}
BYTEV Reader::readPageDecrypted(BYTE page, const ICipher& cipher) {
	BYTEV raw = readPage(page);
	return cipher.decrypt(raw);
}

// --- AAD-capable single-page operations ---
void Reader::writePageEncryptedAAD(BYTE page, const BYTE* data, const ICipher& cipher, const BYTE* aad, size_t aad_len) {
	// Prefer AAD-capable encrypt; ICipher default forwards to non-AAD if unsupported
	auto enc = cipher.encrypt(data, getLE(), aad, aad_len);
	BYTEV buf(getLE());
	for (size_t i = 0; i < getLE() && i < enc.size(); ++i) buf[i] = enc[i];
	writePage(page, buf.data());
}
BYTEV Reader::readPageDecryptedAAD(BYTE page, const ICipher& cipher, const BYTE* aad, size_t aad_len) {
	BYTEV raw = readPage(page);
	return cipher.decrypt(raw.data(), raw.size(), aad, aad_len);
}

// --- Plain convenience overloads ---
void Reader::performAuth(BYTE page) {
	try {
		auth(page, keyType(), getKeyNumber());
	}
	catch (const pcsc::AuthFailedError&) {
		if (!keyLoaded()) {
			BYTE key[16];
			getKey(keyType(), key);
			loadKey(key, keyStructure(), getKeyNumber());
			setKeyLoaded(true);
		}
		auth(page, keyType(), getKeyNumber());
	}
	setAuthRequested(false); // Don't reset auth requested flag here; let caller decide when to re-authenticate
}
BYTEV Reader::readPage(BYTE page, const BYTEV* customApdu) {
	BYTEV apdu;
	if (customApdu) apdu = *customApdu;
	else apdu = { 0xFF, 0xB0, 0x00, page, getLE() };
	auto resp = secureTransmit<ReadPolicy>(page, apdu);
	return BYTEV(resp.begin(), resp.end() - 2);
}
void Reader::writePage(BYTE page, const BYTE* data, const BYTEV* customApdu) {
	BYTEV apdu;
	if (customApdu) apdu = *customApdu;  // dýþarýdan gelen APDU
	else {
		// default APDU
		apdu = { 0xFF, 0xD6, 0x00, page, getLE() };
		apdu.insert(apdu.end(), data, data + getLE());
	}
	secureTransmit<WritePolicy>(page, apdu);
}
void Reader::writePage(BYTE page, const BYTEV& data) {
	BYTEV tmp(getLE());
	for (size_t i = 0; i < getLE() && i < data.size(); ++i) tmp[i] = data[i];
	writePage(page, tmp.data());
}
void Reader::writePage(BYTE page, const std::string& s) {
	BYTEV tmp(getLE());
	for (size_t i = 0; i < getLE() && i < s.size(); ++i) tmp[i] = static_cast<BYTE>(s[i]);
	writePage(page, tmp.data());
}
void Reader::clearPage(BYTE page) {
	// Default implementation: write zeros to the page
	BYTEV zeros(getLE(), 0x00);
	writePage(page, zeros.data());
}

void Reader::writePageEncrypted(BYTE page, const BYTEV& data, const ICipher& cipher) {
	BYTEV tmp(getLE());
	for (size_t i = 0; i < getLE() && i < data.size(); ++i) tmp[i] = data[i];
	writePageEncrypted(page, tmp.data(), cipher);
}
void Reader::writePageEncrypted(BYTE page, const std::string& s, const ICipher& cipher) {
	BYTEV tmp(getLE());
	for (size_t i = 0; i < getLE() && i < s.size(); ++i) tmp[i] = static_cast<BYTE>(s[i]);
	writePageEncrypted(page, tmp.data(), cipher);
}

void Reader::writePageEncryptedAAD(BYTE page, const BYTEV& data, const ICipher& cipher, const BYTEV& aad) {
	BYTEV tmp(getLE());
	for (size_t i = 0; i < getLE() && i < data.size(); ++i) tmp[i] = data[i];
	writePageEncryptedAAD(page, tmp.data(), cipher, aad.data(), aad.size());
}
void Reader::writePageEncryptedAAD(BYTE page, const std::string& s, const ICipher& cipher, const BYTEV& aad) {
	BYTEV tmp(getLE());
	for (size_t i = 0; i < getLE() && i < s.size(); ++i) tmp[i] = static_cast<BYTE>(s[i]);
	writePageEncryptedAAD(page, tmp.data(), cipher, aad.data(), aad.size());
}

// --- Multi-page plain write/read ---
void Reader::writeData(BYTE startPage, const BYTEV& data) {
	size_t total = data.size();
	size_t pages = (total + getLE()-1) / getLE();
	for (size_t i = 0; i < pages; ++i) {
		BYTE page = static_cast<BYTE>(startPage + i);
		BYTEV chunk(getLE());
		for (size_t b = 0; b < getLE(); ++b) {
			size_t idx = i * getLE() + b;
			if (idx < total) chunk[b] = data[idx];
		}
		writePage(page, chunk);
	}
}
void Reader::writeData(BYTE startPage, const std::string& s) {
	BYTEV data(s.begin(), s.end());
	writeData(startPage, data);
}
void Reader::writeData(BYTE startPage, const BYTEV& data, BYTE lc) {
	BYTE old = getLE();
	if (old == lc) { writeData(startPage, data); return; }
	setLE(lc);
	writeData(startPage, data);
	setLE(old);
}
void Reader::writeData(BYTE startPage, const std::string& s, BYTE lc) {
	BYTEV data(s.begin(), s.end());
	writeData(startPage, data, lc);
}

BYTEV Reader::readData(BYTE startPage, size_t length) {
	BYTEV out;
	size_t remaining = length;
	BYTE page = startPage;
	while (remaining > 0) {
		auto p = readPage(page);
		if (!p.empty()) out.insert(out.end(), p.begin(), p.end());
		remaining = (remaining > getLE()) ? (remaining - getLE()) : 0;
		++page;
	}
	out.resize(length);
	return out;
}

// --- Multi-page encrypted write/read ---
void Reader::writeDataEncrypted(BYTE startPage, const BYTEV& data, const ICipher& cipher) {
	// Encrypt the entire blob first, then write raw encrypted bytes page-by-page
	auto encrypted = cipher.encrypt(data);
	writeData(startPage, encrypted);
}
void Reader::writeDataEncrypted(BYTE startPage, const std::string& s, const ICipher& cipher) {
	BYTEV data(s.begin(), s.end());
	writeDataEncrypted(startPage, data, cipher);
}

BYTEV Reader::readDataDecrypted(BYTE startPage, size_t length, const ICipher& cipher) {
	// For block ciphers with PKCS7 padding, encrypted size = ((length / blockSize) + 1) * blockSize
	// AES blockSize=16, 3DES blockSize=8. We don't know the block size, so try common sizes.
	// For XorCipher/CaesarCipher (stream-like), encrypted size == length.
	// Strategy: try to encrypt a same-length dummy to determine the actual encrypted size,
	// or simply read enough pages and let the decrypt handle trimming.
	
	// First pass: try with exact length (for stream ciphers)
	// Second pass: try with additional blocks (for block ciphers: +16 covers AES, +8 covers 3DES)
	// We try encrypted sizes in order and return the first successful decrypt.
	
	// Determine how many bytes were actually written by doing a trial encrypt
	BYTEV dummy(length, 0);
	size_t encLen;
	try {
		auto trial = cipher.encrypt(dummy);
		encLen = trial.size();
	}
	catch (...) {
		encLen = length; // fallback for stream ciphers
	}

	size_t pages = (encLen + getLE()-1) / getLE();
	BYTEV raw;
	for (size_t i = 0; i < pages; ++i) {
		try {
			auto p = readPage(static_cast<BYTE>(startPage + i), getLE());
			raw.insert(raw.end(), p.begin(), p.end());
		}
		catch (...) {
			break;
		}
	}

	if (raw.empty()) return raw;

	// Trim to exact encrypted size (pages may have trailing zeros)
	if (raw.size() > encLen) raw.resize(encLen);

	auto decrypted = cipher.decrypt(raw);
	if (decrypted.size() > length) decrypted.resize(length);
	return decrypted;
}

// --- Multi-page AAD-capable encrypted write/read ---
void Reader::writeDataEncryptedAAD(BYTE startPage, const BYTEV& data, const ICipher& cipher, const BYTE* aad, size_t aad_len) {
	// Encrypt the entire blob with AAD, then write raw encrypted bytes page-by-page
	auto encrypted = cipher.encrypt(data.data(), data.size(), aad, aad_len);
	writeData(startPage, encrypted);
}
void Reader::writeDataEncryptedAAD(BYTE startPage, const std::string& s, const ICipher& cipher, const BYTE* aad, size_t aad_len) {
	BYTEV data(s.begin(), s.end());
	writeDataEncryptedAAD(startPage, data, cipher, aad, aad_len);
}

BYTEV Reader::readDataDecryptedAAD(BYTE startPage, size_t length, const ICipher& cipher, const BYTE* aad, size_t aad_len) {
	// Determine encrypted blob size by trial encrypt
	BYTEV dummy(length, 0);
	size_t encLen;
	try {
		auto trial = cipher.encrypt(dummy.data(), dummy.size(), aad, aad_len);
		encLen = trial.size();
	}
	catch (...) {
		encLen = length;
	}

	size_t pages = (encLen + getLE()-1) / getLE();
	BYTEV raw;
	for (size_t i = 0; i < pages; ++i) {
		try {
			auto p = readPage(static_cast<BYTE>(startPage + i), getLE());
			raw.insert(raw.end(), p.begin(), p.end());
		}
		catch (...) {
			break;
		}
	}

	if (raw.empty()) return raw;

	if (raw.size() > encLen) raw.resize(encLen);

	auto decrypted = cipher.decrypt(raw.data(), raw.size(), aad, aad_len);
	if (decrypted.size() > length) decrypted.resize(length);
	return decrypted;
}

// --- Read all pages until error ---
BYTEV Reader::readAll(BYTE startPage) {
	BYTEV out;
	BYTE page = startPage;
	while (true) {
		try {
			auto p = readPage(page);
			if (p.empty()) break;
			out.insert(out.end(), p.begin(), p.end());
			++page;
		}
		catch (...) {
			break;
		}
	}
	return out;
}

// --- Authenticate with explicit 5-byte data (for readers that support this mode) ---
void Reader::authNew(const BYTE data[5]) {
	cardConnection().checkConnected();
	BYTE authLC = 0x05; // 5 bytes
	BYTEV apdu{ 0xFF, 0x86, 0x00, 0x00, authLC };
	apdu.insert(apdu.end(), data, data + authLC);

	BYTEV resp = cardConnection().transmit(apdu);
	cardConnection().checkResponseSize(resp);
	auto sw = cardConnection().getStatusWords(resp);
	BYTE sw1 = sw.first, sw2 = sw.second;
	// Throw if neither success nor auth-failed sentinel
	if (! ( ((sw1 == 0x90 || sw1 == 0x91) && sw2 == 0x00) || (sw1 == 0x63 && sw2 == 0x00) ) ) {
		std::stringstream ss;
		ss << "Auth failed SW=0x" << std::hex << (int)sw1 << " 0x" << (int)sw2 << '\n';
		throw pcsc::AuthFailedError(ss.str());
	}
}
