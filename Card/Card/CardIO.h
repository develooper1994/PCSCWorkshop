#ifndef CARDIO_H
#define CARDIO_H

#include "CardInterface.h"
#include "Reader.h"

// ════════════════════════════════════════════════════════════════════════════════
// CardIO — Gerçek PCSC Kart I/O (MifareCardCore yerine)
// ════════════════════════════════════════════════════════════════════════════════
//
// CardInterface (in-memory model) ile Reader (PCSC APDU) arasında köprü.
//
//   CardIO io(reader);
//   io.readCard();                          // tüm kartı oku → memory'ye yükle
//   auto uid = io.card().getUID();          // memory'den UID
//   io.writeBlock(8, data);                 // karta yaz + memory güncelle
//   BYTEV rb = io.readBlock(8);             // karttan oku + memory güncelle
//
// Topology, key, auth, access sorguları hep io.card() üzerinden yapılır.
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
