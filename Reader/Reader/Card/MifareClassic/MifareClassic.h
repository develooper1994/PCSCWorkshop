#pragma once
#include <cstdint>
#include <cstring>
#include <vector>
#include <array>
#include <map>
#include <stdexcept>
#include "CipherTypes.h"
#include "../../Reader.h"

//
// Reader interface expected methods (match ACR1281UReader):
//  void loadKey(const BYTE* key, KeyStructure keyStructure, BYTE keyNumber);
//  void auth(BYTE block, BYTE keyTypeOr60or61, BYTE keyNumber);
//  std::vector<BYTE> readPage(BYTE block);         // returns 16 bytes or throws
//  void writePage(BYTE block, const BYTE data[16]); // may throw
//  bool isAuthRequested() const; // optional helper, if not present assume true
//
// The template allows any Reader-like type (ACR1281UReader).
//
template<typename ReaderT>
class MifareCardCore {
public:
    using KeyType = ::KeyType;
    using KeyStructure = ::KeyStructure;

    struct SectorKeyConfig {
        bool keyA_canRead = true;
        bool keyA_canWrite = false;
        bool keyB_canRead = true;
        bool keyB_canWrite = true;
    };

    explicit MifareCardCore(ReaderT& r, bool is4K = false)
        : reader(r), is4KCard(is4K)
    {
        // default: for all sectors KeyA read-only, KeyB read+write
        int sectors = is4KCard ? 40 : 16;
        for (int i = 0; i < sectors; ++i) {
            sectorConfigs[i] = SectorKeyConfig{};
        }
        authSector = -1;
    }

    // store key material + keyStructure + slot number for future load
    void setKey(KeyType kt, const std::array<BYTE, 6>& key, KeyStructure keyStructure, BYTE keyNumber) {
        KeyInfo ki;
        ki.key = key;
        ki.ks = keyStructure;
        ki.slot = keyNumber;
        keys[kt] = ki;
    }

    void setSectorConfig(int sector, const SectorKeyConfig& cfg) {
        validateSectorNumber(sector);
        sectorConfigs[sector] = cfg;
    }

    int blockToSector(BYTE block) const
    {
        int b = static_cast<int>(block);

        if (!is4KCard)
            return b / 4;

        if (b < 128)
            return b / 4;

        return 32 + (b - 128) / 16;
    }

    void ensureAuth(BYTE block,
        KeyType keyType,
        BYTE keySlot)
    {
        int sector = blockToSector(block);

        if (authSector == sector &&
            authType == keyType &&
            authSlot == keySlot)
            return;

        // gerçek auth çaðrýsý
        reader.auth(block, keyType, keySlot);

        authSector = sector;
        authType = keyType;
        authSlot = keySlot;
    }

    std::vector<BYTE> read(BYTE block,
        KeyType keyType,
        BYTE keySlot)
    {
        ensureAuth(block, keyType, keySlot);
        return reader.readPage(block);
    }

    std::vector<BYTE> read(
        BYTE block,
        BYTE blockSize,
        const BYTE* key,
        KeyType keyType,
        BYTE keySlot)
    {
        // key yükle
        reader.loadKey(key, KeyStructure::NonVolatile, keySlot);

        ensureAuth(block, keyType, keySlot);

        return reader.readPage(block);
    }

    void write(BYTE block,
        const std::vector<BYTE>& data,
        KeyType keyType,
        BYTE keySlot)
    {
        ensureAuth(block, keyType, keySlot);
        reader.writePage(block, data);
    }

    void write(BYTE block,
        const std::string& text,
        KeyType keyType,
        BYTE keySlot)
    {
        std::vector<BYTE> data(text.begin(), text.end());
        write(block, data, keyType, keySlot);
    }

    void write(
        BYTE block,
        BYTE blockSize,
        const BYTE* data,
        KeyType keyType,
        BYTE keySlot)
    {
        ensureAuth(block, keyType, keySlot);

        std::vector<BYTE> buffer(data, data + blockSize);

        reader.writeData(block, buffer);
    }

