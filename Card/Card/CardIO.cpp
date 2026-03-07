#include "CardIO.h"
#include "CardModel/CardMemoryLayout.h"
#include "CardModel/DesfireMemoryLayout.h"
#include "CardModel/TrailerConfig.h"
#include "CardProtocol/DesfireCommands.h"
#include "CardProtocol/DesfireAuth.h"
#include "CardProtocol/DesfireSession.h"
#include <cstring>
#include <stdexcept>

// ════════════════════════════════════════════════════════════════════════════════
// Construction
// ════════════════════════════════════════════════════════════════════════════════

CardIO::CardIO(Reader& reader, CardType ct)
    : reader_(reader), card_(ct)
{
    keys_.push_back(KeyInfo{});
    if (card_.isDesfire())
        desfireSession_ = std::make_unique<DesfireSession>();
}

CardIO::CardIO(Reader& reader, bool is4K)
    : CardIO(reader, is4K ? CardType::MifareClassic4K
                          : CardType::MifareClassic1K)
{
}

CardIO::~CardIO() = default;

// ════════════════════════════════════════════════════════════════════════════════
// Key Ayarları
// ════════════════════════════════════════════════════════════════════════════════

void CardIO::setDefaultKey(const KEYBYTES& key, KeyStructure ks,
                           BYTE slot, KeyType kt)
{
    keys_.clear();
    keys_.push_back({ key, kt, ks, slot, "DefaultKey" });
    slotContents_.clear();
}

void CardIO::setKeys(const KEYBYTES& keyA, BYTE slotA,
                     const KEYBYTES& keyB, BYTE slotB,
                     KeyStructure ks)
{
    keys_.clear();
    keys_.push_back({ keyA, KeyType::A, ks, slotA, "KeyA" });
    keys_.push_back({ keyB, KeyType::B, ks, slotB, "KeyB" });
    slotContents_.clear();

    card_.registerKey(KeyType::A, keyA, ks, slotA, "KeyA");
    card_.registerKey(KeyType::B, keyB, ks, slotB, "KeyB");
}

void CardIO::addKey(const KeyInfo& ki)
{
    for (auto& existing : keys_) {
        if (existing.kt == ki.kt && existing.slot == ki.slot) {
            existing = ki;
            slotContents_.erase(ki.slot);
            return;
        }
    }
    keys_.push_back(ki);
}

void CardIO::clearKeys()
{
    keys_.clear();
    keys_.push_back(KeyInfo{});
    invalidateAuth();
}

// ════════════════════════════════════════════════════════════════════════════════
// Auth
// ════════════════════════════════════════════════════════════════════════════════

void CardIO::ensureAuth(int sector, AuthPurpose purpose)
{
    tryEnsureAuth(sector, purpose).unwrap();
}

Result<void> CardIO::tryEnsureAuth(int sector, AuthPurpose purpose)
{
    if (card_.isUltralight()) return {PcscError{}};

    if (sector == lastAuthSector_) {
        if (!isMultiKey()) return {PcscError{}};
        const KeyInfo& lastKey = findKey(lastAuthKT_);
        if (canKeyPerform(lastKey, sector, purpose)) return {PcscError{}};
    }

    const KeyInfo& chosen = chooseKey(sector, purpose);
    auto result = tryDoAuth(sector, chosen);
    if (result.ok()) return result;

    if (isMultiKey()) {
        for (const auto& ki : keys_) {
            if (&ki == &chosen) continue;
            result = tryDoAuth(sector, ki);
            if (result.ok()) return result;
        }
    }

    invalidateAuth();
    return result;
}

void CardIO::doAuth(int sector, const KeyInfo& ki)
{
    tryDoAuth(sector, ki).unwrap();
}

Result<void> CardIO::tryDoAuth(int sector, const KeyInfo& ki)
{
    int trailer = card_.getTrailerBlockOfSector(sector);

    auto it = slotContents_.find(ki.slot);
    if (it == slotContents_.end() || it->second != ki.key) {
        auto lr = reader_.tryLoadKey(ki.key.data(), ki.ks, ki.slot);
        if (!lr) return lr;
        slotContents_[ki.slot] = ki.key;
    }

    auto ar = reader_.tryAuth(static_cast<BYTE>(trailer), ki.kt, ki.slot);
    if (!ar) return ar;

    lastAuthSector_ = sector;
    lastAuthKT_     = ki.kt;
    return {PcscError{}};
}

