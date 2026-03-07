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
    reader_.setAuthRequested(false);
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
    if (card_.isUltralight()) return;

    if (sector == lastAuthSector_) {
        if (!isMultiKey()) return;
        const KeyInfo& lastKey = findKey(lastAuthKT_);
        if (canKeyPerform(lastKey, sector, purpose)) return;
    }

    const KeyInfo& chosen = chooseKey(sector, purpose);

    try {
        doAuth(sector, chosen);
        return;
    }
    catch (...) {
        if (isMultiKey()) {
            for (const auto& ki : keys_) {
                if (&ki == &chosen) continue;
                try {
                    doAuth(sector, ki);
                    return;
                }
                catch (...) { continue; }
            }
        }
        invalidateAuth();
        throw;
    }
}

void CardIO::doAuth(int sector, const KeyInfo& ki)
{
    int trailer = card_.getTrailerBlockOfSector(sector);
    ensureKeyLoaded(ki);
    reader_.auth(static_cast<BYTE>(trailer), ki.kt, ki.slot);
    lastAuthSector_ = sector;
    lastAuthKT_     = ki.kt;
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
    invalidateAuth();
    ensureAuth(sector);
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
    if (card_.isDesfire()) return 0;  // DESFire: SelectApp + Auth gerekli

    int totalBlocks  = card_.getTotalBlocks();
    int totalSectors = card_.getTotalSectors();
    int okCount = 0;

    // Geçici raw buffer
    size_t memSize = card_.getTotalMemory();
    BYTEV rawBuf(memSize, 0);

    for (int s = 0; s < totalSectors; ++s) {
        // Auth
        try { ensureAuth(s); }
        catch (...) {
            invalidateAuth();
            continue;                       // sektör bozuk, atla
        }

        int first = card_.getFirstBlockOfSector(s);
        int last  = card_.getLastBlockOfSector(s);

        for (int b = first; b <= last; ++b) {
            try {
                BYTEV d = reader_.readPage(static_cast<BYTE>(b));
                if (d.size() >= 16) {
                    std::memcpy(&rawBuf[b * 16], d.data(), 16);
                    ++okCount;
                }
            }
            catch (...) { /* blok okunamadı */ }
        }
    }

    // Memory'ye yükle
    card_.loadMemory(rawBuf.data(), memSize);
    return okCount;
}

bool CardIO::readSector(int sector)
{
    if (card_.isDesfire()) return false;  // no sectors in DESFire

    try { ensureAuth(sector); }
    catch (...) { invalidateAuth(); return false; }

    int first = card_.getFirstBlockOfSector(sector);
    int last  = card_.getLastBlockOfSector(sector);
    bool allOk = true;

    CardMemoryLayout& mem = card_.getMemoryMutable();
    BYTE* raw = mem.getRawMemory();

    for (int b = first; b <= last; ++b) {
        try {
            BYTEV d = reader_.readPage(static_cast<BYTE>(b));
            if (d.size() >= 16)
                std::memcpy(raw + b * 16, d.data(), 16);
            else
                allOk = false;
        }
        catch (...) { allOk = false; }
    }
    return allOk;
}

BYTEV CardIO::readBlock(int block)
{
    int sector = card_.getSectorForBlock(block);
    ensureAuth(sector);

    BYTEV d = reader_.readPage(static_cast<BYTE>(block));

    // Memory'yi de güncelle
    if (d.size() >= 16) {
        CardMemoryLayout& mem = card_.getMemoryMutable();
        std::memcpy(mem.getRawMemory() + block * 16, d.data(), 16);
    }
    return d;
}

// ════════════════════════════════════════════════════════════════════════════════
// Kart Yazma
// ════════════════════════════════════════════════════════════════════════════════

void CardIO::writeBlock(int block, const BYTE data[16])
{
    if (card_.isManufacturerBlock(block))
        throw std::runtime_error("Block 0 (manufacturer) yazilamaz");
    if (card_.isTrailerBlock(block))
        throw std::runtime_error("Trailer blogu dogrudan yazilamaz");

    int sector = card_.getSectorForBlock(block);
    ensureAuth(sector, AuthPurpose::Write);

    if (card_.isUltralight()) {
        // Ultralight WRITE komutu: sayfa başına 4 byte
        // Virtual block içindeki 4 page'i sırayla yaz
        int basePage = block * 4;
        for (int p = 0; p < 4; ++p) {
            reader_.writePage(static_cast<BYTE>(basePage + p), data + p * 4);
        }
    } else {
        reader_.writePage(static_cast<BYTE>(block), data);
    }

    // Memory'yi de güncelle
    CardMemoryLayout& mem = card_.getMemoryMutable();
    std::memcpy(mem.getRawMemory() + block * 16, data, 16);
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
    if (card_.isDesfire())
        throw std::logic_error("DESFire has no trailer blocks");

    int trailerBlock = card_.getTrailerBlockOfSector(sector);
    ensureAuth(sector);

    BYTEV raw = reader_.readPage(static_cast<BYTE>(trailerBlock));
    if (raw.size() < 16)
        throw std::runtime_error("Trailer block read failed");

    // Memory'yi güncelle
    CardMemoryLayout& mem = card_.getMemoryMutable();
    std::memcpy(mem.getRawMemory() + trailerBlock * 16, raw.data(), 16);

    // Parse
    MifareBlock blk;
    std::memcpy(blk.raw, raw.data(), 16);
    return TrailerConfig::fromBlock(blk);
}