    // Linear read: startBlock (absolute), read length bytes (<= large). Returns exactly 'length' bytes (last page may be partial)
    BYTEV readLinear(BYTE startBlock, size_t length) {
        BYTEV out;
        out.reserve(length);
        size_t remaining = length;
        int block = static_cast<int>(startBlock);

        while (remaining > 0) {
            // validate block exists
            validateBlockNumber(block);

            // don't allow reading trailer blocks with this convenience read
            if (isTrailerBlock(static_cast<BYTE>(block)))
                throw std::runtime_error("readLinear: encountered trailer block");

            // determine sector
            int sector = sectorFromBlock(static_cast<BYTE>(block));

            // choose auth for read (prefer KeyA if allowed, otherwise KeyB)
            KeyType chosen = chooseKeyForOperation(sector, /*isWrite=*/false);

            // ensure authorization for this sector with chosen key and its configured slot
            const KeyInfo& ki = findKeyOrThrow(chosen);
            ensureAuthorized(sector, chosen, ki.slot);

            // perform read (may throw)
            std::vector<BYTE> page = reader.readPage(static_cast<BYTE>(block));
            if (page.size() < 16) throw std::runtime_error("readLinear: short page read");

            size_t take = std::min<size_t>(16, remaining);
            out.insert(out.end(), page.begin(), page.begin() + take);

            remaining -= take;
            ++block;
        }

        return out;
    }

    // Linear write: writes all bytes, writing full pages (last page padded with zeroes)
    // Throws on trailer/manufacturer blocks (unless you split around them)
    void writeLinear(BYTE startBlock, const BYTEV& data) {
        size_t remaining = data.size();
        size_t offset = 0;
        int block = static_cast<int>(startBlock);

        while (remaining > 0) {
            validateBlockNumber(block);

            if (isTrailerBlock(static_cast<BYTE>(block)))
                throw std::runtime_error("writeLinear: encountered trailer block");

            if (isManufacturerBlock(static_cast<BYTE>(block)))
                throw std::runtime_error("writeLinear: manufacturer block is read-only");

            int sector = sectorFromBlock(static_cast<BYTE>(block));

            // select key for write: prefer KeyB if allowed; otherwise, if KeyA allowed use KeyA
            KeyType chosen = chooseKeyForOperation(sector, /*isWrite=*/true);

            const KeyInfo& ki = findKeyOrThrow(chosen);

            // Ensure auth with chosen key/slot
            ensureAuthorized(sector, chosen, ki.slot);

            BYTE page[16];
            std::memset(page, 0, sizeof(page));
            size_t take = std::min<size_t>(16, remaining);
            std::memcpy(page, data.data() + offset, take);

            // perform write (may throw). If exception, auth cache remains as before (we only set cache on successful auth)
            reader.writePage(static_cast<BYTE>(block), page);

            remaining -= take;
            offset += take;
            ++block;
        }
    }

    // Convenience: read single block (returns exactly 16 bytes)
    std::array<BYTE, 16> readBlock(BYTE block) {
        validateBlockNumber(block);
        if (isTrailerBlock(block))
            throw std::runtime_error("readBlock: trailer block - use readTrailer");

        int sector = sectorFromBlock(block);
        KeyType chosen = chooseKeyForOperation(sector, false);
        const KeyInfo& ki = findKeyOrThrow(chosen);
        ensureAuthorized(sector, chosen, ki.slot);

        std::vector<BYTE> page = reader.readPage(block);
        if (page.size() < 16) throw std::runtime_error("readBlock: short read");
        std::array<BYTE, 16> out;
        std::memcpy(out.data(), page.data(), 16);
        return out;
    }

    // Convenience: write single block (16 bytes)
    void writeBlock(BYTE block, const BYTE data[16]) {
        validateBlockNumber(block);
        if (isTrailerBlock(block))
            throw std::runtime_error("writeBlock: trailer block - use changeKeys");

        if (isManufacturerBlock(block))
            throw std::runtime_error("writeBlock: manufacturer block is read-only");

        int sector = sectorFromBlock(block);
        KeyType chosen = chooseKeyForOperation(sector, true);
        const KeyInfo& ki = findKeyOrThrow(chosen);
        ensureAuthorized(sector, chosen, ki.slot);

        reader.writePage(block, data); // may throw
    }

