#ifndef MIFARECLASSIC_H
#define MIFARECLASSIC_H

#include <cstdint>
#include <cstring>
#include <vector>
#include <array>
#include <map>
#include <stdexcept>
#include "CipherTypes.h"
#include "../../Reader.h"
#include <string>

constexpr int SMALL_SECTOR_COUNT = 32;
constexpr int SMALL_SECTOR_PAGES = 4;
constexpr int SMALL_SECTORS_TOTAL_PAGES = SMALL_SECTOR_COUNT * SMALL_SECTOR_PAGES; // 128
constexpr int LARGE_SECTOR_PAGES = 16;

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
        SectorKeyConfig(bool keyACanRead= true, bool keyACanWrite= false, bool keyBCanRead= true, bool keyBCanWrite= true)
			: keyA_canRead(keyACanRead), keyA_canWrite(keyACanWrite), keyB_canRead(keyBCanRead), keyB_canWrite(keyBCanWrite) {}
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
        ki.kt = kt;
        keys[kt] = ki;
    }

	// memory-only config of sector permissions(computer); used by card logic to determine which key to use for read/write operations
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

    void ensureAuth(BYTE block, KeyType keyType, BYTE keySlot) {
        int sector = blockToSector(block);

        if (authSector == sector &&
            authType == keyType &&
            authSlot == keySlot)
            return;

        // gerçek auth çağrısı
        // reader.auth(block, keyType, keySlot);
        reader.setKeyType(keyType);
        reader.setKeyNumber(keySlot);
		reader.performAuth(block); // ensureAuth çağrıldığında auth istenmiş olur, bu çağrı da gerekli APDU'yu gönderir

        authSector = sector;
        authType = keyType;
        authSlot = keySlot;
    }

    // Ensure authorized for sector using chosen (kt,slot)
    // This function is exception-safe: it only updates authCache if loadKey and auth succeed.
    void ensureAuthorized(int sector, KeyType kt, BYTE keySlot) {
        // if already cached and matches kt+slot -> done
        auto cit = authCache.find(sector);
        if (cit != authCache.end()) {
            if (cit->second.kt == kt && cit->second.slot == keySlot) return;
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
        // reader.auth(authBlock, kt, ki.slot);
        ensureAuth(authBlock, kt, ki.slot);
        
        // if we reach here, auth succeeded: update cache
        AuthState st; st.kt = kt; st.slot = ki.slot;
        authCache[sector] = st;
    }

    std::vector<BYTE> read(BYTE block, KeyType keyType, BYTE keySlot) {
        // ensureAuth(block, keyType, keySlot);
        try{
            return reader.readPage(block);
		} catch (const pcsc::AuthFailedError&) {
            std::cerr << "Auth failed on read, attempting to load key and retry...\n";
            // İkinci deneme
            ensureAuth(block, keyType, keySlot);
            return reader.readPage(block);
        }
    }

    /*std::string read(BYTE block,
        KeyType keyType,
        BYTE keySlot)
    {
		auto result = read(block, keyType, keySlot);
        return std::string(result.begin(), result.end());
    }*/

    /*std::vector<BYTE> read(
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
    }*/

    void write(BYTE block, const std::vector<BYTE>& data, KeyType keyType, BYTE keySlot) {
        // ensureAuth(block, keyType, keySlot);
        try {
            reader.writePage(block, data.data());
        } catch(const pcsc::AuthFailedError&) {
			std::cerr << "Auth failed on write, attempting to load key and retry...\n";
            // İkinci deneme
            ensureAuth(block, keyType, keySlot);
            reader.writePage(block, data.data());
		}
    }

    void write(BYTE block, const std::string& text, KeyType keyType, BYTE keySlot) {
        std::vector<BYTE> data(text.begin(), text.end());
        write(block, data, keyType, keySlot);
    }

    /*void write(
        BYTE block,
        BYTE blockSize,
        const BYTE* data,
        KeyType keyType,
        BYTE keySlot)
    {
        ensureAuth(block, keyType, keySlot);

        std::vector<BYTE> buffer(data, data + blockSize);

        reader.writeData(block, buffer);
    }*/

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

        BYTEV page = reader.readPage(trailer);
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

    // try to authenticate to sector using any loaded key in 'keys' map.
    // returns the KeyType that succeeded and sets outSlot to the slot used.
    // throws if no key can auth.
    KeyType tryAnyAuthForSector(int sector, BYTE& outSlot) {
        for (auto& p : keys) {
            KeyType cand = p.first;
            const KeyInfo& ki = p.second;
            try {
                // ensureAuthorized will load key and perform auth (and update authCache on success)
                ensureAuthorized(sector, cand, ki.slot);
                outSlot = ki.slot;
                return cand;
            } catch (...) {
                // try next key
            }
        }
        throw std::runtime_error("No loaded key could authenticate for this sector");
    }

    // Apply the in-memory SectorKeyConfig to the physical card by rewriting the trailer.
    // - builds access bits from sectorConfigs[sector]
    // - requires that keys[KeyType::A] and keys[KeyType::B] exist (used as new KeyA/KeyB bytes)
    // - attempts to auth with any loaded key (so it works even if card currently uses A or B)
    // - validates access bits pattern before writing
    void applySectorConfigToCard(int sector) {
        validateSectorNumber(sector);

        // make sure we have a config in memory
        auto itcfg = sectorConfigs.find(sector);
        if (itcfg == sectorConfigs.end()) throw std::runtime_error("sector config not set");

        // build access bytes from config
        std::array<BYTE, 4> access = makeSectorAccessBits(itcfg->second);

        // validate generated access bits (just to be safe)
        if (!validateAccessBits(access.data())) throw std::runtime_error("Generated access bits are invalid");

        // ensure we have key material to write into the trailer (KeyA and KeyB)
        if (keys.find(KeyType::A) == keys.end() || keys.find(KeyType::B) == keys.end())
            throw std::runtime_error("Both KeyType::A and KeyType::B must be set before changing trailer");

        const KeyInfo& ka = findKeyOrThrow(KeyType::A), kb = findKeyOrThrow(KeyType::B);

        // Build 16-byte trailer: [keyA(6) | access(4) | keyB(6)]
        auto trailer = buildTrailer(ka.key.data(), access.data(), kb.key.data());

        // Determine trailer block absolute index
        int first = firstBlockOfSector(sector);
        BYTE trailerBlock = static_cast<BYTE>(first + blocksPerSector(sector) - 1);

        // Authenticate with any loaded key that works for this sector (reader.loadKey + auth)
        // This ensures we can write the trailer regardless whether current auth uses A or B.
        BYTE usedSlot = 0xFF;
        KeyType authedKey = tryAnyAuthForSector(sector, usedSlot);

        // Now perform the write. If writePage throws, nothing in authCache is modified here.
        try {
            reader.writePage(trailerBlock, trailer.data()); // may throw
        } catch (...) {
            // don't modify auth cache on failure; rethrow
            throw;
        }

        // After successful trailer change, invalidate auth cache for this sector
        invalidateAuthForSector(sector);
    }

	// Trailer can only be changed by authenticating with a key that has write permission on trailers (usually KeyB).
    void applySectorConfigStrict(int sector, KeyType authKeyType= KeyType::B, bool enableRollback = true) {
        validateSectorNumber(sector);

        // 1️⃣ Config var mı?
        auto itcfg = sectorConfigs.find(sector);
        if (itcfg == sectorConfigs.end()) throw std::runtime_error("Sector config not set");

        // 2️⃣ Key materyali var mı?
        if (keys.find(KeyType::A) == keys.end() ||
            keys.find(KeyType::B) == keys.end())
            throw std::runtime_error("KeyA and KeyB must be defined before trailer update");

        if (keys.find(authKeyType) == keys.end()) throw std::runtime_error("Auth key not loaded");

        const KeyInfo& keyAInfo = findKeyOrThrow(KeyType::A);
        const KeyInfo& keyBInfo = findKeyOrThrow(KeyType::B);
        const KeyInfo& authKey = findKeyOrThrow(authKeyType);

        // 3️⃣ Access bits üret
        std::array<BYTE, 4> access = makeSectorAccessBits(itcfg->second);

        if (!validateAccessBits(access.data()))
            throw std::runtime_error("Generated access bits invalid");

        // 4️⃣ Trailer block hesapla
        int first = firstBlockOfSector(sector);
        BYTE trailerBlock = static_cast<BYTE>(first + blocksPerSector(sector) - 1);

        // 5️⃣ Mevcut trailer yedeği al
        std::array<BYTE, 16> oldTrailer;
        if (enableRollback) oldTrailer = readTrailer(sector);

        // 6️⃣ Yeni trailer oluştur
        auto newTrailer = buildTrailer(keyAInfo.key.data(), access.data(), keyBInfo.key.data());

        // 7️⃣ Sadece belirtilen key ile auth
        ensureAuthorized(sector, authKeyType, authKey.slot);

        try {
            reader.writePage(trailerBlock, newTrailer.data());
        } catch (...) {
            if (enableRollback && !oldTrailer.empty()) {
                try {
                    // tekrar auth gerekir
                    ensureAuthorized(sector, authKeyType, authKey.slot);
                    reader.writePage(trailerBlock, oldTrailer.data());
                } catch (...) {
                    std::cerr << "Rollback failed. trailerBlock: " << static_cast<int>(trailerBlock) << std::endl;
                }
            }
            throw;
        }

        // 8️⃣ Auth cache temizle
        invalidateAuthForSector(sector);
    }

private:
    ReaderT& reader;
    bool is4KCard;

    // --- AUTH CACHE MEMBERS (ekle bunları!) ---
    int authSector = -1;       // currently authenticated sector, -1 == none
    int authSectorCurrently = -1; // not strictly necessary (kept for clarity)
    BYTE authSlot = 0xFF;    // slot used for last auth
    KeyType authType = KeyType::A;

    // --- auth cache map (per-sector) --- (opsiyonel; senin kodun map kullandıysa onu da koru)
    std::map<int, KeyType> authorizedSectors;

    std::map<KeyType, KeyInfo> keys;
    std::map<int, SectorKeyConfig> sectorConfigs;

    // cached auth state for sector -> (KeyType, slot)
    struct AuthState { KeyType kt; BYTE slot; };
    std::map<int, AuthState> authCache;

    // ===== helpers: layout (1K/4K aware) =====
    int sectorFromBlock(BYTE block) const {
        int b = static_cast<int>(block);
		return !is4KCard || b < 128 ? b / 4 : 32 + (b - 128) / 16;
    }
    int blocksPerSector(int sector) const {
		return !is4KCard || sector < 32 ? 4 : 16;
    }
    int firstBlockOfSector(int sector) const {
		return !is4KCard || sector < 32 ? sector * 4 : 128 + (sector - 32) * 16;
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
	// method is not change the sector trailer. It only determines which key to use for auth based on configured permissions on computer runtime.
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
        if (it == keys.end()) throw std::runtime_error("key not set for requested KeyType");
        return it->second;
    }

    void invalidateAuthForSector(int sector) {
        authCache.erase(sector);
    }

    // Access bits inverted pattern validation (same logic as before)
    static bool validateAccessBits(const BYTE a[4]) {
        BYTE b6 = a[0], b7 = a[1], b8 = a[2];
        BYTE c1[4], c2[4], c3[4];
        // Extracting Bits
        c1[0] = (b6 >> 4) & 1; c1[1] = (b6 >> 5) & 1; c1[2] = (b6 >> 6) & 1; c1[3] = (b6 >> 7) & 1;
        c2[0] = (b6 >> 0) & 1; c2[1] = (b6 >> 1) & 1; c2[2] = (b6 >> 2) & 1; c2[3] = (b6 >> 3) & 1;
        c3[0] = (b7 >> 4) & 1; c3[1] = (b7 >> 5) & 1; c3[2] = (b7 >> 6) & 1; c3[3] = (b7 >> 7) & 1;

        // Inverted bit check
        if ((b7 & 0x0F) != (((~c1[0] & 1) << 0) | ((~c1[1] & 1) << 1) | ((~c1[2] & 1) << 2) | ((~c1[3] & 1) << 3))) return false;
        if (((b8 >> 4) & 0x0F) != (((~c2[0] & 1) << 0) | ((~c2[1] & 1) << 1) | ((~c2[2] & 1) << 2) | ((~c2[3] & 1) << 3))) return false;
        if ((b8 & 0x0F) != (((~c3[0] & 1) << 0) | ((~c3[1] & 1) << 1) | ((~c3[2] & 1) << 2) | ((~c3[3] & 1) << 3))) return false;
        return true;
    }

    // mapDataBlock: daha az branch, lookup table ile
    static void mapDataBlock(const SectorKeyConfig& cfg, BYTE& c1, BYTE& c2, BYTE& c3) { 
        // KeyA: R, KeyB: R/W 
        if (cfg.keyA_canRead && !cfg.keyA_canWrite && cfg.keyB_canRead && cfg.keyB_canWrite) { c1 = 0; c2 = 1; c3 = 0; return; } 
        // KeyA: R/W, KeyB: R/W 
        if (cfg.keyA_canRead && cfg.keyA_canWrite && cfg.keyB_canRead && cfg.keyB_canWrite) { c1 = 0; c2 = 0; c3 = 0; return; } 
        // KeyA: R/W, KeyB: R 
        if (cfg.keyA_canRead && cfg.keyA_canWrite && cfg.keyB_canRead && !cfg.keyB_canWrite) { c1 = 1; c2 = 0; c3 = 0; return; } 
        // KeyA: R, KeyB: R 
        if (cfg.keyA_canRead && !cfg.keyA_canWrite && cfg.keyB_canRead && !cfg.keyB_canWrite) { c1 = 1; c2 = 1; c3 = 0; return; } 
        throw std::runtime_error("Unsupported access combination"); 
    }

    // makeSectorAccessBits: nibble-level hesaplama, minimal ops
    static inline std::array<BYTE, 4> makeSectorAccessBits(const SectorKeyConfig& dataCfg)
    {
        // c?_i: c1,c2,c3 for blocks 0..3
        BYTE c1[SMALL_SECTOR_PAGES], c2[SMALL_SECTOR_PAGES], c3[SMALL_SECTOR_PAGES];

        // data blocks (0..2)
        for (int i = 0; i < SMALL_SECTOR_PAGES-1; ++i) mapDataBlock(dataCfg, c1[i], c2[i], c3[i]);

        // trailer safe default: KeyA read-only, KeyB write (senin önceki tercihin)
        c1[3] = 1; c2[3] = 1; c3[3] = 0;

        // Build 4-bit nibbles where bit3 = index 3, bit2 = index 2, bit1 = index 1, bit0 = index 0
        const BYTE c1_nibble = static_cast<BYTE>((c1[3] << 3) | (c1[2] << 2) | (c1[1] << 1) | (c1[0] << 0));
        const BYTE c2_nibble = static_cast<BYTE>((c2[3] << 3) | (c2[2] << 2) | (c2[1] << 1) | (c2[0] << 0));
        const BYTE c3_nibble = static_cast<BYTE>((c3[3] << 3) | (c3[2] << 2) | (c3[1] << 1) | (c3[0] << 0));

        // Compose bytes:
        // b6 = (c1[3]..c1[0]) << 4  | (c2[3]..c2[0])
        const BYTE b6 = static_cast<BYTE>((c1_nibble << 4) | (c2_nibble & 0x0F));
        // b7 = (c3_nibble << 4) | (~c1_nibble & 0x0F)
        const BYTE b7 = static_cast<BYTE>((c3_nibble << 4) | (static_cast<BYTE>(~c1_nibble) & 0x0F));
        // b8 = ((~c2_nibble & 0x0F) << 4) | (~c3_nibble & 0x0F)
        const BYTE b8 = static_cast<BYTE>(((static_cast<BYTE>(~c2_nibble) & 0x0F) << 4) | (static_cast<BYTE>(~c3_nibble) & 0x0F));

        return { b6, b7, b8, 0x00 };
    }

    // buildTrailer: heap-free, fast copy
    static inline std::array<BYTE, 16> buildTrailer(const BYTE keyA[6],
                                                    const BYTE access[4],
                                                    const BYTE keyB[6]) noexcept {
        std::array<BYTE, 16> trailer;
        // small copies: memcpy optimizes well
        std::memcpy(trailer.data(), keyA, 6);
        std::memcpy(trailer.data() + 6, access, 4);
        std::memcpy(trailer.data() + 10, keyB, 6);
        return trailer;
    }
};

#endif // MIFARECLASSIC_H