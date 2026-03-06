#ifndef ACCESSCONTROL_H
#define ACCESSCONTROL_H

#include "../CardModel/CardDataTypes.h"
#include "../CardModel/CardMemoryLayout.h"
#include <array>
#include <string>

// ════════════════════════════════════════════════════════════════════════════════
// AccessControl - Permission Matrix for Mifare Classic
// ════════════════════════════════════════════════════════════════════════════════
//
// Manages read/write permissions for sectors and blocks.
// Works directly with CardMemoryLayout - reads access bits from trailers.
//
// Permissions are stored in sector trailers (4 bytes):
// - Bytes 6-9: [C1 C2 C3_inv GPB]
// - Encodes: For each data block and trailer:
//   - KeyA readable?
//   - KeyA writable?
//   - KeyB readable?
//   - KeyB writable?
//
// This class:
// 1. Queries CardMemoryLayout trailers for access bits
// 2. Decodes permissions
// 3. Checks if operation is allowed
//
// ════════════════════════════════════════════════════════════════════════════════

class AccessControl {
public:
    // ────────────────────────────────────────────────────────────────────────────
    // Constructor
    // ────────────────────────────────────────────────────────────────────────────

    explicit AccessControl(const CardMemoryLayout& cardMemory);

    // ────────────────────────────────────────────────────────────────────────────
    // Permission Queries
    // ────────────────────────────────────────────────────────────────────────────

    // Can this key read/write a data block in this sector?
    bool canReadDataBlock(int sector, KeyType kt) const;
    bool canWriteDataBlock(int sector, KeyType kt) const;

    // Can this key read/write the trailer block of this sector?
    bool canReadTrailer(int sector, KeyType kt) const;
    bool canWriteTrailer(int sector, KeyType kt) const;

    // ────────────────────────────────────────────────────────────────────────────
    // Authorization Checks
    // ────────────────────────────────────────────────────────────────────────────

    // Is operation allowed?
    bool canRead(int block, KeyType kt) const;
    bool canWrite(int block, KeyType kt) const;

    // ────────────────────────────────────────────────────────────────────────────
    // Permission Setting (Modify access bits)
    // ────────────────────────────────────────────────────────────────────────────

    // Change permissions for data blocks
    void setDataBlockPermissions(int sector,
                                  bool keyARead, bool keyAWrite,
                                  bool keyBRead, bool keyBWrite);

    // Change permissions for trailer
    void setTrailerPermissions(int sector,
                               bool keyARead, bool keyAWrite,
                               bool keyBRead, bool keyBWrite);

    // ────────────────────────────────────────────────────────────────────────────
    // Standard Modes
    // ────────────────────────────────────────────────────────────────────────────

    // Apply standard access mode to sector
    enum class StandardMode {
        MODE_0,  // KeyA: R, KeyB: RW (default)
        MODE_1,  // KeyA: R, KeyB: R (locked)
        MODE_2,  // KeyA: RW, KeyB: RW (open)
        MODE_3   // KeyA: RW, KeyB: R (restricted)
    };

    void applySectorMode(int sector, StandardMode mode);

    // ────────────────────────────────────────────────────────────────────────────
    // Introspection
    // ────────────────────────────────────────────────────────────────────────────

    // Get access bits for sector
    ACCESSBYTES getAccessBits(int sector) const;

    // Get string description of permissions
    std::string describePermissions(int sector) const;

private:
    const CardMemoryLayout& cardMemory_;

    // ────────────────────────────────────────────────────────────────────────────
    // Internal Helpers
    // ────────────────────────────────────────────────────────────────────────────

    // Helper: get trailer block for sector
    MifareBlock getTrailer(int sector) const;

    // Helper: decode access bits and check permission
    bool checkPermissionBit(const ACCESSBYTES& bits, int blockIndex,
                           KeyType kt, bool isWrite) const;

    // Helper: encode permissions to access bits
    ACCESSBYTES encodeAccessBits(bool dataReadA, bool dataWriteA,
                                  bool dataReadB, bool dataWriteB,
                                  bool trailerReadA, bool trailerWriteA,
                                  bool trailerReadB, bool trailerWriteB) const;
};

#endif // ACCESSCONTROL_H
