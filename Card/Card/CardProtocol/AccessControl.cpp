#include "AccessControl.h"
#include "../CardModel/CardMemoryLayout.h"
#include "../CardModel/TrailerConfig.h"
#include <sstream>
#include <iomanip>
#include <cstring>

// ════════════════════════════════════════════════════════════════════════════════
// Construction
// ════════════════════════════════════════════════════════════════════════════════

AccessControl::AccessControl(CardMemoryLayout& cardMemory)
    : cardMemory_(cardMemory) {
}

// ════════════════════════════════════════════════════════════════════════════════
// Internal — sektör access config al
// ════════════════════════════════════════════════════════════════════════════════

static SectorAccessConfig getSectorConfig(const ACCESSBYTES& bits) {
    return AccessBitsCodec::decode(bits);
}

// Blok'un sektör içi index'i (0,1,2 = data, 3 = trailer)
static int blockInSector(int block, bool is4K) {
    int sector;
    if (!is4K) {
        sector = block / 4;
        return block - sector * 4;
    }
    if (block < 128) {
        sector = block / 4;
        return block - sector * 4;
    }
    // 4K extended: 16 blocks per sector
    sector = 32 + (block - 128) / 16;
    int first = 128 + (sector - 32) * 16;
    return block - first;
}

static int sectorOf(int block, bool is4K) {
    if (!is4K) return block / 4;
    if (block < 128) return block / 4;
    return 32 + (block - 128) / 16;
}

// ════════════════════════════════════════════════════════════════════════════════
// Permission Queries — Gerçek access bits decode
// ════════════════════════════════════════════════════════════════════════════════

bool AccessControl::canReadDataBlock(int sector, KeyType kt) const {
    ACCESSBYTES bits = getAccessBits(sector);
    SectorAccessConfig cfg = getSectorConfig(bits);
    // Tüm data blokları aynı izinde mi diye bakıyoruz, block 0 kullanıyoruz
    DataBlockPermission perm = cfg.dataPermission(0);
    return perm.canRead(kt);
}

bool AccessControl::canWriteDataBlock(int sector, KeyType kt) const {
    ACCESSBYTES bits = getAccessBits(sector);
    SectorAccessConfig cfg = getSectorConfig(bits);
    DataBlockPermission perm = cfg.dataPermission(0);
    return perm.canWrite(kt);
}

bool AccessControl::canReadTrailer(int sector, KeyType kt) const {
    ACCESSBYTES bits = getAccessBits(sector);
    SectorAccessConfig cfg = getSectorConfig(bits);
    TrailerPermission perm = cfg.trailerPermission();
    return perm.canRead(kt);
}

bool AccessControl::canWriteTrailer(int sector, KeyType kt) const {
    ACCESSBYTES bits = getAccessBits(sector);
    SectorAccessConfig cfg = getSectorConfig(bits);
    TrailerPermission perm = cfg.trailerPermission();
    return perm.canWrite(kt);
}

// ════════════════════════════════════════════════════════════════════════════════
// Authorization Checks — Blok seviyesi (data veya trailer)
// ════════════════════════════════════════════════════════════════════════════════

bool AccessControl::canRead(int block, KeyType kt) const {
    int sector = sectorOf(block, cardMemory_.is4K());
    int idxInSec = blockInSector(block, cardMemory_.is4K());
    ACCESSBYTES bits = getAccessBits(sector);
    SectorAccessConfig cfg = getSectorConfig(bits);

    // Trailer blok: sektördeki son blok
    int bps = cardMemory_.is4K() ? (sector < 32 ? 4 : 16) : 4;
    if (idxInSec == bps - 1) {
        return cfg.trailerPermission().canRead(kt);
    }
    // Data blok (4K extended: index 0-14 hepsi aynı access → min(idx,2) kullan)
    int dataIdx = (idxInSec < 3) ? idxInSec : 2;
    return cfg.dataPermission(dataIdx).canRead(kt);
}