void CardIO::ensureKeyLoaded(const KeyInfo& ki)
{
    auto it = slotContents_.find(ki.slot);
    if (it != slotContents_.end() && it->second == ki.key) return;

    reader_.loadKey(ki.key.data(), ki.ks, ki.slot);
    slotContents_[ki.slot] = ki.key;
}

void CardIO::invalidateAuth()
{
    lastAuthSector_ = -1;
    lastAuthKT_     = KeyType::A;
    slotContents_.clear();
}

const KeyInfo& CardIO::chooseKey(int sector, AuthPurpose purpose) const
{
    if (keys_.size() == 1) return keys_[0];

    const KeyInfo* best = nullptr;
    int bestScore = 0;

    for (const auto& ki : keys_) {
        bool canR = canKeyPerform(ki, sector, AuthPurpose::Read);
        bool canW = canKeyPerform(ki, sector, AuthPurpose::Write);

        bool usable = (purpose == AuthPurpose::Read) ? canR : canW;
        if (!usable) continue;

        // Least privilege: sadece gereken yetkiye sahip key tercih edilir.
        //   score 2 → sadece gereken yetki (read-only veya write-only)
        //   score 1 → read+write (gereğinden fazla yetki)
        int score = (canR && canW) ? 1 : 2;

        if (score > bestScore) {
            best = &ki;
            bestScore = score;
        }
    }

    return best ? *best : keys_.front();
}

const KeyInfo& CardIO::findKey(KeyType kt) const
{
    for (const auto& ki : keys_) {
        if (ki.kt == kt) return ki;
    }
    return keys_.front();
}

bool CardIO::isMultiKey() const
{
    return keys_.size() > 1;
}

bool CardIO::canKeyPerform(const KeyInfo& ki, int sector, AuthPurpose purpose) const
{
    // Mifare Classic: permission sektörün access bits'inden gelir
    if (card_.isClassic()) {
        return (purpose == AuthPurpose::Read)
            ? card_.canReadDataBlocks(sector, ki.kt)
            : card_.canWriteDataBlocks(sector, ki.kt);
    }

    // DESFire / diğer: permission key'in kendisinde tanımlı
    return (purpose == AuthPurpose::Read) ? ki.canRead() : ki.canWrite();
}

void CardIO::authenticate(int sector)
{
    tryAuthenticate(sector).unwrap();
}

Result<void> CardIO::tryAuthenticate(int sector)
{
    invalidateAuth();
    return tryEnsureAuth(sector);
}

void CardIO::authenticate(int sector, KeyType kt, BYTE slot)
{
    const KeyInfo& ki = findKey(kt);
    int trailer = card_.getTrailerBlockOfSector(sector);
    reader_.loadKey(ki.key.data(), ki.ks, slot);
    reader_.auth(static_cast<BYTE>(trailer), kt, slot);
    lastAuthSector_ = sector;
    lastAuthKT_     = kt;
    slotContents_.clear();
}

// ════════════════════════════════════════════════════════════════════════════════
// Kart Okuma
// ════════════════════════════════════════════════════════════════════════════════

int CardIO::readCard()
{
    return tryReadCard().unwrap();
}

Result<int> CardIO::tryReadCard()
{
    if (card_.isDesfire()) return {0, PcscError{}};

    int totalSectors = card_.getTotalSectors();
    int okCount = 0;
    size_t memSize = card_.getTotalMemory();
    BYTEV rawBuf(memSize, 0);

    for (int s = 0; s < totalSectors; ++s) {
        auto authResult = tryEnsureAuth(s);
        if (!authResult) { invalidateAuth(); continue; }

        int first = card_.getFirstBlockOfSector(s);
        int last  = card_.getLastBlockOfSector(s);

        for (int b = first; b <= last; ++b) {
            auto rr = reader_.tryReadPage(static_cast<BYTE>(b));
            if (rr && rr.value.size() >= 16) {
                std::memcpy(&rawBuf[b * 16], rr.value.data(), 16);
                ++okCount;
            }
        }
    }

    card_.loadMemory(rawBuf.data(), memSize);
    return {okCount, PcscError{}};
}

