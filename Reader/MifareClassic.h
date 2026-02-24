#ifndef MIFARECLASSIC_H
#define MIFARECLASSIC_H

#include <cstdint>
#include <utility>
#include "CipherTypes.h"
#include "Readers.h"

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
    virtual uint32_t totalBlocks() const = 0;
    virtual uint16_t totalSectors() const = 0;
    virtual uint32_t absoluteBlock(uint16_t sector, uint16_t index) const = 0;
    virtual std::pair<uint16_t, uint16_t> sectorAndIndex(uint32_t abs) const = 0;
    virtual uint32_t trailerBlock(uint16_t sector) const = 0;
    virtual bool isTrailer(uint32_t abs) const = 0;
    virtual bool isDataBlock(uint32_t abs) const = 0;
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
    static constexpr uint16_t blocksInSector = 4;
    static const char* name() { return "MIFARE Classic 1K"; }
    static uint32_t totalBlocks() { return sectors * blocksInSector; }
    static uint16_t totalSectors() { return sectors; }
    static uint32_t absolute(uint16_t s, uint16_t i) { return s * blocksInSector + i; }
    static std::pair<uint16_t, uint16_t> sectorAndIndex(uint32_t a) { return { uint16_t(a / blocksInSector), uint16_t(a % blocksInSector) }; }
    static uint32_t trailer(uint16_t s) { return absolute(s, blocksInSector-1); }
    static bool isTrailer(uint32_t a) { return (a % blocksInSector) == (blocksInSector - 1); }
    static constexpr bool isManufacturerBlock(uint32_t abs) { return abs == 0; }
};
struct ClassicAccessPolicy {
    static void validateStatic(const BYTE a[4]) { /* check inverted bits */ }
    static void transportStatic(BYTE out[4]) { out[0] = 0xFF; out[1] = 0x07; out[2] = 0x80; out[3] = 0x69; }
};
struct ClassicAuthPolicy {
    static uint32_t authBlockStatic(uint16_t sector) { return Classic1KLayout::absolute(sector, 0); }
};

// ---------- adapters: static -> runtime ----------
template<typename StaticLayout>
struct StaticLayoutAdapter : IMifareLayoutPolicy {
    uint32_t totalBlocks() const override { return StaticLayout::totalBlocks(); }
    uint16_t totalSectors() const override { return StaticLayout::totalSectors(); }
    uint32_t absoluteBlock(uint16_t s, uint16_t i) const override { return StaticLayout::absolute(s, i); }
    std::pair<uint16_t, uint16_t> sectorAndIndex(uint32_t a) const override { return StaticLayout::sectorAndIndex(a); }
    uint32_t trailerBlock(uint16_t s) const override { return StaticLayout::trailer(s); }
    bool isTrailer(uint32_t a) const override { return StaticLayout::isTrailer(a); }
    bool isDataBlock(uint32_t a) const override { return !StaticLayout::isTrailer(a); }
    const char* name() const override { return StaticLayout::name(); }
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
};

// ---------- MifareCardCore (policy-agnostic runtime orchestrator) ----------
/* 

// ---------- Factory / wrapper örneði ----------
struct Classic1KCardWrapper {
    StaticLayoutAdapter<Classic1KLayout> layoutAdapter;
    StaticAccessAdapter<ClassicAccessPolicy> accessAdapter;
    StaticAuthAdapter<ClassicAuthPolicy> authAdapter;
    MifareCardCore core;

    Classic1KCardWrapper(IReader& r)
      : core(r, layoutAdapter, accessAdapter, authAdapter) {}
    // core'un metotlarýný sarmala ya da core'u public yap
};
Kullaným:
  - Compile-time (template) hýz istersen doðrudan Static* tipleriyle optimize edilmiþ kod yazarsýn.
  - Runtime esnekliði istersen Classic1KCardWrapper gibi factory ile runtime policy adaptörleri oluþturup MifareCardCore'u kullanýrsýn.
*/
class MifareCardCore {
public:
    MifareCardCore(Reader& r,
        IMifareLayoutPolicy& layout,
        IAccessPolicy& access,
        IAuthPolicy& auth)
        : reader(r), layout(layout), access(access), auth(auth) {
    }

    void loadKey(uint8_t slot, const BYTE key[6]) { reader.loadKey(slot, key); }

    void read(uint16_t sector, uint16_t index, BYTE out[16], KeyType kt, uint8_t slot) {
        ensureAuth(sector, kt, slot);
        uint32_t block = layout.absoluteBlock(sector, index);
        if (layout.isTrailer(block)) throw std::runtime_error("Use readTrailer");
        reader.readBlockRaw(block, out);
    }

    void write(uint16_t sector, uint16_t index, const BYTE data[16], KeyType kt, uint8_t slot) {
        ensureAuth(sector, kt, slot);
        uint32_t block = layout.absoluteBlock(sector, index);
        if (layout.isTrailer(block)) throw std::runtime_error("Use changeKeys");
        reader.writeBlockRaw(block, data);
    }

    void changeKeys(uint16_t sector, const SectorTrailer& tr, KeyType kt, uint8_t slot) {
        ensureAuth(sector, kt, slot);
        access.validate(tr.access);
        BYTE raw[16]; tr.toBytes(raw);
        reader.writePage(layout.trailerBlock(sector), raw);
        invalidateAuth();
    }

private:
    void ensureAuth(uint16_t sector, KeyType kt, uint8_t slot) {
        if (authSector == (int)sector && authType == kt) return;
        uint32_t block = auth.authBlock(sector);
        reader.auth(block, kt, slot);
        authSector = sector; authType = kt;
    }
    void invalidateAuth() { authSector = -1; }

    Reader& reader;
    IMifareLayoutPolicy& layout;
    IAccessPolicy& access;
    IAuthPolicy& auth;
    int authSector = -1;
    KeyType authType = KeyType::A;
};


#endif // !MIFARECLASSIC_H