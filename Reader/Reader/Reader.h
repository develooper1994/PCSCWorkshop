#ifndef PCSC_WORKSHOP1_READER_H
#define PCSC_WORKSHOP1_READER_H

#include "CardConnection.h"
#include "Cipher.h"
#include <string>
#include <memory>
#include <functional>

enum class KeyStructure : BYTE {
	Volatile,
	NonVolatile
};

// [keyA(6 Byte)|AccessBytes(4 Byte)|keyB(6 Byte)|]
enum class KeyType {
	A,		// keyA
	B,		// keyB
	ACCESS, // AccessBytes
	AB,	// [keyA(6 Byte)|-|keyB(6 Byte)|]
	ALL,	// [keyA(6 Byte)|AccessBytes(4 Byte)|keyB(6 Byte)|]
};

struct ReadPolicy {
	static void handle(BYTE sw1, BYTE sw2) {
		std::stringstream ss;
		ss << "Read failed SW=0x"
			<< std::hex << (int)sw1 << " 0x" << (int)sw2;
		throw pcsc::ReaderReadError(ss.str());
	}
};

struct WritePolicy {
	static void handle(BYTE sw1, BYTE sw2) {
		std::stringstream ss;
		ss << "Write failed SW=0x"
			<< std::hex << (int)sw1 << " 0x" << (int)sw2;
		throw pcsc::ReaderWriteError(ss.str());
	}
};

// Reader abstraction: each reader model can implement its own read/write APDUs
class Reader {
public:
	virtual ~Reader();

	explicit Reader(CardConnection& c);
	// New overload: construct Reader and initialize Impl fields
	Reader(CardConnection& c, BYTE lc, bool authRequested, KeyType kt, KeyStructure ks, BYTE keyNumber, const BYTE key[6]);
	Reader(CardConnection& c, BYTE lc, bool authRequested, KeyType kt, KeyStructure ks, BYTE keyNumber, const BYTE keyA[6], const BYTE keyB[6]);
	// New overload: accept explicit 4-byte access bits between keyA and keyB
	Reader(CardConnection& c, BYTE lc, bool authRequested, KeyType kt, KeyStructure ks, BYTE keyNumber, const BYTE keyA[6], const BYTE accessBits[4], const BYTE keyB[6]);
	// Non-copyable, movable
	Reader(const Reader&) = delete;
	Reader& operator=(const Reader&) = delete;
	Reader(Reader&&) noexcept;
	Reader& operator=(Reader&&) noexcept;

	// Core primitive that derived classes must implement
	virtual void performAuth(BYTE page);
	template<typename Policy>
	BYTEV secureTransmit(BYTE page, const BYTEV& apdu)
	{
		cardConnection().checkConnected();
		if (isAuthRequested()) performAuth(page);

		auto resp = cardConnection().transmit(apdu);
		if (resp.size() < 2) throw pcsc::ReaderError("Invalid response");
		BYTE sw1 = resp[resp.size() - 2], sw2 = resp[resp.size() - 1];
		if (sw1 == 0x63 && sw2 == 0x00) {
			setAuthRequested(true);
			throw pcsc::AuthFailedError("Auth failed");
		} 
		else if (!((sw1 == 0x90 || sw1 == 0x91) && sw2 == 0x00)) {
			setAuthRequested(true);
			Policy::handle(sw1, sw2);   // 👈 compile-time dispatch
		}
		setAuthRequested(false);
		return resp;
	}
	virtual void writePage(BYTE page, const BYTE* data, const BYTEV* customApdu = nullptr);
	virtual void clearPage(BYTE page);
	virtual BYTEV readPage(BYTE page, const BYTEV* customApdu = nullptr);

	// New overload: read a page requesting a specific length. Default implementation
	// forwards to the single-argument virtual readPage; derived readers may override
	// to implement reader-specific APDU behavior.
	virtual BYTEV readPage(BYTE page, BYTE len);

	// Encrypted write/read
	void writePageEncrypted(BYTE page, const BYTE* data, const ICipher& cipher);
	BYTEV readPageDecrypted(BYTE page, const ICipher& cipher);