bool CardIO::readSector(int sector)
{
    auto r = tryReadSector(sector);
    return r.ok() && r.value;
}

Result<bool> CardIO::tryReadSector(int sector)
{
    if (card_.isDesfire()) return {false, PcscError{}};

    auto authResult = tryEnsureAuth(sector);
    if (!authResult) { invalidateAuth(); return {false, authResult.error}; }

    int first = card_.getFirstBlockOfSector(sector);
    int last  = card_.getLastBlockOfSector(sector);
    bool allOk = true;

    CardMemoryLayout& mem = card_.getMemoryMutable();
    BYTE* raw = mem.getRawMemory();

    for (int b = first; b <= last; ++b) {
        auto rr = reader_.tryReadPage(static_cast<BYTE>(b));
        if (rr && rr.value.size() >= 16)
            std::memcpy(raw + b * 16, rr.value.data(), 16);
        else
            allOk = false;
    }
    return {allOk, PcscError{}};
}

BYTEV CardIO::readBlock(int block)
{
    return tryReadBlock(block).unwrap();
}

Result<BYTEV> CardIO::tryReadBlock(int block)
{
    int sector = card_.getSectorForBlock(block);
    auto authResult = tryEnsureAuth(sector);
    if (!authResult) return {BYTEV{}, authResult.error};

    auto rr = reader_.tryReadPage(static_cast<BYTE>(block));
    if (!rr) return rr;

    if (rr.value.size() >= 16) {
        CardMemoryLayout& mem = card_.getMemoryMutable();
        std::memcpy(mem.getRawMemory() + block * 16, rr.value.data(), 16);
    }
    return rr;
}

// ════════════════════════════════════════════════════════════════════════════════
// Kart Yazma
// ════════════════════════════════════════════════════════════════════════════════

void CardIO::writeBlock(int block, const BYTE data[16])
{
    tryWriteBlock(block, data).unwrap();
}

Result<void> CardIO::tryWriteBlock(int block, const BYTE data[16])
{
    if (card_.isManufacturerBlock(block))
        return {{ErrorCode::ManufacturerBlock}};
    if (card_.isTrailerBlock(block))
        return {{ErrorCode::TrailerBlock}};

    int sector = card_.getSectorForBlock(block);
    auto authResult = tryEnsureAuth(sector, AuthPurpose::Write);
    if (!authResult) return authResult;

    if (card_.isUltralight()) {
        int basePage = block * 4;
        for (int p = 0; p < 4; ++p) {
            auto wr = reader_.tryWritePage(static_cast<BYTE>(basePage + p), data + p * 4);
            if (!wr) return wr;
        }
    } else {
        auto wr = reader_.tryWritePage(static_cast<BYTE>(block), data);
        if (!wr) return wr;
    }

    CardMemoryLayout& mem = card_.getMemoryMutable();
    std::memcpy(mem.getRawMemory() + block * 16, data, 16);
    return {PcscError{}};
}

void CardIO::writeBlock(int block, const BYTEV& data)
{
    if (data.size() < 16)
        throw std::invalid_argument("Data must be at least 16 bytes");
    writeBlock(block, data.data());
}

// ════════════════════════════════════════════════════════════════════════════════
// Trailer Okuma / Yazma
// ════════════════════════════════════════════════════════════════════════════════

TrailerConfig CardIO::readTrailer(int sector)
{
    return tryReadTrailer(sector).unwrap();
}

Result<TrailerConfig> CardIO::tryReadTrailer(int sector)
{
    if (card_.isDesfire())
        return {TrailerConfig{}, {ErrorCode::NotDesfire}};

    int trailerBlock = card_.getTrailerBlockOfSector(sector);
    auto authResult = tryEnsureAuth(sector);
    if (!authResult) return {TrailerConfig{}, authResult.error};

    auto rr = reader_.tryReadPage(static_cast<BYTE>(trailerBlock));
    if (!rr) return {TrailerConfig{}, rr.error};
    if (rr.value.size() < 16) return {TrailerConfig{}, {ErrorCode::ReadFailed}};

    CardMemoryLayout& mem = card_.getMemoryMutable();
    std::memcpy(mem.getRawMemory() + trailerBlock * 16, rr.value.data(), 16);

    MifareBlock blk;
    std::memcpy(blk.raw, rr.value.data(), 16);
    return {TrailerConfig::fromBlock(blk), PcscError{}};
}

