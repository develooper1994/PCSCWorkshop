#ifndef TRAILERCONFIG_H
#define TRAILERCONFIG_H

#include "../CardDataTypes.h"
#include "BlockDefinition.h"
#include <array>
#include <cstring>
#include <stdexcept>
#include <string>

// ════════════════════════════════════════════════════════════════════════════════
// Mifare Classic Access Bits — Tam Spec Implementasyonu
// ════════════════════════════════════════════════════════════════════════════════
//
// Bu header Mifare Classic trailer bloğunun tüm bileşenlerini modeller:
//   - AccessCondition      : Tek blok için C1/C2/C3 bit üçlüsü
//   - DataBlockPermission  : C1C2C3 → data blok izin matrisi (spec tablosu)
//   - TrailerPermission    : C1C2C3 → trailer blok izin matrisi (spec tablosu)
//   - SectorAccessConfig   : Sektördeki 4 blok için access condition + GPB
//   - AccessBitsCodec      : ACCESSBYTES ↔ SectorAccessConfig encode/decode
//   - SectorMode           : Hazır izin şablonları (TRANSPORT, READ_ONLY_AB, ...)
//   - TrailerConfig        : KeyA + access + KeyB → MifareBlock serialize/parse
//
// ─── Trailer Block Yapısı (16 byte) ────────────────────────────────────────
//
//   Offset  Boyut  İçerik
//   ─────── ────── ─────────────────────────────────
//   0       6      Key A  (PCSC okumada 00 döner — asla okunamaz)
//   6       4      Access Bits  [byte6 byte7 byte8 GPB]
//   10      6      Key B
//
// ─── Access Bytes Bit Düzeni (byte 6-8) ────────────────────────────────────
//
//   Byte 6:  ~C2[3] ~C2[2] ~C2[1] ~C2[0] | ~C1[3] ~C1[2] ~C1[1] ~C1[0]
//   Byte 7:   C1[3]  C1[2]  C1[1]  C1[0] | ~C3[3] ~C3[2] ~C3[1] ~C3[0]
//   Byte 8:   C3[3]  C3[2]  C3[1]  C3[0] |  C2[3]  C2[2]  C2[1]  C2[0]
//   Byte 9:  GPB (General Purpose Byte — kullanıcı verisi)
//
//   Blok index: 0,1,2 = data blokları, 3 = trailer (sektör içi)
//   Her bitin ters kopyası (~ ile işaretli) bütünlük kontrolü için tutulur.
//   Factory default: FF 07 80 69  (data=000, trailer=001)
//
// ─── Kullanım Örnekleri ────────────────────────────────────────────────────
//
//   // 1) Raw access bits decode:
//   ACCESSBYTES raw = {0xFF, 0x07, 0x80, 0x69};
//   SectorAccessConfig cfg = AccessBitsCodec::decode(raw);
//   DataBlockPermission dp = cfg.dataPermission(0);
//   // dp.readA == true, dp.writeA == true  (transport mode)
//
//   // 2) Bütünlük kontrolü:
//   bool ok = AccessBitsCodec::verify(raw);   // true
//
//   // 3) Hazır mod ile yeni config oluştur:
//   SectorAccessConfig newCfg = sectorModeToConfig(SectorMode::READ_AB_WRITE_B);
//   ACCESSBYTES encoded = AccessBitsCodec::encode(newCfg);
//
//   // 4) Trailer config oluştur + serialize:
//   TrailerConfig tc;
//   tc.keyA  = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
//   tc.keyB  = {0xA0,0xA1,0xA2,0xA3,0xA4,0xA5};
//   tc.access = sectorModeToConfig(SectorMode::READ_AB_WRITE_B);
//   MifareBlock blk = tc.toBlock();          // → 16 byte trailer
//   bool valid = tc.isValid();               // bütünlük kontrolü
//
//   // 5) Trailer bloğunu parse et:
//   TrailerConfig parsed = TrailerConfig::fromBlock(someBlock);
//   KEYBYTES keyB = parsed.keyB;
//   TrailerPermission tp = parsed.access.trailerPermission();
//   // tp.accReadA, tp.keyBWriteB, ...
//
//   // 6) Permission sorgula:
//   DataBlockPermission dp = DataBlockPermission::fromCondition({true,false,false});
//   // C1=1,C2=0,C3=0 → readA=true, readB=true, writeA=false, writeB=true
//
// ════════════════════════════════════════════════════════════════════════════════