	// AAD-capable single-page encrypted write/read
	void writePageEncryptedAAD(BYTE page, const BYTE* data, const ICipher& cipher, const BYTE* aad, size_t aad_len);
	BYTEV readPageDecryptedAAD(BYTE page, const ICipher& cipher, const BYTE* aad, size_t aad_len);

	// Convenience overloads for common container types
	void writePage(BYTE page, const BYTEV& data);
	void writePage(BYTE page, const std::string& s);

	void writePageEncrypted(BYTE page, const BYTEV& data, const ICipher& cipher);
	void writePageEncrypted(BYTE page, const std::string& s, const ICipher& cipher);

	void writePageEncryptedAAD(BYTE page, const BYTEV& data, const ICipher& cipher, const BYTEV& aad);
	void writePageEncryptedAAD(BYTE page, const std::string& s, const ICipher& cipher, const BYTEV& aad);

	// convenience: write multi-page data
	virtual void writeData(BYTE startPage, const BYTEV& data);
	void writeData(BYTE startPage, const std::string& s);

	// New overloads: allow caller to specify LC (bytes per page) for this operation
	void writeData(BYTE startPage, const BYTEV& data, BYTE lc);
	void writeData(BYTE startPage, const std::string& s, BYTE lc);

	virtual BYTEV readData(BYTE startPage, size_t length);

	// Multi-page encrypted write/read
	void writeDataEncrypted(BYTE startPage, const BYTEV& data, const ICipher& cipher);
	void writeDataEncrypted(BYTE startPage, const std::string& s, const ICipher& cipher);
	BYTEV readDataDecrypted(BYTE startPage, size_t length, const ICipher& cipher);

	void writeDataEncryptedAAD(BYTE startPage, const BYTEV& data, const ICipher& cipher, const BYTE* aad, size_t aad_len);
	void writeDataEncryptedAAD(BYTE startPage, const std::string& s, const ICipher& cipher, const BYTE* aad, size_t aad_len);
	BYTEV readDataDecryptedAAD(BYTE startPage, size_t length, const ICipher& cipher, const BYTE* aad, size_t aad_len);

	// Read all pages until card returns error
	BYTEV readAll(BYTE startPage = 0);