void CardIO::writeTrailer(int sector, const TrailerConfig& config)
{
    tryWriteTrailer(sector, config).unwrap();
}

Result<void> CardIO::tryWriteTrailer(int sector, const TrailerConfig& config)
{
    if (card_.isDesfire())
        return {{ErrorCode::NotDesfire}};
    if (!config.isValid())
        return {{ErrorCode::InvalidData}};

    int trailerBlock = card_.getTrailerBlockOfSector(sector);
    auto authResult = tryEnsureAuth(sector, AuthPurpose::Write);
    if (!authResult) return authResult;

    MifareBlock blk = config.toBlock();
    auto wr = reader_.tryWritePage(static_cast<BYTE>(trailerBlock), blk.raw);
    if (!wr) return wr;

    CardMemoryLayout& mem = card_.getMemoryMutable();
    std::memcpy(mem.getRawMemory() + trailerBlock * 16, blk.raw, 16);
    return {PcscError{}};
}

// ════════════════════════════════════════════════════════════════════════════════
// Access Bits Konfigürasyonu
// ════════════════════════════════════════════════════════════════════════════════

SectorAccessConfig CardIO::getAccessConfig(int sector) const
{
    if (card_.isDesfire())
        throw std::logic_error("DESFire has no access bits");

    const MifareBlock& trailer = card_.getBlock(
        card_.getTrailerBlockOfSector(sector));
    ACCESSBYTES ab;
    std::memcpy(ab.data(), trailer.trailer.accessBits, 4);
    return AccessBitsCodec::decode(ab);
}

void CardIO::setAccessConfig(int sector, const SectorAccessConfig& config)
{
    if (card_.isDesfire())
        throw std::logic_error("DESFire has no access bits");

    // Mevcut trailer'ı oku
    TrailerConfig tc = readTrailer(sector);
    tc.access = config;
    writeTrailer(sector, tc);
}

void CardIO::setSectorMode(int sector, SectorMode mode)
{
    if (card_.isDesfire())
        throw std::logic_error("DESFire has no sector modes");
    setAccessConfig(sector, sectorModeToConfig(mode));
}

DataBlockPermission CardIO::getDataPermission(int sector, int blockIndex) const
{
    if (card_.isDesfire())
        throw std::logic_error("DESFire has no data block permissions");
    SectorAccessConfig cfg = getAccessConfig(sector);
    return cfg.dataPermission(blockIndex);
}

TrailerPermission CardIO::getTrailerPermission(int sector) const
{
    if (card_.isDesfire())
        throw std::logic_error("DESFire has no trailer permissions");
    SectorAccessConfig cfg = getAccessConfig(sector);
    return cfg.trailerPermission();
}

// ════════════════════════════════════════════════════════════════════════════════
// Bulk Trailer İşlemleri
// ════════════════════════════════════════════════════════════════════════════════

std::vector<TrailerConfig> CardIO::saveAllTrailers()
{
    if (card_.isDesfire()) return {};

    int totalSectors = card_.getTotalSectors();
    std::vector<TrailerConfig> configs;
    configs.reserve(totalSectors);

    for (int s = 0; s < totalSectors; ++s) {
        auto r = tryReadTrailer(s);
        configs.push_back(r.ok() ? r.value : TrailerConfig::factoryDefault());
    }
    return configs;
}

void CardIO::restoreAllTrailers(const std::vector<TrailerConfig>& configs)
{
    if (card_.isDesfire()) return;

    int totalSectors = card_.getTotalSectors();
    int count = static_cast<int>(configs.size());
    if (count > totalSectors) count = totalSectors;

    for (int s = 0; s < count; ++s) {
        tryWriteTrailer(s, configs[s]); // best-effort
    }
}

// ════════════════════════════════════════════════════════════════════════════════
// Erişim
// ════════════════════════════════════════════════════════════════════════════════

CardInterface&       CardIO::card()       { return card_; }
const CardInterface& CardIO::card() const { return card_; }
Reader&              CardIO::reader()     { return reader_; }

// ════════════════════════════════════════════════════════════════════════════════
// DESFire — Internal Helpers
// ════════════════════════════════════════════════════════════════════════════════

BYTEV CardIO::desfireTransmit(const BYTEV& apdu) {
    return reader_.transmit(apdu).raw();
}

