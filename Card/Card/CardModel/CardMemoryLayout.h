#ifndef CARDMEMORYLAYOUT_H
#define CARDMEMORYLAYOUT_H

#include "../CardDataTypes.h"
#include "BlockDefinition.h"
#include "PageDefinition.h"
#include "Result.h"
#include <array>
#include <cstring>

// ════════════════════════════════════════════════════════════════════════════════
// CardMemoryLayout - Zero-Copy Union for Complete Card Memory
// ════════════════════════════════════════════════════════════════════════════════
//
// Single memory region with multiple interpretations:
// 1. Raw bytes (byte-level access)
// 2. Blocks (block-level access, 16-byte chunks)
// 3. Sectors (sector-level access with grouped blocks)
//
// All views share same memory - zero-copy interpretation.
//
// Example (1K):
// ┌─ Raw View ─────────────────────────┐
// │ [0...1023] BYTE raw[1024];         │
// ├─ Block View ───────────────────────┤
// │ blocks[0]  = raw[0...15]           │
// │ blocks[1]  = raw[16...31]          │
// │ ...                                 │
// │ blocks[63] = raw[1008...1023]      │
// ├─ Sector View ──────────────────────┤
// │ sectors[0] = blocks[0...3]         │
// │ sectors[1] = blocks[4...7]         │
// │ ...                                 │
// │ sectors[15] = blocks[60...63]      │
// └────────────────────────────────────┘
//
// ════════════════════════════════════════════════════════════════════════════════

// ────────────────────────────────────────────────────────────────────────────
// 1K Card Memory Layout
// ────────────────────────────────────────────────────────────────────────────
// Tasarım Notu:
// 1K ve 4K yalnızca toplam sektör sayısıyla değil, sektör içi blok organizasyonu
// ile de farklıdır.
// - 1K: tüm sektörler 4 blok
// - 4K: sektör 0-31 -> 4 blok, sektör 32-39 -> 16 blok
// Bu nedenle 4K için karma (normal+extended) sektör görünümü gerekir.
// ────────────────────────────────────────────────────────────────────────────

struct Card1KMemoryLayout {
    union {
        // View 1: Raw bytes (complete memory)
        BYTE raw[1024];

        // View 2: Blocks (64 blocks × 16 bytes each)
        MifareBlock blocks[64];

        // View 3: Sectors (16 sectors × 4 blocks each)
        struct {
            MifareBlock block[4];  // 4 blocks per sector
        } sectors[16];

        // View 4: Detailed sector structure
        struct {
            struct {
                MifareBlock dataBlock0;      // Block 0 (data)
                MifareBlock dataBlock1;      // Block 1 (data)
                MifareBlock dataBlock2;      // Block 2 (data)
                MifareBlock trailerBlock;    // Block 3 (trailer: keys + access bits)
            } sector[16];
        } detailed;
    };

    // Constructors
    Card1KMemoryLayout() {
        std::memset(raw, 0, 1024);
    }

    Card1KMemoryLayout(const BYTE* dataPtr) {
        std::memcpy(raw, dataPtr, 1024);
    }

    // Size info
    static constexpr size_t MEMORY_SIZE = 1024;
    static constexpr int TOTAL_BLOCKS = 64;
    static constexpr int TOTAL_SECTORS = 16;
    static constexpr int BLOCKS_PER_SECTOR = 4;
};

// ────────────────────────────────────────────────────────────────────────────
// 4K Card Memory Layout
// ────────────────────────────────────────────────────────────────────────────
// Tasarım Notu:
// 4K detay görünümünde extended sektörlerin normal sektörlerden sonra bellek içinde
// bitişik yerleşmesi kritik. Bu yüzden tek bir `detailed` altında
// `normalSector[32] + extendedSector[8]` kullanılır.
// Ayrı union üyeleri (ör. detailedNormal / detailedExtended) offset=0 çakışması
// yaratabileceğinden tercih edilmez.
// ────────────────────────────────────────────────────────────────────────────

struct Card4KMemoryLayout {
    union {
        // View 1: Raw bytes (complete memory)
        BYTE raw[4096];

        // View 2: Blocks (256 blocks × 16 bytes each)
        MifareBlock blocks[256];