    // Read trailer (raw)
    std::array<BYTE, 16> readTrailer(int sector) {
        validateSectorNumber(sector);
        int first = firstBlockOfSector(sector);
        BYTE trailer = static_cast<BYTE>(first + blocksPerSector(sector) - 1);

        KeyType chosen = chooseKeyForOperation(sector, false);
        const KeyInfo& ki = findKeyOrThrow(chosen);
        ensureAuthorized(sector, chosen, ki.slot);

        std::vector<BYTE> page = reader.readPage(trailer);
        if (page.size() < 16) throw std::runtime_error("readTrailer: short read");
        std::array<BYTE, 16> out;
        std::memcpy(out.data(), page.data(), 16);
        return out;
    }

    // Change trailer (atomic - caller must supply full 16-byte trailer)
    // Validates access bits inverted pattern before writing.
    void changeTrailer(int sector, const BYTE trailer16[16]) {
        validateSectorNumber(sector);
        // check access bits pattern
        if (!validateAccessBits(trailer16 + 6))
            throw std::runtime_error("changeTrailer: invalid access bits");

        // choose auth to perform the write (must authenticate with existing key first)
        // chosen should be the key type allowed to write trailers by your policy; here we pick KeyB by default
        KeyType chosen = KeyType::B;
        const KeyInfo& ki = findKeyOrThrow(chosen);
        ensureAuthorized(sector, chosen, ki.slot);

        int first = firstBlockOfSector(sector);
        BYTE trailer = static_cast<BYTE>(first + blocksPerSector(sector) - 1);

        reader.writePage(trailer, trailer16); // may throw
        // After changing trailer keys, invalidate cached auth for this sector (keys changed)
        invalidateAuthForSector(sector);
    }

private:
    ReaderT& reader;
    bool is4KCard;

    // --- AUTH CACHE MEMBERS (ekle bunlarý!) ---
    int authSector = -1;       // currently authenticated sector, -1 == none
    BYTE authSlot = 0xFF;    // slot used for last auth
    KeyType authType = KeyType::A;

    // --- auth cache map (per-sector) --- (opsiyonel; senin kodun map kullandýysa onu da koru)
    std::map<int, KeyType> authorizedSectors;

    struct KeyInfo {
        std::array<BYTE, 6> key{};
        KeyStructure ks = KeyStructure::KeyA;
        BYTE slot = 0x00;
    };

    std::map<KeyType, KeyInfo> keys;
    std::map<int, SectorKeyConfig> sectorConfigs;

    // cached auth state for sector -> (KeyType, slot)
    struct AuthState { KeyType kt; BYTE slot; };
    std::map<int, AuthState> authCache;
    int authSectorCurrently = -1; // not strictly necessary (kept for clarity)

    // ===== helpers: layout (1K/4K aware) =====
    int sectorFromBlock(BYTE block) const {
        int b = static_cast<int>(block);
        if (!is4KCard) return b / 4;
        if (b < 128) return b / 4;
        return 32 + (b - 128) / 16;
    }
    int blocksPerSector(int sector) const {
        if (!is4KCard) return 4;
        if (sector < 32) return 4;
        return 16;
    }
    int firstBlockOfSector(int sector) const {
        if (!is4KCard) return sector * 4;
        if (sector < 32) return sector * 4;
        return 128 + (sector - 32) * 16;
    }
    bool isTrailerBlock(BYTE block) const {
        int sector = sectorFromBlock(block);
        int first = firstBlockOfSector(sector);
        return block == static_cast<BYTE>(first + blocksPerSector(sector) - 1);
    }
    bool isManufacturerBlock(BYTE block) const {
        return (!is4KCard && block == 0) || (is4KCard && block == 0); // same: absolute block 0
    }
    void validateBlockNumber(int block) const {
        if (block < 0) throw std::out_of_range("block < 0");
        int maxBlocks = is4KCard ? (128 + (40 - 32) * 16) : (16 * 4);
        if (block >= maxBlocks) throw std::out_of_range("block out of range");
    }
    void validateSectorNumber(int sector) const {
        int maxSectors = is4KCard ? 40 : 16;
        if (sector < 0 || sector >= maxSectors) throw std::out_of_range("sector out of range");
    }