	/* 
	* Load key to card's volatile or non-volatile memory, then use it for authentication. The actual APDU structure and parameters depend on the reader model.
	ACR1281U specific operations:
	keyStructure(1 byte):
	0x00 : Volatile memory (default)
	0x20 : Non-volatile memory (EEPROM)

	keyNumber(1 byte):
	00h – 1Fh = Non-volatile memory for storing keys. The keys are
	permanently stored in the reader and will not be erased even if the
	reader is disconnected from the PC. It can store up to 32 keys inside
	the reader non-volatile memory.
	20h (Session Key) = Volatile memory for temporarily storing keys.
	The keys will be erased when the reader is disconnected from the
	PC. Only one volatile memory is provided. The volatile key can be
	used as a session key for different sessions. Default value = FF FF
	FF FF FF FFh.
	*/
	void loadKey(const BYTE* key, KeyStructure keyStructure, BYTE keyNumber) {
		if (!cardConnection().isConnected()) throw pcsc::ReaderError("Card not connected");
		BYTE keyStructureValue = mapKeyStructure(keyStructure);

		BYTE LC = 0x06; // 6 bytes
		BYTEV apdu{ 0xFF, 0x82, keyStructureValue, keyNumber, LC };
		apdu.insert(apdu.end(), key, key + LC);

		auto resp = cardConnection().transmit(apdu);
		if (resp.size() < 2) throw pcsc::ReaderError("Invalid response for write");
		BYTE sw1 = resp[resp.size() - 2], sw2 = resp[resp.size() - 1];
		if (!((sw1 == 0x90 || sw1 == 0x91) && sw2 == 0x00)) {
			std::stringstream ss;
			ss << "LoadKey failed SW=0x" << std::hex << (int)sw1 << " 0x" << (int)sw2 << '\n';
			throw pcsc::LoadKeyFailedError(ss.str());
		}
	}
	void loadKeyA(const BYTE* key, KeyStructure keyStructure, BYTE keyNumber) {
		loadKey(key, keyStructure, keyNumber);
		setKeyLoaded(true);
	}
	void loadKeyB(const BYTE* key, KeyStructure keyStructure, BYTE keyNumber) {
		loadKey(key, keyStructure, keyNumber);
		setKeyLoaded(true);
	}
	/*
	keyType(1 byte):
	A -> 0x60, B -> 0x61
	keyNumber(1 byte):
	00h – 1Fh = Non-volatile memory for storing keys. The keys are
	permanently stored in the reader and will not be erased even if the
	reader is disconnected from the PC. It can store up to 32 keys inside
	the reader non-volatile memory.
	20h (Session Key) = Volatile memory for temporarily storing keys.
	The keys will be erased when the reader is disconnected from the
	PC. Only 1 volatile memory is provided. The volatile key can be
	used as a session key for different sessions. Default value = FF FF FF FF
	FF FFh.
	*/
	void auth(BYTE blockNumber, KeyType keyType, BYTE keyNumber) {
		if (!cardConnection().isConnected()) throw pcsc::ReaderError("Card not connected");
		BYTE keyTypeValue = mapKeyKind(keyType);

		BYTEV apdu{ 0xFF, 0x88, 0x00, blockNumber, keyTypeValue, keyNumber };

		auto resp = cardConnection().transmit(apdu);
		if (resp.size() < 2) throw pcsc::ReaderError("Invalid response for write");
		BYTE sw1 = resp[resp.size() - 2], sw2 = resp[resp.size() - 1];
		if (!((sw1 == 0x90 || sw1 == 0x91) && sw2 == 0x00) || (sw1 == 0x63 && sw2==0x00)) {
			std::stringstream ss;
			ss << "Auth failed SW=0x" << std::hex << (int)sw1 << " 0x" << (int)sw2 << '\n';
			throw pcsc::AuthFailedError(ss.str());
		}
	}
	/* {0x01(versionnumber), 0x00, Block Number, Key Type, Key Number} */
	template<typename TReader>
	void authNew(BYTE blockNumber, KeyType keyType, BYTE keyNumber) {
		// BYTE keyTypeValue = mapKeyKind(keyType);
		BYTE keyTypeValue = KeyMapping<TReader>::map(keyType);
		BYTE data5[5] = { 0x01, 0x00, blockNumber, keyTypeValue, keyNumber };
		authNew(data5);
	}
	void authNew(const BYTE* data5);

	bool isAuthRequested() const;
	void setAuthRequested(bool v);

	BYTE getLC() const;
	void setLC(BYTE lc);

	KeyType keyType() const;
	void setKeyType(KeyType kt);

	KeyStructure keyStructure() const;
	void setKeyStructure(KeyStructure ks);

	BYTE getKeyNumber() const;
	void setKeyNumber(BYTE key); // copies 6 bytes

	// Full 16-byte key storage accessor: [keyA(6)|accessbits(4)|keyB(6)]
	void getKeyAll(BYTE out16[16]) const;
	void setKeyAll(const BYTE* key16); // copies 16 bytes

	// Convenience helpers to get/set 6-byte Key A or Key B from the 16-byte blob
	void getKey(KeyType keyType, BYTE out6[6]) const; // copies 6 bytes
	void setKey(KeyType keyType, const BYTE* key6); // copies 6 bytes into internal blob

	bool keyLoaded() const;
	void setKeyLoaded(bool loaded);

protected:
	struct Impl;
	std::unique_ptr<Impl> pImpl;

	// Derived classes access CardConnection through this
	CardConnection& cardConnection();
	const CardConnection& cardConnection() const;

	virtual BYTE mapKeyStructure(KeyStructure structure) const = 0;
	virtual BYTE mapKeyKind(KeyType kind) const = 0;

	template<typename TReader>
	struct KeyMapping;
};

#endif // PCSC_WORKSHOP1_READER_H