bool AccessControl::canWrite(int block, KeyType kt) const {
    int sector = sectorOf(block, cardMemory_.is4K());
    int idxInSec = blockInSector(block, cardMemory_.is4K());
    ACCESSBYTES bits = getAccessBits(sector);
    SectorAccessConfig cfg = getSectorConfig(bits);

    int bps = cardMemory_.is4K() ? (sector < 32 ? 4 : 16) : 4;
    if (idxInSec == bps - 1) {
        return cfg.trailerPermission().canWrite(kt);
    }
    int dataIdx = (idxInSec < 3) ? idxInSec : 2;
    return cfg.dataPermission(dataIdx).canWrite(kt);
}

// ════════════════════════════════════════════════════════════════════════════════
// Permission Setting — Access bits'i memory'de güncelle
// ════════════════════════════════════════════════════════════════════════════════

void AccessControl::setDataBlockPermissions(int sector,
                                            bool keyARead, bool keyAWrite,
                                            bool keyBRead, bool keyBWrite) {
    // Uygun C1C2C3 kombinasyonunu bul
    // Basitleştirilmiş: en yakın modu seç
    AccessCondition ac = {false, false, false};
    if (keyARead && keyBRead && keyAWrite && keyBWrite)
        ac = {false, false, false};   // 000: full access
    else if (keyARead && keyBRead && !keyAWrite && keyBWrite)
        ac = {true, false, false};    // 100: A|B read, B write
    else if (keyARead && keyBRead && !keyAWrite && !keyBWrite)
        ac = {false, true, false};    // 010: read-only
    else if (!keyARead && keyBRead && !keyAWrite && keyBWrite)
        ac = {false, true, true};     // 011: B only
    else if (!keyARead && !keyBRead && !keyAWrite && !keyBWrite)
        ac = {true, true, true};      // 111: frozen

    ACCESSBYTES bits = getAccessBits(sector);
    SectorAccessConfig cfg = getSectorConfig(bits);
    cfg.dataBlock[0] = cfg.dataBlock[1] = cfg.dataBlock[2] = ac;
    writeAccessBits(sector, AccessBitsCodec::encode(cfg));
}

void AccessControl::setTrailerPermissions(int sector,
                                          bool keyARead, bool keyAWrite,
                                          bool keyBRead, bool keyBWrite) {
    ACCESSBYTES bits = getAccessBits(sector);
    SectorAccessConfig cfg = getSectorConfig(bits);
    // Simplified: trailer C1C2C3 based on write capability
    if (keyAWrite && !keyBWrite)
        cfg.trailer = {false, false, true};   // 001: A manages all
    else if (!keyAWrite && keyBWrite)
        cfg.trailer = {false, true, true};    // 011: B manages
    else if (!keyAWrite && !keyBWrite)
        cfg.trailer = {true, true, false};    // 110: frozen
    else
        cfg.trailer = {false, false, true};   // 001: default
    writeAccessBits(sector, AccessBitsCodec::encode(cfg));
}

void AccessControl::applySectorMode(int sector, StandardMode mode) {
    SectorMode sm;
    switch (mode) {
        case StandardMode::MODE_0: sm = SectorMode::READ_AB_WRITE_B; break;
        case StandardMode::MODE_1: sm = SectorMode::READ_ONLY_AB;    break;
        case StandardMode::MODE_2: sm = SectorMode::TRANSPORT;        break;
        case StandardMode::MODE_3: sm = SectorMode::READ_AB_WRITE_B; break;
        default:                   sm = SectorMode::TRANSPORT;        break;
    }
    SectorAccessConfig cfg = sectorModeToConfig(sm);
    writeAccessBits(sector, AccessBitsCodec::encode(cfg));
}

// ════════════════════════════════════════════════════════════════════════════════
// Introspection
// ════════════════════════════════════════════════════════════════════════════════

ACCESSBYTES AccessControl::getAccessBits(int sector) const {
    MifareBlock trailer = getTrailer(sector);
    ACCESSBYTES bits;
    std::memcpy(bits.data(), trailer.raw + 6, 4);
    return bits;
}

