#ifndef CARDIO_H
#define CARDIO_H

#include "CardInterface.h"
#include "Reader.h"
#include <vector>
#include <map>
#include <functional>
#include <memory>

// Forward declares (TrailerConfig.h types — only used in method signatures)
struct TrailerConfig;
struct SectorAccessConfig;
struct DataBlockPermission;
struct TrailerPermission;
enum class SectorMode;
struct DesfireAID;
struct DesfireSession;
struct DesfireVersionInfo;
struct DesfireFileSettings;
struct DesfireAccessRights;
enum class DesfireKeyType : BYTE;
enum class DesfireCommMode : BYTE;

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

    explicit CardIO(Reader& reader, CardType ct = CardType::MifareClassic1K);

    // Backward-compatible: bool is4K → CardType
    explicit CardIO(Reader& reader, bool is4K);

    ~CardIO();

    // ────────────────────────────────────────────────────────────────────────────
    // Key Ayarları  (auth öncesi çağrılmalı)
    // ────────────────────────────────────────────────────────────────────────────

    // Default key ayarla — readCard / readSector bu key ile auth yapar
    void setDefaultKey(const KEYBYTES& key,
                       KeyStructure ks   = KeyStructure::NonVolatile,
                       BYTE slot         = 0x01,
                       KeyType kt        = KeyType::A);

    // İki key ayarla (A ve B) — Mifare Classic
    void setKeys(const KEYBYTES& keyA, BYTE slotA,
                 const KEYBYTES& keyB, BYTE slotB,
                 KeyStructure ks = KeyStructure::NonVolatile);

    // Tek key ekle / güncelle (aynı KeyType varsa değiştirir, yoksa ekler)
    // DESFire multi-key senaryoları için kullanılabilir
    void addKey(const KeyInfo& ki);

    // Tüm kayıtlı key'leri temizle ve varsayılana dön
    void clearKeys();

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

    // ────────────────────────────────────────────────────────────────────────────
    // DESFire API (yalnızca isDesfire() true iken geçerli)
    // ────────────────────────────────────────────────────────────────────────────

    // Kart keşfi: GetVersion → DesfireMemoryLayout'a yükle
    DesfireVersionInfo discoverCard();

    // Application seç (session düşer, yeniden auth gerekir)
    void selectApplication(const DesfireAID& aid);

    // DESFire 3-pass mutual auth (seçili app üzerinde)
    void authenticateDesfire(const BYTEV& key, BYTE keyNo, DesfireKeyType keyType);

    // Dosya bilgisi
    std::vector<BYTE> getFileIDs();
    DesfireFileSettings getFileSettings(BYTE fileNo);

    // Dosya okuma/yazma (PlainText — secure messaging Faz 4)
    BYTEV readFileData(BYTE fileNo, uint32_t offset, uint32_t length);
    void  writeFileData(BYTE fileNo, uint32_t offset, const BYTEV& data);

    // Application discovery
    std::vector<DesfireAID> getApplicationIDs();

    // Free memory
    size_t getFreeMemory();

    // DESFire session durumu
    bool isDesfireAuthenticated() const;

    // Session timeout (ms). 0 = sınırsız (varsayılan).
    void setDesfireSessionTimeout(uint32_t ms);

    // ────────────────────────────────────────────────────────────────────────────
    // DESFire Management API (Faz 4)
    // ────────────────────────────────────────────────────────────────────────────

    // Application oluştur/sil
    void createApplication(const DesfireAID& aid, BYTE keySettings,
                            BYTE maxKeys, DesfireKeyType keyType);
    void deleteApplication(const DesfireAID& aid);

    // File oluştur/sil
    void createStdDataFile(BYTE fileNo, DesfireCommMode comm,
                            const DesfireAccessRights& access, uint32_t fileSize);
    void createValueFile(BYTE fileNo, DesfireCommMode comm,
                          const DesfireAccessRights& access,
                          int32_t lower, int32_t upper, int32_t value,
                          bool limitedCredit = false);
    void createLinearRecordFile(BYTE fileNo, DesfireCommMode comm,
                                 const DesfireAccessRights& access,
                                 uint32_t recordSize, uint32_t maxRecords);
    void deleteFile(BYTE fileNo);

    // Record File Operations
    BYTEV readRecords(BYTE fileNo, uint32_t offset, uint32_t count);
    void appendRecord(BYTE fileNo, const BYTEV& recordData);

    // Transaction
    void creditValue(BYTE fileNo, int32_t value);
    void debitValue(BYTE fileNo, int32_t value);
    void commitTransaction();
    void abortTransaction();

    // Key management
    BYTE getKeyVersion(BYTE keyNo);

    // Card-level
    void formatPICC();

    // ────────────────────────────────────────────────────────────────────────────
    // Exception-free alternatifler (Result<T> döner)
    // ────────────────────────────────────────────────────────────────────────────

    // Mifare Classic
    Result<int>            tryReadCard();
    Result<bool>           tryReadSector(int sector);
    Result<BYTEV>          tryReadBlock(int block);
    Result<void>           tryWriteBlock(int block, const BYTE data[16]);
    Result<void>           tryAuthenticate(int sector);
    Result<TrailerConfig>  tryReadTrailer(int sector);
    Result<void>           tryWriteTrailer(int sector, const TrailerConfig& config);

    // DESFire
    Result<DesfireVersionInfo>        tryDiscoverCard();
    Result<void>                      trySelectApplication(const DesfireAID& aid);
    Result<void>                      tryAuthenticateDesfire(const BYTEV& key, BYTE keyNo, DesfireKeyType keyType);
    Result<std::vector<DesfireAID>>   tryGetApplicationIDs();
    Result<std::vector<BYTE>>         tryGetFileIDs();
    Result<DesfireFileSettings>       tryGetFileSettings(BYTE fileNo);
    Result<BYTEV>                     tryReadFileData(BYTE fileNo, uint32_t offset, uint32_t length);
    Result<void>                      tryWriteFileData(BYTE fileNo, uint32_t offset, const BYTEV& data);
    Result<size_t>                    tryGetFreeMemory();
    Result<void>                      tryCreateApplication(const DesfireAID& aid, BYTE keySettings, BYTE maxKeys, DesfireKeyType keyType);
    Result<void>                      tryDeleteApplication(const DesfireAID& aid);
    Result<void>                      tryCreateStdDataFile(BYTE fileNo, DesfireCommMode comm, const DesfireAccessRights& access, uint32_t fileSize);
    Result<void>                      tryCreateValueFile(BYTE fileNo, DesfireCommMode comm, const DesfireAccessRights& access, int32_t lower, int32_t upper, int32_t value, bool limitedCredit = false);
    Result<void>                      tryCreateLinearRecordFile(BYTE fileNo, DesfireCommMode comm, const DesfireAccessRights& access, uint32_t recordSize, uint32_t maxRecords);
    Result<void>                      tryDeleteFile(BYTE fileNo);
    Result<BYTEV>                     tryReadRecords(BYTE fileNo, uint32_t offset, uint32_t count);
    Result<void>                      tryAppendRecord(BYTE fileNo, const BYTEV& recordData);
    Result<void>                      tryCreditValue(BYTE fileNo, int32_t value);
    Result<void>                      tryDebitValue(BYTE fileNo, int32_t value);
    Result<void>                      tryCommitTransaction();
    Result<void>                      tryAbortTransaction();
    Result<BYTE>                      tryGetKeyVersion(BYTE keyNo);
    Result<void>                      tryFormatPICC();

