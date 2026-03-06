#include "CardIO.h"
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
// Erişim
// ════════════════════════════════════════════════════════════════════════════════

CardInterface&       CardIO::card()       { return card_; }
const CardInterface& CardIO::card() const { return card_; }
Reader&              CardIO::reader()     { return reader_; }
