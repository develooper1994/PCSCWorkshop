#ifndef CARDIO_H
#define CARDIO_H

#include "CardInterface.h"
#include "CardModel/TrailerConfig.h"
#include "Reader.h"
#include <vector>

// ════════════════════════════════════════════════════════════════════════════════
// CardIO — Gerçek PCSC Kart I/O
// ════════════════════════════════════════════════════════════════════════════════
//
// CardInterface (in-memory model) ile Reader (PCSC APDU) arasinda köprü.
// Okuma/yazma/auth islemlerini otomatik yönetir, her islemde hem karta hem
// memory modeline yansitir.
//
// ─── Mimari ────────────────────────────────────────────────────────────────
//
//   CardIO
//   ├── Reader&          PCSC APDU (loadKey, auth, readPage, writePage)
//   └── CardInterface    In-memory model (topology, key, auth, access)
//
// ─── Temel Kullanim ────────────────────────────────────────────────────────
//
//   ACR1281UReader reader(conn, 16, false, ...);
//   CardIO io(reader, false);                // 1K kart
//
//   // Key ayarla (opsiyonel — default FFFFFFFFFFFF):
//   KEYBYTES myKey = {0xA0,0xA1,0xA2,0xA3,0xA4,0xA5};
//   io.setDefaultKey(myKey, KeyStructure::NonVolatile, 0x01, KeyType::A);
//
//   // Tüm karti oku:
//   int ok = io.readCard();                  // 60/64 blok (sektör 1 bozuksa)
//
//   // Memory model üzerinden sorgula:
//   auto uid = io.card().getUID();
//   int sector = io.card().getSectorForBlock(9);
//
//   // Tek blok oku/yaz (auto-auth):
//   BYTEV data = io.readBlock(8);
//   BYTE payload[16] = {'H','E','L','L','O',0};
//   io.writeBlock(8, payload);               // kart + memory güncellenir
//
//   // Tek sektör oku:
//   io.readSector(2);
//
// ─── Trailer Islemleri ─────────────────────────────────────────────────────
//
//   // Trailer oku — KeyA, access bits, KeyB parse edilir:
//   TrailerConfig tc = io.readTrailer(2);
//   KEYBYTES keyB = tc.keyB;
//   bool valid = tc.isValid();               // bütünlük kontrolü
//
//   // Access config sorgula:
//   SectorAccessConfig cfg = io.getAccessConfig(2);
//   DataBlockPermission dp = cfg.dataPermission(0);
//   // dp.readA, dp.writeB, dp.incrementA ...
//
//   // Kisa yol — permission sorgula:
//   DataBlockPermission dp2 = io.getDataPermission(2, 0);
//   TrailerPermission   tp  = io.getTrailerPermission(2);
//
//   // Hazir mod uygula (karta yazar!):
//   io.setSectorMode(5, SectorMode::READ_AB_WRITE_B);
//
//   // Trailer yaz (DİKKAT — yanlis bits karti kilitler!):
//   TrailerConfig newTc;
//   newTc.keyA   = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
//   newTc.keyB   = {0xA0,0xA1,0xA2,0xA3,0xA4,0xA5};
//   newTc.access = sectorModeToConfig(SectorMode::READ_AB_WRITE_B);
//   io.writeTrailer(5, newTc);
//
// ─── Bulk Trailer Yedekleme ────────────────────────────────────────────────
//
//   auto backup = io.saveAllTrailers();      // 16 sektör yedekle
//   // ... degisiklikler ...
//   io.restoreAllTrailers(backup);           // geri yükle
//
// ─── Alt Model Erisimi ─────────────────────────────────────────────────────
//
//   CardInterface& card = io.card();         // in-memory model
//   Reader& rdr = io.reader();               // alt PCSC reader
//
//   const CardMemoryLayout& mem = io.card().getMemory();
//   BYTE uid0 = mem.data.card1K.blocks[0].manufacturer.uid[0];
//
// ════════════════════════════════════════════════════════════════════════════════

class CardIO {
public:
    // ────────────────────────────────────────────────────────────────────────────
    // Construction
    // ────────────────────────────────────────────────────────────────────────────

    explicit CardIO(Reader& reader, bool is4K = false);