        // View 3: Sectors (mixed: 32 normal + 8 extended)
        struct {
            struct {
                MifareBlock block[4];   // Normal: 4 blocks
            } normal[32];

            struct {
                MifareBlock block[16];  // Extended: 16 blocks
            } extended[8];
        } sectors;

        // View 4: Detailed sector structure (normal + extended, contiguous)
        struct {
            struct {
                MifareBlock dataBlock0;      // Block 0 (data)
                MifareBlock dataBlock1;      // Block 1 (data)
                MifareBlock dataBlock2;      // Block 2 (data)
                MifareBlock trailerBlock;    // Block 3 (trailer)
            } normalSector[32];              // Sectors 0-31:  offset 0-2047

            struct {
                MifareBlock dataBlocks[15];  // Blocks 0-14 (data)
                MifareBlock trailerBlock;    // Block 15 (trailer)
            } extendedSector[8];             // Sectors 32-39: offset 2048-4095
        } detailed;
    };

    // Constructors
    Card4KMemoryLayout() {
        std::memset(raw, 0, 4096);
    }

    Card4KMemoryLayout(const BYTE* dataPtr) {
        std::memcpy(raw, dataPtr, 4096);
    }

    // Size info
    static constexpr size_t MEMORY_SIZE = 4096;
    static constexpr int TOTAL_BLOCKS = 256;
    static constexpr int TOTAL_SECTORS = 40;
    static constexpr int NORMAL_SECTORS = 32;
    static constexpr int EXTENDED_SECTORS = 8;
};

// ────────────────────────────────────────────────────────────────────────────
// Ultralight Memory Layout
// ────────────────────────────────────────────────────────────────────────────
// Tasarım Notu:
// Ultralight 16 page × 4 byte = 64 byte düz bellektir.
// Ultralight READ komutu (0x30) her seferinde 4 ardışık page = 16 byte döndürür.
// Bu sayede Classic'teki MifareBlock (16B) Ultralight'ta da "virtual block"
// olarak kullanılır: blocks[0]=page 0-3, blocks[1]=page 4-7 …
// Sektör/trailer kavramı yoktur.
// ────────────────────────────────────────────────────────────────────────────

struct UltralightMemoryLayout {
    union {
        // View 1: Raw bytes
        BYTE raw[64];

        // View 2: Virtual blocks (4 × 16 bytes — APDU read granularity)
        MifareBlock blocks[4];

        // View 3: Pages (16 × 4 bytes — actual page granularity)
        UltralightPage pages[16];

        // View 4: Detailed page structure
        struct {
            UltralightPage serial0;      // page 0 : SN0-SN2 + BCC0
            UltralightPage serial1;      // page 1 : SN3-SN6
            UltralightPage lockPage;     // page 2 : BCC1 + internal + Lock0 + Lock1
            UltralightPage otpPage;      // page 3 : OTP
            UltralightPage userData[12]; // page 4-15 : user data (48 bytes)
        } detailed;
    };

    // Constructors
    UltralightMemoryLayout() {
        std::memset(raw, 0, 64);
    }

    UltralightMemoryLayout(const BYTE* dataPtr) {
        std::memcpy(raw, dataPtr, 64);
    }

    // Size info
    static constexpr size_t MEMORY_SIZE = 64;
    static constexpr int TOTAL_BLOCKS = 4;
    static constexpr int TOTAL_PAGES = 16;
    static constexpr int USER_DATA_PAGES = 12;
};

// ════════════════════════════════════════════════════════════════════════════════
// CardMemoryLayout - Type-Safe Wrapper
// ════════════════════════════════════════════════════════════════════════════════
// Tasarım Notu:
// Dış API tek tip `CardMemoryLayout` ile çalışır; kart türü runtime'da `cardType`
// ile belirlenir.
// - `getRawMemory()` ve `getBlock()` erişim noktalarıdır; kart türünden bağımsız.
// - Sınır kontrolü `getBlock()` içinde merkezi olarak uygulanır.
// - Topoloji hesapları (`sector->block`, trailer konumu vb.) CardTopology katmanında
//   tutulmalı; bellek modeli ham görünüm sağlamalıdır.
// - Ultralight'ta `getBlock(i)` → 4 ardışık page'i 16-byte virtual block olarak verir.
//
// Union-based memory layout for Classic 1K, Classic 4K, or Ultralight.
// Discriminant (cardType) determines which layout is active.
//
// ════════════════════════════════════════════════════════════════════════════════