// ── Per-block access condition bits ─────────────────────────────────────────

struct AccessCondition {
    bool c1 = false;
    bool c2 = false;
    bool c3 = false;

    int toIndex() const { return (c1 ? 4 : 0) | (c2 ? 2 : 0) | (c3 ? 1 : 0); }
};

// ── Data block permissions (C1 C2 C3 → permission matrix) ──────────────────
//
//  C1C2C3 | Read  | Write | Inc   | Dec/Trans/Rest
//  -------|-------|-------|-------|----------------
//  0 0 0  | A|B   | A|B   | A|B   | A|B       (transport)
//  0 1 0  | A|B   | never | never | never     (read-only)
//  1 0 0  | A|B   | B     | never | never
//  1 1 0  | A|B   | B     | B     | A|B       (value block)
//  0 0 1  | A|B   | never | never | A|B       (value block, dec only)
//  0 1 1  | B     | B     | never | never     (KeyB only)
//  1 0 1  | B     | never | never | never     (KeyB read-only)
//  1 1 1  | never | never | never | never     (frozen)

struct DataBlockPermission {
    bool readA;
    bool readB;
    bool writeA;
    bool writeB;
    bool incrementA;
    bool incrementB;
    bool decrementA;
    bool decrementB;

    static DataBlockPermission fromCondition(const AccessCondition& ac) {
        // Index: c1*4 + c2*2 + c3
        int idx = ac.toIndex();
        //                         rdA   rdB   wrA   wrB   incA  incB  decA  decB
        static const DataBlockPermission table[8] = {
            /* 000 */ { true,  true,  true,  true,  true,  true,  true,  true  },
            /* 001 */ { true,  true,  false, false, false, false, true,  true  },
            /* 010 */ { true,  true,  false, false, false, false, false, false },
            /* 011 */ { false, true,  false, true,  false, false, false, false },
            /* 100 */ { true,  true,  false, true,  false, false, false, false },
            /* 101 */ { false, true,  false, false, false, false, false, false },
            /* 110 */ { true,  true,  false, true,  false, true,  true,  true  },
            /* 111 */ { false, false, false, false, false, false, false, false },
        };
        return table[idx];
    }

    bool canRead(KeyType kt) const {
        return (kt == KeyType::A) ? readA : readB;
    }
    bool canWrite(KeyType kt) const {
        return (kt == KeyType::A) ? writeA : writeB;
    }
};

// ── Trailer block permissions (C1 C2 C3 → trailer permission matrix) ───────
//
//  C1C2C3 | KeyA_W | Acc_R | Acc_W | KeyB_R | KeyB_W
//  -------|--------|-------|-------|--------|-------
//  0 0 0  | A      | A     | never | A      | A       (transport, no acc write)
//  0 0 1  | A      | A     | A     | A      | A       (transport, full control)
//  0 1 0  | never  | A     | never | A      | never   (read-only config)
//  0 1 1  | B      | A|B   | B     | never  | B
//  1 0 0  | B      | A|B   | never | never  | B
//  1 0 1  | never  | A|B   | B     | never  | never
//  1 1 0  | never  | A|B   | never | never  | never   (frozen config)
//  1 1 1  | never  | A|B   | never | never  | never   (frozen config)
//
// NOTE: KeyA is NEVER readable on any condition.

struct TrailerPermission {
    // KeyA write (who can change KeyA)
    bool keyAWriteA;
    bool keyAWriteB;
    // Access bits read
    bool accReadA;
    bool accReadB;
    // Access bits write
    bool accWriteA;
    bool accWriteB;
    // KeyB read
    bool keyBReadA;
    bool keyBReadB;
    // KeyB write
    bool keyBWriteA;
    bool keyBWriteB;