Result<BYTEV> CardIO::tryDesfireTransmit(const BYTEV& apdu) {
    auto result = reader_.tryTransmit(apdu);
    if (!result) return {BYTEV{}, result.error};
    return {result.value.raw(), PcscError{}};
}

Result<void> CardIO::desfireExec(const BYTEV& apdu) {
    auto tx = tryDesfireTransmit(apdu);
    if (!tx) return {tx.error};
    return {DesfireCommands::evaluateResponse(tx.value)};
}

Result<BYTEV> CardIO::desfireQuery(const BYTEV& apdu) {
    auto tx = [this](const BYTEV& a) -> Result<BYTEV> { return tryDesfireTransmit(a); };
    return DesfireCommands::tryTransceive(tx, apdu);
}

// ════════════════════════════════════════════════════════════════════════════════
// DESFire — Public API
// ════════════════════════════════════════════════════════════════════════════════

DesfireVersionInfo CardIO::discoverCard() { return tryDiscoverCard().unwrap(); }

Result<DesfireVersionInfo> CardIO::tryDiscoverCard() {
    if (!card_.isDesfire()) return {DesfireVersionInfo{}, {ErrorCode::NotDesfire}};
    auto tx = [this](const BYTEV& a) -> Result<BYTEV> { return tryDesfireTransmit(a); };
    auto r = DesfireCommands::tryParseGetVersion(tx);
    if (!r) return r;
    card_.getDesfireMemoryMutable().initFromVersion(r.value);
    return r;
}

void CardIO::selectApplication(const DesfireAID& aid) { trySelectApplication(aid).unwrap(); }

Result<void> CardIO::trySelectApplication(const DesfireAID& aid) {
    if (!card_.isDesfire()) return {{ErrorCode::NotDesfire}};
    auto r = desfireExec(DesfireCommands::selectApplication(aid));
    if (!r) return r;
    if (desfireSession_) desfireSession_->resetKeepApp();
    if (desfireSession_) desfireSession_->currentAID = aid;
    card_.getDesfireMemoryMutable().currentAID = aid;
    return r;
}

void CardIO::authenticateDesfire(const BYTEV& key, BYTE keyNo, DesfireKeyType keyType) {
    tryAuthenticateDesfire(key, keyNo, keyType).unwrap();
}

Result<void> CardIO::tryAuthenticateDesfire(const BYTEV& key, BYTE keyNo, DesfireKeyType keyType) {
    if (!card_.isDesfire()) return {{ErrorCode::NotDesfire}};
    if (!desfireSession_)
        desfireSession_ = std::make_unique<DesfireSession>();
    DesfireAuth auth;
    auto tx = [this](const BYTEV& a) -> Result<BYTEV> { return tryDesfireTransmit(a); };
    return auth.tryAuthenticate(*desfireSession_, key, keyNo, keyType, tx);
}

std::vector<DesfireAID> CardIO::getApplicationIDs() { return tryGetApplicationIDs().unwrap(); }

Result<std::vector<DesfireAID>> CardIO::tryGetApplicationIDs() {
    if (!card_.isDesfire()) return {std::vector<DesfireAID>{}, {ErrorCode::NotDesfire}};
    auto r = desfireQuery(DesfireCommands::getApplicationIDs());
    if (!r) return {std::vector<DesfireAID>{}, r.error};
    return {DesfireCommands::parseApplicationIDs(r.value), PcscError{}};
}

std::vector<BYTE> CardIO::getFileIDs() { return tryGetFileIDs().unwrap(); }

Result<std::vector<BYTE>> CardIO::tryGetFileIDs() {
    if (!card_.isDesfire()) return {std::vector<BYTE>{}, {ErrorCode::NotDesfire}};
    auto r = desfireQuery(DesfireCommands::getFileIDs());
    if (!r) return {std::vector<BYTE>{}, r.error};
    return {DesfireCommands::parseFileIDs(r.value), PcscError{}};
}

DesfireFileSettings CardIO::getFileSettings(BYTE fileNo) { return tryGetFileSettings(fileNo).unwrap(); }

