#ifndef CARDINTERFACE_H
#define CARDINTERFACE_H

#include "CardModel/CardDataTypes.h"
#include <memory>
#include <vector>

// Forward declares (definitions in CardInterface.cpp)
struct CardMemoryLayout;
class CardLayoutTopology;
class AccessControl;
class KeyManagement;
class AuthenticationState;
struct MifareBlock;

// ════════════════════════════════════════════════════════════════════════════════
// CardInterface — Mifare Classic Kart Modeli (In-Memory)
// ════════════════════════════════════════════════════════════════════════════════
//
// Kart bellegi, topology, key yönetimi, auth durumu ve erisim kontrolünü
// tek bir arayüzde birlestiren sinif. I/O yapmaz — saf bellek modelidir.
// Gerçek kart I/O için CardIO sinifini kullanin.
//
// ─── Mimari ────────────────────────────────────────────────────────────────
//
//   CardInterface
//   ├── CardMemoryLayout      raw bellek (union: 1K veya 4K)
//   ├── CardLayoutTopology    blok/sektör hesaplamalari
//   ├── AccessControl         trailer'dan izin matrisi decode
//   ├── KeyManagement         key kayit/sorgulama
//   └── AuthenticationState   auth oturum takibi
//
// ─── Kullanim Örnekleri ────────────────────────────────────────────────────
//
//   // 1) Olustur ve bellek yükle:
//   CardInterface card(false);                  // 1K kart
//   card.loadMemory(rawBytes, 1024);            // PCSC'den okunan 1024 byte
//
//   // 2) Kart bilgisi:
//   KEYBYTES uid  = card.getUID();              // manufacturer blogundan UID
//   bool     is1k = card.is1K();                // true
//   int      blks = card.getTotalBlocks();      // 64
//
//   // 3) Topology sorgulari:
//   int sector  = card.getSectorForBlock(9);    // 2
//   int trailer = card.getTrailerBlockOfSector(2); // 11
//   bool isMfg  = card.isManufacturerBlock(0);  // true
//   bool isTrl  = card.isTrailerBlock(3);        // true
//
//   // 4) Bellek erisimi (zero-copy union view):
//   const CardMemoryLayout& mem = card.getMemory();
//   BYTE uid0 = mem.data.card1K.blocks[0].manufacturer.uid[0];
//   BYTE keyB = mem.data.card1K.blocks[3].trailer.keyB[2];
//
//   // 5) Blok erisimi:
//   const MifareBlock& blk = card.getBlock(8);
//   BYTE firstByte = blk.raw[0];
//
//   // 6) Key yönetimi:
//   KEYBYTES myKey = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
//   card.registerKey(KeyType::A, myKey, KeyStructure::NonVolatile, 0x01, "DefA");
//   KEYBYTES k = card.getKey(KeyType::A, 0x01);
//   KEYBYTES cardKeyA = card.getCardKey(0, KeyType::A); // trailer'dan oku
//
//   // 7) Auth simulasyonu:
//   card.authenticate(5, KeyType::A);
//   bool ok = card.isAuthenticated(5);          // true
//   card.deauthenticate(5);
//   card.clearAuthentication();                 // tüm sektörler
//
//   // 8) Erisim kontrolü (trailer access bits'e göre):
//   bool r = card.canRead(8, KeyType::A);       // data blok
//   bool w = card.canWrite(8, KeyType::B);      // data blok
//   bool dr = card.canReadDataBlocks(2, KeyType::A);  // sektör bazli
//
//   // 9) Bellegi geri al:
//   BYTEV exported = card.exportMemory();        // 1024 byte kopyasi
//
// ════════════════════════════════════════════════════════════════════════════════

class CardInterface {
public:
    // ────────────────────────────────────────────────────────────────────────────
    // Constructor
    // ────────────────────────────────────────────────────────────────────────────

    // Create interface for 1K (is4K=false) or 4K (is4K=true)
    explicit CardInterface(bool is4K = false);

    // Destructor (defined in cpp — required for unique_ptr forward declares)
    ~CardInterface();

    // ────────────────────────────────────────────────────────────────────────────
    // Memory Management
    // ────────────────────────────────────────────────────────────────────────────

    // Load card memory from raw bytes
    void loadMemory(const BYTE* data, size_t size);

    // Get reference to memory layout
    const CardMemoryLayout& getMemory() const;
    CardMemoryLayout& getMemoryMutable();

    // Export memory to raw bytes
    BYTEV exportMemory() const;

    // ────────────────────────────────────────────────────────────────────────────
    // Key Management
    // ────────────────────────────────────────────────────────────────────────────

    // Register a key
    void registerKey(KeyType kt, const KEYBYTES& key, KeyStructure structure,
                    BYTE slot = 0x01, const std::string& name = "");

    // Get registered key
    const KEYBYTES& getKey(KeyType kt, BYTE slot = 0x01) const;

    // Get card's trailer key
    KEYBYTES getCardKey(int sector, KeyType kt) const;

    // ────────────────────────────────────────────────────────────────────────────
    // Authentication
    // ────────────────────────────────────────────────────────────────────────────

    // Simulate authentication (mark sector as authenticated)
    void authenticate(int sector, KeyType kt);

    // Check if sector is authenticated
    bool isAuthenticated(int sector) const;

    // Check if sector is authenticated with specific key
    bool isAuthenticatedWith(int sector, KeyType kt) const;

    // Deauthenticate sector
    void deauthenticate(int sector);

    // Clear all authentication
    void clearAuthentication();

    // ────────────────────────────────────────────────────────────────────────────
    // Access Control
    // ────────────────────────────────────────────────────────────────────────────

    // Check if can perform operation on block
    bool canRead(int block, KeyType kt) const;
    bool canWrite(int block, KeyType kt) const;

    // Check if can perform operation on data blocks in sector
    bool canReadDataBlocks(int sector, KeyType kt) const;
    bool canWriteDataBlocks(int sector, KeyType kt) const;

    // ────────────────────────────────────────────────────────────────────────────
    // Topology Queries
    // ────────────────────────────────────────────────────────────────────────────

    // Get block information
    MifareBlock& getBlock(int blockNum);
    const MifareBlock& getBlock(int blockNum) const;

    // Get sector information
    int getSectorForBlock(int block) const;
    int getFirstBlockOfSector(int sector) const;
    int getLastBlockOfSector(int sector) const;
    int getTrailerBlockOfSector(int sector) const;

    // Check block type
    bool isManufacturerBlock(int block) const;
    bool isTrailerBlock(int block) const;
    bool isDataBlock(int block) const;

    // ────────────────────────────────────────────────────────────────────────────
    // Card Metadata
    // ────────────────────────────────────────────────────────────────────────────

    // Get card type
    bool is4K() const;
    bool is1K() const;

    // Get card size info
    size_t getTotalMemory() const;
    int getTotalBlocks() const;
    int getTotalSectors() const;

    // ────────────────────────────────────────────────────────────────────────────
    // Utility / Introspection
    // ────────────────────────────────────────────────────────────────────────────

    // Print complete card information
    void printCardInfo() const;

    // Print authentication status
    void printAuthenticationStatus() const;

    // Get UID from manufacturer block
    KEYBYTES getUID() const;

private:
    // Components
    std::unique_ptr<CardMemoryLayout> memory_;
    std::unique_ptr<CardLayoutTopology> topology_;
    std::unique_ptr<AccessControl> accessControl_;
    std::unique_ptr<KeyManagement> keyMgmt_;
    std::unique_ptr<AuthenticationState> authState_;

    bool is4K_;
};

#endif // CARDINTERFACE_H
