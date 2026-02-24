#include "MifareClassic.h"
#include <stdexcept>
#include <cstring>

MifareCardCore::MifareCardCore(Reader& r,
    IMifareLayoutPolicy& layout,
    IAccessPolicy& access,
    IAuthPolicy& auth)
    : reader(r), layout(layout), access(access), auth(auth)
{
    // MIFARE Classic block size is 16 bytes
    reader.setLC(16);
}

void MifareCardCore::loadKey(const BYTE key[6], KeyStructure ks, BYTE slot) {
    reader.loadKey(key, ks, slot);
}

void MifareCardCore::read(uint16_t sector,
    uint8_t index,
    BYTE out[16],
    KeyType kt,
    BYTE slot)
{
    validateLocation(sector, index);
    ensureAuth(sector, kt, slot);

    uint32_t block = layout.absoluteBlock(sector, index);

    if (layout.isTrailer(block))
        throw std::runtime_error("Use readTrailer()");

    auto data = reader.readPage(static_cast<BYTE>(block));
    if (data.size() != 16)
        throw std::runtime_error("Short read");

    std::memcpy(out, data.data(), 16);
}

void MifareCardCore::write(uint16_t sector,
    uint8_t index,
    const BYTE data[16],
    KeyType kt,
    BYTE slot)
{
    validateLocation(sector, index);
    ensureAuth(sector, kt, slot);

    uint32_t block = layout.absoluteBlock(sector, index);

    if (layout.isTrailer(block))
        throw std::runtime_error("Use changeKeys()");

    if (layout.isManufacturerBlock(block))
        throw std::runtime_error("Manufacturer block is read-only");

    reader.writePage(static_cast<BYTE>(block), data);
}

SectorTrailer MifareCardCore::readTrailer(uint16_t sector,
    KeyType kt,
    BYTE slot)
{
    validateSector(sector);
    ensureAuth(sector, kt, slot);

    auto data = reader.readPage(static_cast<BYTE>(layout.trailerBlock(sector)));

    if (data.size() != 16) throw std::runtime_error("Short read");

    return SectorTrailer::fromBytes(data.data());
}

void MifareCardCore::changeKeys(uint16_t sector,
    const SectorTrailer& tr,
    KeyType kt,
    BYTE slot)
{
    validateSector(sector);
    ensureAuth(sector, kt, slot);

    access.validate(tr.access);

    BYTE raw[16];
    tr.toBytes(raw);

    reader.writePage(static_cast<BYTE>(layout.trailerBlock(sector)), raw);

    invalidateAuth();
}

void MifareCardCore::validateSector(uint16_t sector) const {
    if (sector >= layout.totalSectors()) throw std::out_of_range("Invalid sector");
}

void MifareCardCore::validateLocation(uint16_t sector, uint8_t index) const {
    validateSector(sector);

    if (index >= layout.blocksPerSector(sector))
        throw std::out_of_range("Invalid block index");
}

void MifareCardCore::ensureAuth(uint16_t sector, KeyType kt, BYTE slot) {
    if (authSector == sector && authType == kt && authSlot == slot) return;

    uint32_t block = auth.authBlock(sector);
    reader.auth(static_cast<BYTE>(block), kt, slot);

    authSector = sector;
    authType = kt;
    authSlot = slot;
}

void MifareCardCore::invalidateAuth() {
    authSector = 0xFFFF;
    authSlot = 0xFF;
}