Result<DesfireFileSettings> CardIO::tryGetFileSettings(BYTE fileNo) {
    if (!card_.isDesfire()) return {DesfireFileSettings{}, {ErrorCode::NotDesfire}};
    auto r = desfireQuery(DesfireCommands::getFileSettings(fileNo));
    if (!r) return {DesfireFileSettings{}, r.error};
    return {DesfireCommands::parseFileSettings(r.value), PcscError{}};
}

BYTEV CardIO::readFileData(BYTE fileNo, uint32_t offset, uint32_t length) {
    return tryReadFileData(fileNo, offset, length).unwrap();
}

Result<BYTEV> CardIO::tryReadFileData(BYTE fileNo, uint32_t offset, uint32_t length) {
    if (!card_.isDesfire()) return {BYTEV{}, {ErrorCode::NotDesfire}};
    return desfireQuery(DesfireCommands::readData(fileNo, offset, length));
}

void CardIO::writeFileData(BYTE fileNo, uint32_t offset, const BYTEV& data) {
    tryWriteFileData(fileNo, offset, data).unwrap();
}

Result<void> CardIO::tryWriteFileData(BYTE fileNo, uint32_t offset, const BYTEV& data) {
    if (!card_.isDesfire()) return {{ErrorCode::NotDesfire}};
    return desfireExec(DesfireCommands::writeData(fileNo, offset, data));
}

BYTEV CardIO::readRecords(BYTE fileNo, uint32_t offset, uint32_t count) {
    return tryReadRecords(fileNo, offset, count).unwrap();
}

Result<BYTEV> CardIO::tryReadRecords(BYTE fileNo, uint32_t offset, uint32_t count) {
    if (!card_.isDesfire()) return {BYTEV{}, {ErrorCode::NotDesfire}};
    return desfireQuery(DesfireCommands::readRecords(fileNo, offset, count));
}

void CardIO::appendRecord(BYTE fileNo, const BYTEV& recordData) {
    tryAppendRecord(fileNo, recordData).unwrap();
}

Result<void> CardIO::tryAppendRecord(BYTE fileNo, const BYTEV& recordData) {
    if (!card_.isDesfire()) return {{ErrorCode::NotDesfire}};
    return desfireExec(DesfireCommands::appendRecord(fileNo, recordData));
}

size_t CardIO::getFreeMemory() { return tryGetFreeMemory().unwrap(); }

Result<size_t> CardIO::tryGetFreeMemory() {
    if (!card_.isDesfire()) return {0, {ErrorCode::NotDesfire}};
    auto r = desfireQuery(DesfireCommands::getFreeMemory());
    if (!r) return {0, r.error};
    return {DesfireCommands::parseFreeMemory(r.value), PcscError{}};
}

bool CardIO::isDesfireAuthenticated() const {
    return desfireSession_ && desfireSession_->isValid();
}

void CardIO::setDesfireSessionTimeout(uint32_t ms) {
    if (!desfireSession_)
        desfireSession_ = std::make_unique<DesfireSession>();
    desfireSession_->setTimeoutMs(ms);
}

// ════════════════════════════════════════════════════════════════════════════════
// DESFire — Management API (Faz 4)
// ════════════════════════════════════════════════════════════════════════════════

void CardIO::createApplication(const DesfireAID& aid, BYTE keySettings,
                                BYTE maxKeys, DesfireKeyType keyType) {
    tryCreateApplication(aid, keySettings, maxKeys, keyType).unwrap();
}
Result<void> CardIO::tryCreateApplication(const DesfireAID& aid, BYTE keySettings,
                                           BYTE maxKeys, DesfireKeyType keyType) {
    if (!card_.isDesfire()) return {{ErrorCode::NotDesfire}};
    return desfireExec(DesfireCommands::createApplication(aid, keySettings, maxKeys, keyType));
}

void CardIO::deleteApplication(const DesfireAID& aid) { tryDeleteApplication(aid).unwrap(); }
Result<void> CardIO::tryDeleteApplication(const DesfireAID& aid) {
    if (!card_.isDesfire()) return {{ErrorCode::NotDesfire}};
    return desfireExec(DesfireCommands::deleteApplication(aid));
}

void CardIO::createStdDataFile(BYTE fileNo, DesfireCommMode comm,
                                const DesfireAccessRights& access, uint32_t fileSize) {
    tryCreateStdDataFile(fileNo, comm, access, fileSize).unwrap();
}
Result<void> CardIO::tryCreateStdDataFile(BYTE fileNo, DesfireCommMode comm,
                                           const DesfireAccessRights& access, uint32_t fileSize) {
    if (!card_.isDesfire()) return {{ErrorCode::NotDesfire}};
    return desfireExec(DesfireCommands::createStdDataFile(fileNo, comm, access, fileSize));
}