    // choose which key to use for operation on sector
    // for reads prefer KeyA if allowed, else KeyB (must be configured)
    // for writes prefer KeyB if allowed, else KeyA
    KeyType chooseKeyForOperation(int sector, bool isWrite) const {
        auto it = sectorConfigs.find(sector);
        if (it == sectorConfigs.end())
            throw std::runtime_error("sector config not set");

        const SectorKeyConfig& cfg = it->second;
        if (isWrite) {
            if (cfg.keyB_canWrite) return KeyType::B;
            if (cfg.keyA_canWrite) return KeyType::A;
            throw std::runtime_error("no key allowed to write in this sector");
        }
        else {
            if (cfg.keyA_canRead) return KeyType::A;
            if (cfg.keyB_canRead) return KeyType::B;
            throw std::runtime_error("no key allowed to read in this sector");
        }
    }

    // find key info or throw
    const KeyInfo& findKeyOrThrow(KeyType kt) const {
        auto it = keys.find(kt);
        if (it == keys.end())
            throw std::runtime_error("key not set for requested KeyType");
        return it->second;
    }

    // Ensure authorized for sector using chosen (kt,slot)
    // This function is exception-safe: it only updates authCache if loadKey and auth succeed.
    void ensureAuthorized(int sector, KeyType kt, BYTE slot) {
        // if already cached and matches kt+slot -> done
        auto cit = authCache.find(sector);
        if (cit != authCache.end()) {
            if (cit->second.kt == kt && cit->second.slot == slot) return;
            // else fallthrough and re-auth with requested credentials
        }

        // get KeyInfo for kt
        const KeyInfo& ki = findKeyOrThrow(kt);

        // loadKey may throw; allow exception to propagate
        reader.loadKey(ki.key.data(), ki.ks, ki.slot);

        // perform auth: Reader.auth expects (block, keyTypeByte, keyNumber)
        // choose a representative block in sector to authenticate (use first block)
        int firstBlock = firstBlockOfSector(sector);
        BYTE authBlock = static_cast<BYTE>(firstBlock);

        // call auth; may throw
        reader.auth(authBlock, static_cast<BYTE>(kt), ki.slot);

        // if we reach here, auth succeeded: update cache
        AuthState st; st.kt = kt; st.slot = ki.slot;
        authCache[sector] = st;
    }

    void invalidateAuthForSector(int sector) {
        authCache.erase(sector);
    }

    // Access bits inverted pattern validation (same logic as before)
    static bool validateAccessBits(const BYTE a[4]) {
        BYTE b6 = a[0], b7 = a[1], b8 = a[2];
        BYTE c1[4], c2[4], c3[4];
        c1[0] = (b6 >> 4) & 1; c1[1] = (b6 >> 5) & 1; c1[2] = (b6 >> 6) & 1; c1[3] = (b6 >> 7) & 1;
        c2[0] = (b6 >> 0) & 1; c2[1] = (b6 >> 1) & 1; c2[2] = (b6 >> 2) & 1; c2[3] = (b6 >> 3) & 1;
        c3[0] = (b7 >> 4) & 1; c3[1] = (b7 >> 5) & 1; c3[2] = (b7 >> 6) & 1; c3[3] = (b7 >> 7) & 1;

        if ((b7 & 0x0F) != (((~c1[0] & 1) << 0) | ((~c1[1] & 1) << 1) | ((~c1[2] & 1) << 2) | ((~c1[3] & 1) << 3))) return false;
        if (((b8 >> 4) & 0x0F) != (((~c2[0] & 1) << 0) | ((~c2[1] & 1) << 1) | ((~c2[2] & 1) << 2) | ((~c2[3] & 1) << 3))) return false;
        if ((b8 & 0x0F) != (((~c3[0] & 1) << 0) | ((~c3[1] & 1) << 1) | ((~c3[2] & 1) << 2) | ((~c3[3] & 1) << 3))) return false;
        return true;
    }
};