void CardIO::writeTrailer(int sector, const TrailerConfig& config)
{
    if (card_.isDesfire())
        throw std::logic_error("DESFire has no trailer blocks");

    if (!config.isValid())
        throw std::invalid_argument("Geçersiz access bits — kart kilitlenebilir!");

    int trailerBlock = card_.getTrailerBlockOfSector(sector);
    ensureAuth(sector, AuthPurpose::Write);

    MifareBlock blk = config.toBlock();
    reader_.writePage(static_cast<BYTE>(trailerBlock), blk.raw);

    // Memory'yi güncelle
    CardMemoryLayout& mem = card_.getMemoryMutable();
    std::memcpy(mem.getRawMemory() + trailerBlock * 16, blk.raw, 16);
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
    if (card_.isDesfire()) return {};  // no trailers

    int totalSectors = card_.getTotalSectors();
    std::vector<TrailerConfig> configs;
    configs.reserve(totalSectors);

    for (int s = 0; s < totalSectors; ++s) {
        try {
            configs.push_back(readTrailer(s));
        }
        catch (...) {
            // Auth başarısız — factory default ekle (placeholder)
            configs.push_back(TrailerConfig::factoryDefault());
        }
    }
    return configs;
}

void CardIO::restoreAllTrailers(const std::vector<TrailerConfig>& configs)
{
    if (card_.isDesfire()) return;  // no trailers

    int totalSectors = card_.getTotalSectors();
    int count = static_cast<int>(configs.size());
    if (count > totalSectors) count = totalSectors;

    for (int s = 0; s < count; ++s) {
        try {
            writeTrailer(s, configs[s]);
        }
        catch (...) {
            // Sektör 0 manufacturer — yazılamaz, atla
        }
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
    return reader_.cardConnection().transmit(apdu);
}

std::function<BYTEV(const BYTEV&)> CardIO::makeTransmitFn() {
    return [this](const BYTEV& apdu) -> BYTEV {
        return desfireTransmit(apdu);
    };
}

static void requireDesfire(const CardInterface& card, const char* op) {
    if (!card.isDesfire())
        throw std::logic_error(std::string(op) + ": requires DESFire card");
}

// Session timeout kontrolü — auth gerektiren komutlarda çağrılır
void requireAuth(const DesfireSession* session, const char* op) {
    if (!session || !session->authenticated)
        throw std::runtime_error(std::string(op) + ": not authenticated");
    if (session->isExpired())
        throw std::runtime_error(std::string(op) + ": session expired");
}

// ════════════════════════════════════════════════════════════════════════════════
// DESFire — Public API
// ════════════════════════════════════════════════════════════════════════════════

DesfireVersionInfo CardIO::discoverCard() {
    requireDesfire(card_, "discoverCard");

    auto vi = DesfireCommands::parseGetVersion(makeTransmitFn());

    DesfireMemoryLayout& dfm = card_.getDesfireMemoryMutable();
    dfm.initFromVersion(vi);

    return vi;
}

void CardIO::selectApplication(const DesfireAID& aid) {
    requireDesfire(card_, "selectApplication");

    BYTEV cmd = DesfireCommands::selectApplication(aid);
    BYTEV resp = desfireTransmit(cmd);
    DesfireCommands::checkResponse(resp, "SelectApplication");

    // Session düşer — yeniden auth gerekir
    if (desfireSession_)
        desfireSession_->resetKeepApp();

    if (desfireSession_)
        desfireSession_->currentAID = aid;

    // Model tarafında da seçili app'i güncelle
    card_.getDesfireMemoryMutable().currentAID = aid;
}

void CardIO::authenticateDesfire(const BYTEV& key, BYTE keyNo, DesfireKeyType keyType) {
    requireDesfire(card_, "authenticateDesfire");

    if (!desfireSession_)
        desfireSession_ = std::make_unique<DesfireSession>();

    DesfireAuth auth;
    auth.authenticate(*desfireSession_, key, keyNo, keyType, makeTransmitFn());
}

std::vector<DesfireAID> CardIO::getApplicationIDs() {
    requireDesfire(card_, "getApplicationIDs");

    BYTEV data = DesfireCommands::transceive(makeTransmitFn(),
                    DesfireCommands::getApplicationIDs());
    return DesfireCommands::parseApplicationIDs(data);
}

std::vector<BYTE> CardIO::getFileIDs() {
    requireDesfire(card_, "getFileIDs");

    BYTEV data = DesfireCommands::transceive(makeTransmitFn(),
                    DesfireCommands::getFileIDs());
    return DesfireCommands::parseFileIDs(data);
}

DesfireFileSettings CardIO::getFileSettings(BYTE fileNo) {
    requireDesfire(card_, "getFileSettings");

    BYTEV data = DesfireCommands::transceive(makeTransmitFn(),
                    DesfireCommands::getFileSettings(fileNo));
    return DesfireCommands::parseFileSettings(data);
}

BYTEV CardIO::readFileData(BYTE fileNo, uint32_t offset, uint32_t length) {
    requireDesfire(card_, "readFileData");

    BYTEV cmd = DesfireCommands::readData(fileNo, offset, length);
    return DesfireCommands::transceive(makeTransmitFn(), cmd);
}

void CardIO::writeFileData(BYTE fileNo, uint32_t offset, const BYTEV& data) {
    requireDesfire(card_, "writeFileData");

    BYTEV cmd = DesfireCommands::writeData(fileNo, offset, data);
    BYTEV resp = desfireTransmit(cmd);
    DesfireCommands::checkResponse(resp, "WriteData");
}

BYTEV CardIO::readRecords(BYTE fileNo, uint32_t offset, uint32_t count) {
    requireDesfire(card_, "readRecords");
    BYTEV cmd = DesfireCommands::readRecords(fileNo, offset, count);
    return DesfireCommands::transceive(makeTransmitFn(), cmd);
}

void CardIO::appendRecord(BYTE fileNo, const BYTEV& recordData) {
    requireDesfire(card_, "appendRecord");
    BYTEV cmd = DesfireCommands::appendRecord(fileNo, recordData);
    BYTEV resp = desfireTransmit(cmd);
    DesfireCommands::checkResponse(resp, "AppendRecord");
}

size_t CardIO::getFreeMemory() {
    requireDesfire(card_, "getFreeMemory");

    BYTEV data = DesfireCommands::transceive(makeTransmitFn(),
                    DesfireCommands::getFreeMemory());
    return DesfireCommands::parseFreeMemory(data);
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
    requireDesfire(card_, "createApplication");
    BYTEV resp = desfireTransmit(
        DesfireCommands::createApplication(aid, keySettings, maxKeys, keyType));
    DesfireCommands::checkResponse(resp, "CreateApplication");
}

void CardIO::deleteApplication(const DesfireAID& aid) {
    requireDesfire(card_, "deleteApplication");
    BYTEV resp = desfireTransmit(DesfireCommands::deleteApplication(aid));
    DesfireCommands::checkResponse(resp, "DeleteApplication");
}

void CardIO::createStdDataFile(BYTE fileNo, DesfireCommMode comm,
                                const DesfireAccessRights& access,
                                uint32_t fileSize) {
    requireDesfire(card_, "createStdDataFile");
    BYTEV resp = desfireTransmit(
        DesfireCommands::createStdDataFile(fileNo, comm, access, fileSize));
    DesfireCommands::checkResponse(resp, "CreateStdDataFile");
}

void CardIO::createValueFile(BYTE fileNo, DesfireCommMode comm,
                              const DesfireAccessRights& access,
                              int32_t lower, int32_t upper, int32_t value,
                              bool limitedCredit) {
    requireDesfire(card_, "createValueFile");
    BYTEV resp = desfireTransmit(
        DesfireCommands::createValueFile(fileNo, comm, access,
                                          lower, upper, value, limitedCredit));
    DesfireCommands::checkResponse(resp, "CreateValueFile");
}

void CardIO::createLinearRecordFile(BYTE fileNo, DesfireCommMode comm,
                                     const DesfireAccessRights& access,
                                     uint32_t recordSize, uint32_t maxRecords) {
    requireDesfire(card_, "createLinearRecordFile");
    BYTEV resp = desfireTransmit(
        DesfireCommands::createLinearRecordFile(fileNo, comm, access,
                                                 recordSize, maxRecords));
    DesfireCommands::checkResponse(resp, "CreateLinearRecordFile");
}

void CardIO::deleteFile(BYTE fileNo) {
    requireDesfire(card_, "deleteFile");
    BYTEV resp = desfireTransmit(DesfireCommands::deleteFile(fileNo));
    DesfireCommands::checkResponse(resp, "DeleteFile");
}
