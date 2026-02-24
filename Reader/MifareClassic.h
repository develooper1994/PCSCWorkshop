#ifndef MIFARECLASSIC_H
#define MIFARECLASSIC_H

#include <cstdint>
#include <utility>
#include <cstring>
#include <stdexcept>
#include "CipherTypes.h"
#include "Reader/Reader.h"

struct Classic1KPolicy
{
    static constexpr uint8_t SectorCount = 16;
    static constexpr uint8_t BlocksPerSector = 4;

    static uint8_t sectorTrailerBlock(uint8_t sector)
    {
        return sector * 4 + 3;
    }

    static bool isTrailer(uint8_t block)
    {
        return (block % 4) == 3;
    }

    static bool isDataBlock(uint8_t block)
    {
        return (block % 4) != 3;
    }

    static uint8_t absoluteBlock(uint8_t sector, uint8_t index)
    {
        return sector * 4 + index;
    }
};

// ---------- layout_iface.h ----------
struct IMifareLayoutPolicy {
    virtual ~IMifareLayoutPolicy() = default;

    virtual uint16_t totalSectors() const = 0;
    virtual uint8_t blocksPerSector(uint16_t sector) const = 0;

    virtual uint32_t absoluteBlock(uint16_t sector, uint8_t index) const = 0;
    virtual std::pair<uint16_t, uint8_t> sectorAndIndex(uint32_t abs) const = 0;

    virtual uint32_t trailerBlock(uint16_t sector) const = 0;
    virtual bool isTrailer(uint32_t abs) const = 0;
    virtual bool isManufacturerBlock(uint32_t abs) const = 0;

    virtual const char* name() const = 0;
};

struct IAccessPolicy {
    virtual ~IAccessPolicy() = default;
    virtual void validate(const BYTE access[4]) const = 0;
    virtual void transport(BYTE out[4]) const = 0;
};

struct IAuthPolicy {
    virtual ~IAuthPolicy() = default;
    virtual uint32_t authBlock(uint16_t sector) const = 0;
};

// ---------- static policies (örnek) ----------
struct Classic1KLayout {
    static constexpr uint16_t sectors = 16;
    static constexpr uint8_t blocksInSector = 4;
    static const char* name() { return "MIFARE Classic 1K"; }
    static constexpr bool isManufacturerBlock(uint32_t abs) { return abs == 0; }
    static constexpr uint32_t totalBlocks() { return sectors * blocksInSector; }
    static constexpr uint16_t totalSectors() { return sectors; }
    static uint32_t absolute(uint16_t s, uint8_t i) { return static_cast<uint32_t>(s * blocksInSector + i); }
    static std::pair<uint16_t, uint16_t> sectorAndIndex(uint32_t a) { return { uint16_t(a / blocksInSector), uint16_t(a % blocksInSector) }; }
    static uint32_t trailer(uint16_t s) { return absolute(s, static_cast<uint8_t>(blocksInSector-1)); }
    static bool isTrailer(uint32_t a) { return (a % blocksInSector) == (blocksInSector - 1); }
    static uint8_t blocksPerSector(uint16_t /*sector*/) { return blocksInSector; }
};

struct ClassicAccessPolicy {
    static void validateStatic(const BYTE a[4]) {
        // TODO: gerçek inverted-bit kontrolü eklenmeli
        (void)a;
    }

    static void transportStatic(BYTE out[4]) {
        out[0] = 0xFF;
        out[1] = 0x07;
        out[2] = 0x80;
        out[3] = 0x69;
    }
};

struct ClassicAuthPolicy {
    static uint32_t authBlockStatic(uint16_t sector) { return Classic1KLayout::absolute(sector, 0); }
};

// ---------- adapters: static -> runtime ----------
template<typename StaticLayout>
struct StaticLayoutAdapter : IMifareLayoutPolicy {
    uint16_t totalSectors() const override { return StaticLayout::totalSectors(); }
    uint8_t blocksPerSector(uint16_t sector) const override { return StaticLayout::blocksPerSector(static_cast<uint16_t>(sector)); }
    uint32_t absoluteBlock(uint16_t s, uint8_t i) const override { return StaticLayout::absolute(s, i); }
    std::pair<uint16_t, uint8_t> sectorAndIndex(uint32_t a) const override { auto p = StaticLayout::sectorAndIndex(a); return { p.first, static_cast<uint8_t>(p.second) }; }
    uint32_t trailerBlock(uint16_t s) const override { return StaticLayout::trailer(s); }
    bool isTrailer(uint32_t a) const override { return StaticLayout::isTrailer(a); }
    const char* name() const override { return StaticLayout::name(); }
    bool isManufacturerBlock(uint32_t abs) const override { return StaticLayout::isManufacturerBlock(abs); }
};

template<typename StaticAccess>
struct StaticAccessAdapter : IAccessPolicy {
    void validate(const BYTE a[4]) const override { StaticAccess::validateStatic(a); }
    void transport(BYTE out[4]) const override { StaticAccess::transportStatic(out); }
};

template<typename StaticAuth>
struct StaticAuthAdapter : IAuthPolicy {
    uint32_t authBlock(uint16_t sector) const override { return StaticAuth::authBlockStatic(sector); }
};

// ---------- SectorTrailer ----------
struct SectorTrailer {
    BYTE keyA[6];
    BYTE access[4];
    BYTE keyB[6];

    void toBytes(BYTE out[16]) const {
        std::memcpy(out, keyA, 6);
        std::memcpy(out + 6, access, 4);
        std::memcpy(out + 10, keyB, 6);
    }

    static SectorTrailer fromBytes(const BYTE in[16]) {
        SectorTrailer t;
        std::memcpy(t.keyA, in, 6);
        std::memcpy(t.access, in + 6, 4);
        std::memcpy(t.keyB, in + 10, 6);
        return t;
    }
};

// ---------- MifareCardCore (policy-agnostic runtime orchestrator) ----------
class MifareCardCore {
public:
    MifareCardCore(Reader& r,
        IMifareLayoutPolicy& layout,
        IAccessPolicy& access,
        IAuthPolicy& auth);

    void loadKey(const BYTE key[6], KeyStructure ks, BYTE slot);

    void read(uint16_t sector,
        uint8_t index,
        BYTE out[16],
        KeyType kt,
        BYTE slot);

    void write(uint16_t sector,
        uint8_t index,
        const BYTE data[16],
        KeyType kt,
        BYTE slot);

    SectorTrailer readTrailer(uint16_t sector,
        KeyType kt,
        BYTE slot);

    void changeKeys(uint16_t sector,
        const SectorTrailer& tr,
        KeyType kt,
        BYTE slot);

private:
    void validateSector(uint16_t sector) const;
    void validateLocation(uint16_t sector, uint8_t index) const;

    void ensureAuth(uint16_t sector, KeyType kt, BYTE slot);
    void invalidateAuth();

    Reader& reader;
    IMifareLayoutPolicy& layout;
    IAccessPolicy& access;
    IAuthPolicy& auth;

    uint16_t authSector = 0xFFFF;
    BYTE authSlot = 0xFF;
    KeyType authType = KeyType::A;
};

// ---------- Factory / wrapper örneði ----------
struct Classic1KCard {
    StaticLayoutAdapter<Classic1KLayout> layout;
    StaticAccessAdapter<ClassicAccessPolicy> access;
    StaticAuthAdapter<ClassicAuthPolicy> auth;
    MifareCardCore core;

    Classic1KCard(Reader& r): core(r, layout, access, auth) {}
};

#endif // !MIFARECLASSIC_H