    static TrailerPermission fromCondition(const AccessCondition& ac) {
        int idx = ac.toIndex();
        //                            kAw_A kAw_B aR_A  aR_B  aW_A  aW_B  kBr_A kBr_B kBw_A kBw_B
        static const TrailerPermission table[8] = {
            /* 000 */ { true,  false, true,  false, false, false, true,  false, true,  false },
            /* 001 */ { true,  false, true,  false, true,  false, true,  false, true,  false },
            /* 010 */ { false, false, true,  false, false, false, true,  false, false, false },
            /* 011 */ { false, true,  true,  true,  false, true,  false, false, false, true  },
            /* 100 */ { false, true,  true,  true,  false, false, false, false, false, true  },
            /* 101 */ { false, false, true,  true,  false, true,  false, false, false, false },
            /* 110 */ { false, false, true,  true,  false, false, false, false, false, false },
            /* 111 */ { false, false, true,  true,  false, false, false, false, false, false },
        };
        return table[idx];
    }

    bool canRead(KeyType kt) const {
        // "Read trailer" = can read access bits
        return (kt == KeyType::A) ? accReadA : accReadB;
    }
    bool canWrite(KeyType kt) const {
        // "Write trailer" = can modify access bits
        return (kt == KeyType::A) ? accWriteA : accWriteB;
    }
};

// ════════════════════════════════════════════════════════════════════════════════
// SectorAccessConfig — Bir sektörün tam access konfigürasyonu
// ════════════════════════════════════════════════════════════════════════════════

struct SectorAccessConfig {
    AccessCondition dataBlock[3];     // block 0, 1, 2
    AccessCondition trailer;          // block 3 (trailer)
    BYTE gpb = 0x69;                  // General Purpose Byte (byte 9)

    DataBlockPermission dataPermission(int blockIndex) const {
        if (blockIndex < 0 || blockIndex > 2)
            throw std::out_of_range("blockIndex must be 0-2");
        return DataBlockPermission::fromCondition(dataBlock[blockIndex]);
    }

    TrailerPermission trailerPermission() const {
        return TrailerPermission::fromCondition(trailer);
    }

    // Factory default: data=000, trailer=001, gpb=0x69
    static SectorAccessConfig factoryDefault() {
        SectorAccessConfig cfg;
        cfg.dataBlock[0] = {false, false, false};
        cfg.dataBlock[1] = {false, false, false};
        cfg.dataBlock[2] = {false, false, false};
        cfg.trailer      = {false, false, true};   // 001
        cfg.gpb = 0x69;
        return cfg;
    }
};

// ════════════════════════════════════════════════════════════════════════════════
// AccessBitsCodec — Encode / Decode ACCESSBYTES ↔ SectorAccessConfig
// ════════════════════════════════════════════════════════════════════════════════

namespace AccessBitsCodec {

    // Decode 4-byte access bits → SectorAccessConfig
    inline SectorAccessConfig decode(const ACCESSBYTES& ab) {
        BYTE b6 = ab[0], b7 = ab[1], b8 = ab[2];
        SectorAccessConfig cfg;
        cfg.gpb = ab[3];

        for (int i = 0; i < 4; ++i) {
            bool c1 = (b7 >> (4 + i)) & 1;
            bool c2 = (b8 >> i)       & 1;
            bool c3 = (b8 >> (4 + i)) & 1;

            if (i < 3)
                cfg.dataBlock[i] = {c1, c2, c3};
            else
                cfg.trailer = {c1, c2, c3};
        }
        return cfg;
    }

    // Encode SectorAccessConfig → 4-byte access bits (with inverted copies)
    inline ACCESSBYTES encode(const SectorAccessConfig& cfg) {
        BYTE b6 = 0, b7 = 0, b8 = 0;

        auto set = [](const AccessCondition& ac, int i,
                      BYTE& out6, BYTE& out7, BYTE& out8)
        {
            if (ac.c1) { out7 |= (1 << (4 + i)); }
            else       { out6 |= (1 << i); }         // ~C1

            if (ac.c2) { out8 |= (1 << i); }
            else       { out6 |= (1 << (4 + i)); }   // ~C2

            if (ac.c3) { out8 |= (1 << (4 + i)); }
            else       { out7 |= (1 << i); }         // ~C3
        };

        for (int i = 0; i < 3; ++i)
            set(cfg.dataBlock[i], i, b6, b7, b8);
        set(cfg.trailer, 3, b6, b7, b8);

        return {b6, b7, b8, cfg.gpb};
    }