    // ────────────────────────────────────────────────────────────────────────────
    // Key Ayarları  (auth öncesi çağrılmalı)
    // ────────────────────────────────────────────────────────────────────────────

    // Default key ayarla — readCard / readSector bu key ile auth yapar
    void setDefaultKey(const KEYBYTES& key,
                       KeyStructure ks   = KeyStructure::NonVolatile,
                       BYTE slot         = 0x01,
                       KeyType kt        = KeyType::A);

    // İki key ayarla (A ve B)
    void setKeys(const KEYBYTES& keyA, BYTE slotA,
                 const KEYBYTES& keyB, BYTE slotB,
                 KeyStructure ks = KeyStructure::NonVolatile);

    // ────────────────────────────────────────────────────────────────────────────
    // Kart Okuma
    // ────────────────────────────────────────────────────────────────────────────

    // Tüm kartı oku → CardInterface memory'sine yükle
    // @return başarılı okunan blok sayısı
    int readCard();

    // Tek sektörü oku → memory'ye yükle
    // @return true: tüm bloklar okundu
    bool readSector(int sector);

    // Tek blok oku (gerekirse auth yapar)
    // Memory'yi de günceller, BYTEV olarak döndürür
    BYTEV readBlock(int block);

    // ────────────────────────────────────────────────────────────────────────────
    // Kart Yazma
    // ────────────────────────────────────────────────────────────────────────────

    // Tek blok yaz (gerekirse auth yapar)
    // Hem karta yazar hem memory'yi günceller
    void writeBlock(int block, const BYTE data[16]);
    void writeBlock(int block, const BYTEV& data);

    // ────────────────────────────────────────────────────────────────────────────
    // Auth (manuel)
    // ────────────────────────────────────────────────────────────────────────────

    // Belirli sektör için loadKey + auth
    void authenticate(int sector);
    void authenticate(int sector, KeyType kt, BYTE slot);

    // ────────────────────────────────────────────────────────────────────────────
    // Trailer Okuma / Yazma
    // ────────────────────────────────────────────────────────────────────────────

    // Sektörün trailer'ını karttan oku → TrailerConfig olarak döndür
    TrailerConfig readTrailer(int sector);

    // Trailer'ı karta yaz (KeyA + access bits + KeyB)
    // DİKKAT: Yanlış access bits kartı kalıcı olarak kilitleyebilir!
    void writeTrailer(int sector, const TrailerConfig& config);

    // ────────────────────────────────────────────────────────────────────────────
    // Access Bits Konfigürasyonu
    // ────────────────────────────────────────────────────────────────────────────

    // Sektörün mevcut access config'ini al (memory'den)
    SectorAccessConfig getAccessConfig(int sector) const;

    // Sektöre yeni access config uygula (memory + karta yaz)
    void setAccessConfig(int sector, const SectorAccessConfig& config);

    // Hazır mod uygula (memory + karta yaz)
    void setSectorMode(int sector, SectorMode mode);

    // Permission sorgula (decode edilmiş)
    DataBlockPermission getDataPermission(int sector, int blockIndex = 0) const;
    TrailerPermission   getTrailerPermission(int sector) const;

    // ────────────────────────────────────────────────────────────────────────────
    // Bulk Trailer İşlemleri
    // ────────────────────────────────────────────────────────────────────────────

    // Tüm sektörlerin trailer config'ini kaydet (backup)
    std::vector<TrailerConfig> saveAllTrailers();

    // Kaydedilmiş config'leri karta geri yaz (restore)
    void restoreAllTrailers(const std::vector<TrailerConfig>& configs);

    // ────────────────────────────────────────────────────────────────────────────
    // Erişim
    // ────────────────────────────────────────────────────────────────────────────

    CardInterface&       card();
    const CardInterface& card() const;
    Reader&              reader();

private:
    Reader&        reader_;
    CardInterface  card_;

    // Default key bilgisi
    KEYBYTES       defaultKey_  = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
    KeyStructure   defaultKS_   = KeyStructure::NonVolatile;
    BYTE           defaultSlot_ = 0x01;
    KeyType        defaultKT_   = KeyType::A;

    int            lastAuthSector_ = -1;

    // Auth gerekirse yap
    void ensureAuth(int sector);
};

#endif // CARDIO_H
