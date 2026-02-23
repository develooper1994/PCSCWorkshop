#ifndef PCSC_WORKSHOP1_READER_H
#define PCSC_WORKSHOP1_READER_H

#include "CardConnection.h"
#include "Cipher.h"
#include <string>
#include <memory>

enum class KeyStructure : BYTE {
    Volatile,
    NonVolatile
};

enum class KeyType {
    A,
    B
};

// Reader abstraction: each reader model can implement its own read/write APDUs
class Reader {
public:
    explicit Reader(CardConnection& c);
    virtual ~Reader();

    // Non-copyable, movable
    Reader(const Reader&) = delete;
    Reader& operator=(const Reader&) = delete;
    Reader(Reader&&) noexcept;
    Reader& operator=(Reader&&) noexcept;

    // Core primitive that derived classes must implement (4 bytes)
    virtual void writePage(BYTE page, const BYTE* data4, BYTE LC = 0x04) = 0;
    virtual BYTEV readPage(BYTE page, BYTE LC = 0x04) = 0;

    // Encrypted write/read
    void writePageEncrypted(BYTE page, const BYTE* data4, const ICipher& cipher, BYTE LC = 0x04);
    BYTEV readPageDecrypted(BYTE page, const ICipher& cipher, BYTE LC = 0x04);

    // AAD-capable single-page encrypted write/read
    void writePageEncryptedAAD(BYTE page, const BYTE* data4, const ICipher& cipher, const BYTE* aad, size_t aad_len, BYTE LC = 0x04);
    BYTEV readPageDecryptedAAD(BYTE page, const ICipher& cipher, const BYTE* aad, size_t aad_len, BYTE LC = 0x04);

    // Convenience overloads for common container types
    void writePage(BYTE page, const BYTEV& data, BYTE LC = 0x04);
    void writePage(BYTE page, const std::string& s, BYTE LC = 0x04);

    void writePageEncrypted(BYTE page, const BYTEV& data, const ICipher& cipher, BYTE LC = 0x04);
    void writePageEncrypted(BYTE page, const std::string& s, const ICipher& cipher, BYTE LC = 0x04);

    void writePageEncryptedAAD(BYTE page, const BYTEV& data, const ICipher& cipher, const BYTEV& aad, BYTE LC = 0x04);
    void writePageEncryptedAAD(BYTE page, const std::string& s, const ICipher& cipher, const BYTEV& aad, BYTE LC = 0x04);

    // convenience: write multi-page data
    virtual void writeData(BYTE startPage, const BYTEV& data, BYTE LC = 4);
    void writeData(BYTE startPage, const std::string& s, BYTE LC = 4);

    virtual BYTEV readData(BYTE startPage, size_t length, BYTE LC = 4);

    // Multi-page encrypted write/read
    void writeDataEncrypted(BYTE startPage, const BYTEV& data, const ICipher& cipher, BYTE LC = 4);
    void writeDataEncrypted(BYTE startPage, const std::string& s, const ICipher& cipher, BYTE LC = 4);
    BYTEV readDataDecrypted(BYTE startPage, size_t length, const ICipher& cipher, BYTE LC = 4);

    void writeDataEncryptedAAD(BYTE startPage, const BYTEV& data, const ICipher& cipher, const BYTE* aad, size_t aad_len, BYTE LC = 4);
    void writeDataEncryptedAAD(BYTE startPage, const std::string& s, const ICipher& cipher, const BYTE* aad, size_t aad_len, BYTE LC = 4);
    BYTEV readDataDecryptedAAD(BYTE startPage, size_t length, const ICipher& cipher, const BYTE* aad, size_t aad_len, BYTE LC = 4);

    // Read all pages until card returns error
    BYTEV readAll(BYTE startPage = 0);

    /* ACR1281U specific operations:
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
    template<typename TReader>
    void loadKey(const BYTE* key, KeyStructure keyStructure, BYTE keyNumber) {
        if (!card().isConnected()) throw std::runtime_error("Card not connected");
        // BYTE keyStructureValue = mapKeyStructure(keyStructure);
        BYTE keyStructureValue = KeyMapping<TReader>::map(keyStructure);

        BYTE LC = 0x06; // 6 bytes
        BYTEV apdu{ 0xFF, 0x82, keyStructureValue, keyNumber, LC };
        apdu.insert(apdu.end(), key, key + LC);

        auto resp = card().transmit(apdu);
        if (resp.size() < 2) throw std::runtime_error("Invalid response for write");
        BYTE sw1 = resp[resp.size() - 2], sw2 = resp[resp.size() - 1];
        if (!((sw1 == 0x90 || sw1 == 0x91) && sw2 == 0x00)) {
            std::stringstream ss;
            ss << "Write failed SW=0x" << std::hex << (int)sw1 << " 0x" << (int)sw2 << '\n';
            throw std::runtime_error(ss.str());
        }
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
    PC. Only 1 volatile memory is provided. The volatile key can be used
    as a session key for different sessions. Default value = FF FF FF FF
    FF FFh.
    */
    template<typename TReader>
    void authKey(BYTE blockNumber, KeyType keyType, BYTE keyNumber) {
        if (!card().isConnected()) throw std::runtime_error("Card not connected");
        // BYTE keyTypeValue = mapKeyKind(keyType);
        BYTE keyTypeValue = KeyMapping<TReader>::map(keyType);

        BYTEV apdu{ 0xFF, 0x88, 0x00, blockNumber, keyTypeValue, keyNumber };

        auto resp = card().transmit(apdu);
        if (resp.size() < 2) throw std::runtime_error("Invalid response for write");
        BYTE sw1 = resp[resp.size() - 2], sw2 = resp[resp.size() - 1];
        if (!((sw1 == 0x90 || sw1 == 0x91) && sw2 == 0x00)) {
            std::stringstream ss;
            ss << "Write failed SW=0x" << std::hex << (int)sw1 << " 0x" << (int)sw2 << '\n';
            throw std::runtime_error(ss.str());
        }
    }
    /* {0x01(versionnumber), 0x00, Block Number, Key Type, Key Number} */
    template<typename TReader>
    void authKeyNew(BYTE blockNumber, KeyType keyType, BYTE keyNumber) {
        // BYTE keyTypeValue = mapKeyKind(keyType);
        BYTE keyTypeValue = KeyMapping<TReader>::map(keyType);
        BYTE data5[5] = { 0x01, 0x00, blockNumber, keyTypeValue, keyNumber };
        authKeyNew(data5);
    }
    void authKeyNew(const BYTE* data5);

protected:
    struct Impl;
    std::unique_ptr<Impl> pImpl;

    // Derived classes access CardConnection through this
    CardConnection& card();
    const CardConnection& card() const;

    virtual BYTE mapKeyStructure(KeyStructure structure) const = 0;
    virtual BYTE mapKeyKind(KeyType kind) const = 0;

    template<typename TReader>
    struct KeyMapping;
};

#endif // PCSC_WORKSHOP1_READER_H