    // Verify inverted copies match normal copies
    inline bool verify(const ACCESSBYTES& ab) {
        BYTE b6 = ab[0], b7 = ab[1], b8 = ab[2];

        for (int i = 0; i < 4; ++i) {
            bool c1      = (b7 >> (4 + i)) & 1;
            bool c1_inv  = (b6 >> i)       & 1;
            if (c1 == c1_inv) return false;   // should be inverted

            bool c2      = (b8 >> i)       & 1;
            bool c2_inv  = (b6 >> (4 + i)) & 1;
            if (c2 == c2_inv) return false;

            bool c3      = (b8 >> (4 + i)) & 1;
            bool c3_inv  = (b7 >> i)       & 1;
            if (c3 == c3_inv) return false;
        }
        return true;
    }

} // namespace AccessBitsCodec

// ════════════════════════════════════════════════════════════════════════════════
// Predefined Sector Modes
// ════════════════════════════════════════════════════════════════════════════════

enum class SectorMode {
    TRANSPORT,          // data=000, trailer=001 — full access, A manages all
    READ_ONLY_AB,       // data=010, trailer=010 — read-only, config locked
    READ_AB_WRITE_B,    // data=100, trailer=100 — A|B read, B write, config locked
    VALUE_BLOCK,        // data=110, trailer=001 — A|B read, B inc, A|B dec
    KEY_B_ONLY,         // data=011, trailer=011 — only B can access data
    FROZEN              // data=111, trailer=110 — no data access, config frozen
};

inline SectorAccessConfig sectorModeToConfig(SectorMode mode) {
    SectorAccessConfig cfg;
    AccessCondition d, t;

    switch (mode) {
        case SectorMode::TRANSPORT:
            d = {false,false,false}; t = {false,false,true};   break;
        case SectorMode::READ_ONLY_AB:
            d = {false,true, false}; t = {false,true, false};  break;
        case SectorMode::READ_AB_WRITE_B:
            d = {true, false,false}; t = {true, false,false};  break;
        case SectorMode::VALUE_BLOCK:
            d = {true, true, false}; t = {false,false,true};   break;
        case SectorMode::KEY_B_ONLY:
            d = {false,true, true};  t = {false,true, true};   break;
        case SectorMode::FROZEN:
            d = {true, true, true};  t = {true, true, false};  break;
    }

    cfg.dataBlock[0] = cfg.dataBlock[1] = cfg.dataBlock[2] = d;
    cfg.trailer = t;
    cfg.gpb = 0x69;
    return cfg;
}

// ════════════════════════════════════════════════════════════════════════════════
// TrailerConfig — Bir sektörün tam trailer bloğu
// ════════════════════════════════════════════════════════════════════════════════

struct TrailerConfig {
    KEYBYTES keyA = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
    SectorAccessConfig access;
    KEYBYTES keyB = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

    // ── Parse from 16-byte block ────────────────────────────────────────────
    static TrailerConfig fromBlock(const MifareBlock& block) {
        TrailerConfig tc;
        std::memcpy(tc.keyA.data(), block.trailer.keyA, 6);
        ACCESSBYTES ab;
        std::memcpy(ab.data(), block.trailer.accessBits, 4);
        tc.access = AccessBitsCodec::decode(ab);
        std::memcpy(tc.keyB.data(), block.trailer.keyB, 6);
        return tc;
    }

    // ── Serialize to 16-byte block ──────────────────────────────────────────
    MifareBlock toBlock() const {
        MifareBlock blk;
        std::memcpy(blk.trailer.keyA, keyA.data(), 6);
        ACCESSBYTES ab = AccessBitsCodec::encode(access);
        std::memcpy(blk.trailer.accessBits, ab.data(), 4);
        std::memcpy(blk.trailer.keyB, keyB.data(), 6);
        return blk;
    }

    // ── Factory default ─────────────────────────────────────────────────────
    static TrailerConfig factoryDefault() {
        TrailerConfig tc;
        tc.keyA = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
        tc.access = SectorAccessConfig::factoryDefault();
        tc.keyB = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
        return tc;
    }

    // ── Access bits raw bytes ───────────────────────────────────────────────
    ACCESSBYTES accessBitsRaw() const {
        return AccessBitsCodec::encode(access);
    }

    // ── Geçerlilik kontrolü ─────────────────────────────────────────────────
    bool isValid() const {
        ACCESSBYTES ab = AccessBitsCodec::encode(access);
        return AccessBitsCodec::verify(ab);
    }
};

#endif // TRAILERCONFIG_H