struct CardMemoryLayout {
    union {
        Card1KMemoryLayout card1K;
        Card4KMemoryLayout card4K;
        UltralightMemoryLayout ultralight;
    } data = {};

    CardType cardType = CardType::MifareClassic1K;

    // ── Backward-compatible type queries ────────────────────────────────────

    bool is4K()        const { return cardType == CardType::MifareClassic4K; }
    bool is1K()        const { return cardType == CardType::MifareClassic1K; }
    bool isUltralight()const { return cardType == CardType::MifareUltralight; }
    bool isClassic()   const { return is1K() || is4K(); }
    bool isDesfire()   const { return cardType == CardType::MifareDesfire; }

    // ── Constructors ────────────────────────────────────────────────────────

    CardMemoryLayout() {
        std::memset(data.card1K.raw, 0, Card1KMemoryLayout::MEMORY_SIZE);
    }

    explicit CardMemoryLayout(CardType ct) : cardType(ct) {
        if (!isDesfire())
            std::memset(getRawMemory(), 0, memorySize());
    }

    // Backward-compatible: bool is4K → CardType
    explicit CardMemoryLayout(bool is4KCard)
        : CardMemoryLayout(is4KCard ? CardType::MifareClassic4K
                                    : CardType::MifareClassic1K) {}

    // ── Memory size ─────────────────────────────────────────────────────────

    size_t memorySize() const {
        switch (cardType) {
            case CardType::MifareClassic4K:  return Card4KMemoryLayout::MEMORY_SIZE;
            case CardType::MifareUltralight: return UltralightMemoryLayout::MEMORY_SIZE;
            case CardType::MifareDesfire:    return 0;  // lives in DesfireMemoryLayout
            default:                         return Card1KMemoryLayout::MEMORY_SIZE;
        }
    }

    int totalBlocks() const {
        switch (cardType) {
            case CardType::MifareClassic4K:  return Card4KMemoryLayout::TOTAL_BLOCKS;
            case CardType::MifareUltralight: return UltralightMemoryLayout::TOTAL_BLOCKS;
            case CardType::MifareDesfire:    return 0;  // virtual blocks via DesfireMemoryLayout
            default:                         return Card1KMemoryLayout::TOTAL_BLOCKS;
        }
    }

    // ── Raw memory access ───────────────────────────────────────────────────

    BYTE* getRawMemory() {
        switch (cardType) {
            case CardType::MifareClassic4K:  return data.card4K.raw;
            case CardType::MifareUltralight: return data.ultralight.raw;
            default:                         return data.card1K.raw;
        }
    }

    const BYTE* getRawMemory() const {
        switch (cardType) {
            case CardType::MifareClassic4K:  return data.card4K.raw;
            case CardType::MifareUltralight: return data.ultralight.raw;
            default:                         return data.card1K.raw;
        }
    }

    // ── Block access (transparent for all card types) ───────────────────────

    MifareBlock& getBlock(int index) {
        if (index < 0 || index >= totalBlocks()) {
			PcscError::make(CardError::InvalidData,
                "Block index out of range: " + std::to_string(index)).throwIfError();
            static MifareBlock dummy;
            return dummy;
        }
        switch (cardType) {
            case CardType::MifareClassic4K:  return data.card4K.blocks[index];
            case CardType::MifareUltralight: return data.ultralight.blocks[index];
            default:                         return data.card1K.blocks[index];
        }
    }

    const MifareBlock& getBlock(int index) const {
        if (index < 0 || index >= totalBlocks()) {
			PcscError::make(CardError::InvalidData,
                "Block index out of range: " + std::to_string(index)).throwIfError();
            static const MifareBlock dummy;
            return dummy;
        }
        switch (cardType) {
            case CardType::MifareClassic4K:  return data.card4K.blocks[index];
            case CardType::MifareUltralight: return data.ultralight.blocks[index];
            default:                         return data.card1K.blocks[index];
        }
    }
};

#endif // CARDMEMORYLAYOUT_H