private:
    Reader&        reader_;
    CardInterface  card_;

    // ── Key Storage ─────────────────────────────────────────────────────────
    //
    //  keys_: Kayıtlı tüm key'ler (en az 1 tane — constructor garanti eder).
    //
    //    Mifare Classic  → 1 key (setDefaultKey) veya 2 key (setKeys)
    //    DESFire         → 1–14 key (addKey ile eklenir)
    //
    //  Tek key varsa (keys_.size() == 1) tüm işlemler o key ile yapılır.
    //  Birden fazla key varsa access bits'e göre otomatik seçim yapılır.
    //
    std::vector<KeyInfo> keys_;

    // ── Auth / LoadKey Cache ────────────────────────────────────────────────
    //
    //  slotContents_: Reader belleğindeki slot → yüklü key bytes eşlemesi.
    //    Aynı slot/key zaten yüklü ise loadKey tekrarlanmaz.
    //    Farklı slot'lardaki key'ler aynı anda loaded kalabilir.
    //
    //  lastAuthSector_ / lastAuthKT_: Son başarılı auth bilgisi.
    //    Aynı sektör + aynı amaç için yeniden auth atlanır.
    //    Farklı amaç (Read→Write) ise permission kontrolü yapılır,
    //    mevcut key yeterliyse auth atlanır, yetersizse yeniden auth yapılır.
    //
    int            lastAuthSector_ = -1;
    KeyType        lastAuthKT_     = KeyType::A;
    std::map<BYTE, KEYBYTES> slotContents_;

    // ── Private Helpers ─────────────────────────────────────────────────────
    void ensureAuth(int sector, AuthPurpose purpose = AuthPurpose::Read);
    void doAuth(int sector, const KeyInfo& ki);
    void ensureKeyLoaded(const KeyInfo& ki);
    void invalidateAuth();
    const KeyInfo& chooseKey(int sector, AuthPurpose purpose) const;
    const KeyInfo& findKey(KeyType kt) const;
    bool isMultiKey() const;

    Result<void> tryEnsureAuth(int sector, AuthPurpose purpose = AuthPurpose::Read);
    Result<void> tryDoAuth(int sector, const KeyInfo& ki);

    // Classic → sector access bits, DESFire → KeyInfo::permission
    bool canKeyPerform(const KeyInfo& ki, int sector, AuthPurpose purpose) const;

    // ── DESFire State ───────────────────────────────────────────────────────
    std::unique_ptr<DesfireSession> desfireSession_;

    // Raw APDU transmit through Reader's CardConnection
    BYTEV desfireTransmit(const BYTEV& apdu);
    Result<BYTEV> tryDesfireTransmit(const BYTEV& apdu);

    // TransmitFn factory (for DesfireAuth / DesfireCommands callbacks)
    std::function<BYTEV(const BYTEV&)> makeTransmitFn();
    std::function<Result<BYTEV>(const BYTEV&)> makeTryTransmitFn();

    // DESFire helpers — reduce boilerplate
    Result<void>  desfireExec(const BYTEV& apdu);
    Result<BYTEV> desfireQuery(const BYTEV& apdu);
};

#endif // CARDIO_H