std::string AccessControl::describePermissions(int sector) const {
    ACCESSBYTES bits = getAccessBits(sector);
    SectorAccessConfig cfg = getSectorConfig(bits);
    std::ostringstream oss;

    oss << "Sector " << sector << " [";
    for (auto b : bits) oss << std::hex << std::setfill('0') << std::setw(2) << (int)b << " ";
    oss << "]" << std::dec;

    if (AccessBitsCodec::verify(bits))
        oss << " (valid)";
    else
        oss << " (CORRUPT)";

    auto desc = [](const AccessCondition& ac) {
        return std::to_string(ac.c1) + std::to_string(ac.c2) + std::to_string(ac.c3);
    };

    oss << "\n  Data 0: C=" << desc(cfg.dataBlock[0]);
    DataBlockPermission dp0 = cfg.dataPermission(0);
    oss << " Read:" << (dp0.readA?"A":"") << (dp0.readB?"|B":"")
        << " Write:" << (dp0.writeA?"A":"") << (dp0.writeB?"|B":"");

    oss << "\n  Data 1: C=" << desc(cfg.dataBlock[1]);
    oss << "\n  Data 2: C=" << desc(cfg.dataBlock[2]);

    oss << "\n  Trailer: C=" << desc(cfg.trailer);
    TrailerPermission tp = cfg.trailerPermission();
    oss << " AccR:" << (tp.accReadA?"A":"") << (tp.accReadB?"|B":"")
        << " AccW:" << (tp.accWriteA?"A":"") << (tp.accWriteB?"|B":"");

    return oss.str();
}

// ════════════════════════════════════════════════════════════════════════════════
// Internal Helpers
// ════════════════════════════════════════════════════════════════════════════════

MifareBlock AccessControl::getTrailer(int sector) const {
    if (cardMemory_.is4K()) {
        if (sector < 32) {
            return cardMemory_.data.card4K.detailed.normalSector[sector].trailerBlock;
        } else {
            return cardMemory_.data.card4K.detailed.extendedSector[sector - 32].trailerBlock;
        }
    } else {
        return cardMemory_.data.card1K.detailed.sector[sector].trailerBlock;
    }
}

bool AccessControl::checkPermissionBit(const ACCESSBYTES& bits, int blockIndex,
                                       KeyType kt, bool isWrite) const {
    SectorAccessConfig cfg = getSectorConfig(bits);
    if (blockIndex == 3) {
        TrailerPermission tp = cfg.trailerPermission();
        return isWrite ? tp.canWrite(kt) : tp.canRead(kt);
    }
    int idx = (blockIndex < 3) ? blockIndex : 2;
    DataBlockPermission dp = cfg.dataPermission(idx);
    return isWrite ? dp.canWrite(kt) : dp.canRead(kt);
}

ACCESSBYTES AccessControl::encodeAccessBits(bool dataReadA, bool dataWriteA,
                                            bool dataReadB, bool dataWriteB,
                                            bool trailerReadA, bool trailerWriteA,
                                            bool trailerReadB, bool trailerWriteB) const {
    // Artık TrailerConfig/AccessBitsCodec kullanıyoruz
    SectorAccessConfig cfg;
    // Data blocks
    AccessCondition dac = {false, false, false};
    if (dataReadA && dataReadB && dataWriteA && dataWriteB)
        dac = {false, false, false};
    else if (dataReadA && dataReadB && !dataWriteA && !dataWriteB)
        dac = {false, true, false};
    else if (dataReadA && dataReadB && !dataWriteA && dataWriteB)
        dac = {true, false, false};
    cfg.dataBlock[0] = cfg.dataBlock[1] = cfg.dataBlock[2] = dac;
    cfg.trailer = {false, false, true}; // default trailer mode
    return AccessBitsCodec::encode(cfg);
}

void AccessControl::writeAccessBits(int sector, const ACCESSBYTES& bits) {
    MifareBlock* trailer;
    if (cardMemory_.is4K()) {
        if (sector < 32)
            trailer = &cardMemory_.data.card4K.detailed.normalSector[sector].trailerBlock;
        else
            trailer = &cardMemory_.data.card4K.detailed.extendedSector[sector - 32].trailerBlock;
    } else {
        trailer = &cardMemory_.data.card1K.detailed.sector[sector].trailerBlock;
    }
    std::memcpy(trailer->trailer.accessBits, bits.data(), 4);
}
