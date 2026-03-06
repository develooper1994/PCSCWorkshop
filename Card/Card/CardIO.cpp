#include "CardIO.h"
#include "CardModel/CardMemoryLayout.h"
#include "CardModel/TrailerConfig.h"
#include <cstring>
#include <stdexcept>

// ════════════════════════════════════════════════════════════════════════════════
// Construction
// ════════════════════════════════════════════════════════════════════════════════

CardIO::CardIO(Reader& reader, bool is4K)
    : reader_(reader), card_(is4K)
{
    // Reader'ın auto-auth'unu kapat; auth'u biz yöneteceğiz
    reader_.setAuthRequested(false);
}

// ════════════════════════════════════════════════════════════════════════════════
// Key Ayarları
// ════════════════════════════════════════════════════════════════════════════════

void CardIO::setDefaultKey(const KEYBYTES& key, KeyStructure ks,
                           BYTE slot, KeyType kt)
{
    defaultKey_  = key;
    defaultKS_   = ks;
    defaultSlot_ = slot;
    defaultKT_   = kt;
}

void CardIO::setKeys(const KEYBYTES& keyA, BYTE slotA,
                     const KEYBYTES& keyB, BYTE slotB,
                     KeyStructure ks)
{
    // İlk key default olsun
    setDefaultKey(keyA, ks, slotA, KeyType::A);

    // İkisini de CardInterface'e kaydet
    card_.registerKey(KeyType::A, keyA, ks, slotA, "KeyA");
    card_.registerKey(KeyType::B, keyB, ks, slotB, "KeyB");
}

// ════════════════════════════════════════════════════════════════════════════════
// Auth
// ════════════════════════════════════════════════════════════════════════════════

void CardIO::ensureAuth(int sector)
{
    if (sector == lastAuthSector_) return;

    int trailer = card_.getTrailerBlockOfSector(sector);
    reader_.loadKey(defaultKey_.data(), defaultKS_, defaultSlot_);
    reader_.auth(static_cast<BYTE>(trailer), defaultKT_, defaultSlot_);
    lastAuthSector_ = sector;
}

void CardIO::authenticate(int sector)
{
    lastAuthSector_ = -1;          // zorla yeniden auth
    ensureAuth(sector);
}

void CardIO::authenticate(int sector, KeyType kt, BYTE slot)
{
    int trailer = card_.getTrailerBlockOfSector(sector);
    reader_.loadKey(defaultKey_.data(), defaultKS_, slot);
    reader_.auth(static_cast<BYTE>(trailer), kt, slot);
    lastAuthSector_ = sector;
}

// ════════════════════════════════════════════════════════════════════════════════
// Kart Okuma
// ════════════════════════════════════════════════════════════════════════════════

int CardIO::readCard()
{
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
            lastAuthSector_ = -1;
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
    try { ensureAuth(sector); }
    catch (...) { lastAuthSector_ = -1; return false; }

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
    ensureAuth(sector);

    reader_.writePage(static_cast<BYTE>(block), data);

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
    if (!config.isValid())
        throw std::invalid_argument("Geçersiz access bits — kart kilitlenebilir!");

    int trailerBlock = card_.getTrailerBlockOfSector(sector);
    ensureAuth(sector);

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
    const MifareBlock& trailer = card_.getBlock(
        card_.getTrailerBlockOfSector(sector));
    ACCESSBYTES ab;
    std::memcpy(ab.data(), trailer.trailer.accessBits, 4);
    return AccessBitsCodec::decode(ab);
}

void CardIO::setAccessConfig(int sector, const SectorAccessConfig& config)
{
    // Mevcut trailer'ı oku
    TrailerConfig tc = readTrailer(sector);
    tc.access = config;
    writeTrailer(sector, tc);
}

void CardIO::setSectorMode(int sector, SectorMode mode)
{
    setAccessConfig(sector, sectorModeToConfig(mode));
}

DataBlockPermission CardIO::getDataPermission(int sector, int blockIndex) const
{
    SectorAccessConfig cfg = getAccessConfig(sector);
    return cfg.dataPermission(blockIndex);
}

TrailerPermission CardIO::getTrailerPermission(int sector) const
{
    SectorAccessConfig cfg = getAccessConfig(sector);
    return cfg.trailerPermission();
}

// ════════════════════════════════════════════════════════════════════════════════
// Bulk Trailer İşlemleri
// ════════════════════════════════════════════════════════════════════════════════

std::vector<TrailerConfig> CardIO::saveAllTrailers()
{
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