void CardIO::createValueFile(BYTE fileNo, DesfireCommMode comm,
                              const DesfireAccessRights& access,
                              int32_t lower, int32_t upper, int32_t value,
                              bool limitedCredit) {
    tryCreateValueFile(fileNo, comm, access, lower, upper, value, limitedCredit).unwrap();
}
Result<void> CardIO::tryCreateValueFile(BYTE fileNo, DesfireCommMode comm,
                                         const DesfireAccessRights& access,
                                         int32_t lower, int32_t upper, int32_t value,
                                         bool limitedCredit) {
    if (!card_.isDesfire()) return {{ErrorCode::NotDesfire}};
    return desfireExec(DesfireCommands::createValueFile(fileNo, comm, access,
                                                         lower, upper, value, limitedCredit));
}

void CardIO::createLinearRecordFile(BYTE fileNo, DesfireCommMode comm,
                                     const DesfireAccessRights& access,
                                     uint32_t recordSize, uint32_t maxRecords) {
    tryCreateLinearRecordFile(fileNo, comm, access, recordSize, maxRecords).unwrap();
}
Result<void> CardIO::tryCreateLinearRecordFile(BYTE fileNo, DesfireCommMode comm,
                                                const DesfireAccessRights& access,
                                                uint32_t recordSize, uint32_t maxRecords) {
    if (!card_.isDesfire()) return {{ErrorCode::NotDesfire}};
    return desfireExec(DesfireCommands::createLinearRecordFile(fileNo, comm, access,
                                                                recordSize, maxRecords));
}

void CardIO::deleteFile(BYTE fileNo) { tryDeleteFile(fileNo).unwrap(); }
Result<void> CardIO::tryDeleteFile(BYTE fileNo) {
    if (!card_.isDesfire()) return {{ErrorCode::NotDesfire}};
    return desfireExec(DesfireCommands::deleteFile(fileNo));
}

void CardIO::creditValue(BYTE fileNo, int32_t value) { tryCreditValue(fileNo, value).unwrap(); }
Result<void> CardIO::tryCreditValue(BYTE fileNo, int32_t value) {
    if (!card_.isDesfire()) return {{ErrorCode::NotDesfire}};
    return desfireExec(DesfireCommands::credit(fileNo, value));
}

void CardIO::debitValue(BYTE fileNo, int32_t value) { tryDebitValue(fileNo, value).unwrap(); }
Result<void> CardIO::tryDebitValue(BYTE fileNo, int32_t value) {
    if (!card_.isDesfire()) return {{ErrorCode::NotDesfire}};
    return desfireExec(DesfireCommands::debit(fileNo, value));
}

void CardIO::commitTransaction() { tryCommitTransaction().unwrap(); }
Result<void> CardIO::tryCommitTransaction() {
    if (!card_.isDesfire()) return {{ErrorCode::NotDesfire}};
    return desfireExec(DesfireCommands::commitTransaction());
}

void CardIO::abortTransaction() { tryAbortTransaction().unwrap(); }
Result<void> CardIO::tryAbortTransaction() {
    if (!card_.isDesfire()) return {{ErrorCode::NotDesfire}};
    return desfireExec(DesfireCommands::abortTransaction());
}

BYTE CardIO::getKeyVersion(BYTE keyNo) { return tryGetKeyVersion(keyNo).unwrap(); }
Result<BYTE> CardIO::tryGetKeyVersion(BYTE keyNo) {
    if (!card_.isDesfire()) return {0, {ErrorCode::NotDesfire}};
    auto r = desfireQuery(DesfireCommands::getKeyVersion(keyNo));
    if (!r) return {0, r.error};
    return {r.value.empty() ? BYTE(0) : r.value[0], PcscError{}};
}

void CardIO::formatPICC() { tryFormatPICC().unwrap(); }
Result<void> CardIO::tryFormatPICC() {
    if (!card_.isDesfire()) return {{ErrorCode::NotDesfire}};
    return desfireExec(DesfireCommands::formatPICC());
}
