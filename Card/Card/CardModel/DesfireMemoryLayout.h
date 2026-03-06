#ifndef DESFIREMEMORYLAYOUT_H
#define DESFIREMEMORYLAYOUT_H

#include "../CardDataTypes.h"
#include <array>
#include <vector>
#include <cstdint>
#include <cstring>

// ════════════════════════════════════════════════════════════════════════════════
// DESFire Memory Layout — Dosya Sistemi Tabanlı Dinamik Model
// ════════════════════════════════════════════════════════════════════════════════
//
// Classic/Ultralight'tan temel fark:
//   Classic    → sabit byte dizisi, union view'lar, derleme zamanı layout
//   DESFire    → dinamik app/file hiyerarşisi, çalışma zamanında keşif
//
// DESFire Hiyerarşi:
//   Card (PICC)
//   ├── PICC Master App (AID 0x000000) — kart seviyesi key/settings
//   ├── Application 1 (AID 3 byte)
//   │   ├── File 0 (StandardData, 128 byte)
//   │   ├── File 1 (Value)
//   │   └── File 2 (LinearRecord, 64B × 10 kayıt)
//   ├── Application 2 (AID 3 byte)
//   │   └── File 0 (StandardData, 256 byte)
//   └── ... (max 28 uygulama)
//
// Adres şeması:
//   Classic    → block index (0–63 / 0–255)
//   Ultralight → page index  (0–15) veya virtual block (0–3)
//   DESFire    → (AID, fileNo, offset, length)
//
// Kart boyut varyantları (EV1: 2K/4K/8K, EV2: 2K–32K, EV3: 2K/4K/8K):
//   Boyut farkı yapıyı DEĞİŞTİRMEZ — sadece totalMemory/freeMemory değeri farklıdır.
//   Bu nedenle tüm varyantlar tek DesfireMemoryLayout ile temsil edilir.
//
// TASARIM NOTU:
//   Bu dosya şu an TASLAK/REFERANS niteliğindedir.
//   Faz-1 entegrasyonunda CardInterface ile bağlanacak,
//   ihtiyaca göre değişebilir.
//
// ════════════════════════════════════════════════════════════════════════════════

// ── DESFire Kart Varyantları ────────────────────────────────────────────────

enum class DesfireVariant : BYTE {
    EV1   = 0x01,
    EV2   = 0x02,
    EV3   = 0x03,
    Light = 0x10
};

// ── GetVersion Yanıtı ───────────────────────────────────────────────────────
//
//   storageSize byte → kapasite:
//     0x16 = 2KB, 0x18 = 4KB, 0x1A = 8KB, 0x1C = 16KB, 0x1E = 32KB
//

struct DesfireVersionInfo {
    // Hardware
    BYTE hwVendorID   = 0;
    BYTE hwType       = 0;
    BYTE hwSubType    = 0;
    BYTE hwMajorVer   = 0;
    BYTE hwMinorVer   = 0;
    BYTE hwStorageSize = 0;
    BYTE hwProtocol   = 0;

    // Software
    BYTE swVendorID   = 0;
    BYTE swType       = 0;
    BYTE swSubType    = 0;
    BYTE swMajorVer   = 0;
    BYTE swMinorVer   = 0;
    BYTE swStorageSize = 0;
    BYTE swProtocol   = 0;

    // UID + Production
    BYTE uid[7]       = {};
    BYTE batchNo[5]   = {};
    BYTE productionWeek = 0;
    BYTE productionYear = 0;

    // storageSize byte → byte cinsinden kapasite
    static size_t storageSizeToBytes(BYTE ss) {
        switch (ss) {
            case 0x16: return 2048;
            case 0x18: return 4096;
            case 0x1A: return 8192;
            case 0x1C: return 16384;
            case 0x1E: return 32768;
            default:   return 0;
        }
    }

    size_t totalCapacity() const {
        return storageSizeToBytes(hwStorageSize);
    }

    DesfireVariant guessVariant() const {
        if (hwMajorVer == 0 && hwMinorVer == 0) return DesfireVariant::Light;
        else if (swMajorVer >= 3) return DesfireVariant::EV3;
        else if (swMajorVer >= 2) return DesfireVariant::EV2;
        else return DesfireVariant::EV1;
    }
};

// ── AID — 3-byte Application Identifier ─────────────────────────────────────

struct DesfireAID {
    BYTE aid[3] = {0, 0, 0};

    bool operator==(const DesfireAID& o) const { return std::memcmp(aid, o.aid, 3) == 0; }
    bool operator!=(const DesfireAID& o) const { return !(*this == o); }

    bool isPICC() const { return aid[0] == 0 && aid[1] == 0 && aid[2] == 0; }

    uint32_t toUint() const {
        return static_cast<uint32_t>(aid[0])
             | (static_cast<uint32_t>(aid[1]) << 8)
             | (static_cast<uint32_t>(aid[2]) << 16);
    }

    static DesfireAID fromUint(uint32_t v) {
        DesfireAID a;
        a.aid[0] = static_cast<BYTE>(v & 0xFF);
        a.aid[1] = static_cast<BYTE>((v >> 8) & 0xFF);
        a.aid[2] = static_cast<BYTE>((v >> 16) & 0xFF);
        return a;
    }

    static DesfireAID picc() { return {}; }
};

// ── Dosya Tipleri ───────────────────────────────────────────────────────────

enum class DesfireFileType : BYTE {
    StandardData  = 0x00,
    BackupData    = 0x01,
    Value         = 0x02,
    LinearRecord  = 0x03,
    CyclicRecord  = 0x04
};

// ── İletişim Modu (güvenlik seviyesi) ───────────────────────────────────────

enum class DesfireCommMode : BYTE {
    Plain = 0x00,           // şifresiz
    MAC   = 0x01,           // sadece bütünlük
    Full  = 0x03            // tam şifreleme + bütünlük
};

// ── Key Tipleri ─────────────────────────────────────────────────────────────

enum class DesfireKeyType : BYTE {
    DES      = 0x00,        //  8 byte
    TwoDES   = 0x40,        // 16 byte (2K3DES)
    ThreeDES = 0xC0,        // 24 byte (3K3DES)
    AES128   = 0x80         // 16 byte
};

// ── Dosya Erişim Hakları ────────────────────────────────────────────────────
//
//   Her dosya için 4 işlem tipi × key numarası (0–13, 0x0E=free, 0x0F=deny)
//

struct DesfireAccessRights {
    BYTE readKey      = 0x0E;   // 0x0E = free (key gerekmez)
    BYTE writeKey     = 0x0F;   // 0x0F = deny (izin yok)
    BYTE readWriteKey = 0x0E;
    BYTE changeKey    = 0x00;   // genelde app master key

    bool isFreeRead()  const { return readKey  == 0x0E; }
    bool isDenyRead()  const { return readKey  == 0x0F; }
    bool isFreeWrite() const { return writeKey == 0x0E; }
    bool isDenyWrite() const { return writeKey == 0x0F; }

    // 2-byte packed format: [readKey|writeKey] [readWriteKey|changeKey]
    std::array<BYTE, 2> encode() const {
        return {
            static_cast<BYTE>((readKey << 4) | writeKey),
            static_cast<BYTE>((readWriteKey << 4) | changeKey)
        };
    }

    static DesfireAccessRights decode(BYTE b0, BYTE b1) {
        return {
            static_cast<BYTE>((b0 >> 4) & 0x0F),
            static_cast<BYTE>(b0 & 0x0F),
            static_cast<BYTE>((b1 >> 4) & 0x0F),
            static_cast<BYTE>(b1 & 0x0F)
        };
    }
};

// ── Dosya Ayarları (tip bazlı union) ────────────────────────────────────────

struct DesfireFileSettings {
    DesfireFileType  fileType = DesfireFileType::StandardData;
    DesfireCommMode  commMode = DesfireCommMode::Plain;
    DesfireAccessRights access;

    // Dosya tipine göre farklı parametreler
    union {
        struct {
            uint32_t fileSize;              // byte
        } standard;                         // StandardData & BackupData

        struct {
            int32_t  lowerLimit;
            int32_t  upperLimit;
            int32_t  value;
            bool     limitedCreditEnabled;
        } value;                            // Value file

        struct {
            uint32_t recordSize;            // tek kayıt boyutu (byte)
            uint32_t maxRecords;
            uint32_t currentRecords;
        } record;                           // LinearRecord & CyclicRecord
    };

    // Dosyanın veri kapasitesi (byte)
    uint32_t dataCapacity() const {
        switch (fileType) {
            case DesfireFileType::StandardData:
            case DesfireFileType::BackupData:
                return standard.fileSize;
            case DesfireFileType::Value:
                return 4;   // value her zaman 4 byte (int32)
            case DesfireFileType::LinearRecord:
            case DesfireFileType::CyclicRecord:
                return record.recordSize * record.maxRecords;
            default:
                return 0;
        }
    }

    DesfireFileSettings() { std::memset(&standard, 0, sizeof(record)); }
};

// ── Dosya — Ayarlar + Cache ─────────────────────────────────────────────────
//
//   Classic'teki `MifareBlock.raw[16]` karşılığı: `data` vektörü.
//   Fark: boyut sabit değil, dosya oluşturulurken belirlenir.
//

struct DesfireFile {
    BYTE                fileNo = 0;
    DesfireFileSettings settings;
    BYTEV               data;               // okuma sonrası cache

    bool isEmpty() const { return data.empty(); }

    void allocate() {
        data.resize(settings.dataCapacity(), 0);
    }
};

// ── Application Key Konfigürasyonu ──────────────────────────────────────────

struct DesfireKeyConfig {
    BYTE           keyCount    = 1;         // 1–14
    DesfireKeyType keyType     = DesfireKeyType::AES128;
    BYTE           keySettings = 0x0F;      // master key config byte

    // keySettings bit alanları:
    bool allowChangeMasterKey()    const { return keySettings & 0x01; }
    bool allowFreeDirectoryList()  const { return keySettings & 0x02; }
    bool allowFreeCreateDelete()   const { return keySettings & 0x04; }
    bool allowConfigChangeable()   const { return keySettings & 0x08; }
};

// ── Uygulama — Key config + Dosyalar ────────────────────────────────────────
//
//   Classic'teki "sektör" karşılığı bir nevi — ama çok daha esnek.
//   Her app bağımsız key seti ve dosya kümesi tutar.
//

struct DesfireApplication {
    DesfireAID       aid;
    DesfireKeyConfig keyConfig;
    std::vector<DesfireFile> files;         // max 32 dosya

    // ── Dosya Sorguları ─────────────────────────────────────────────────────

    DesfireFile* findFile(BYTE fileNo) {
        for (auto& f : files) {
            if (f.fileNo == fileNo) return &f;
        }
        return nullptr;
    }

    const DesfireFile* findFile(BYTE fileNo) const {
        for (const auto& f : files) {
            if (f.fileNo == fileNo) return &f;
        }
        return nullptr;
    }

    bool hasFile(BYTE fileNo) const { return findFile(fileNo) != nullptr; }

    // ── Toplam veri kapasitesi ──────────────────────────────────────────────

    uint32_t totalDataSize() const {
        uint32_t sum = 0;
        for (const auto& f : files) sum += f.settings.dataCapacity();
        return sum;
    }
};

// ════════════════════════════════════════════════════════════════════════════════
// DesfireMemoryLayout — Kart Seviyesi Model
// ════════════════════════════════════════════════════════════════════════════════
//
// Tasarım Notu:
//   Classic/Ultralight: sabit union (raw[] + blocks[] + sectors[])
//   DESFire:            dinamik hiyerarşi (apps[] → files[] → data[])
//
//   Statik union'a gömülemez çünkü app/file sayısı çalışma zamanında değişir.
//   Bu nedenle CardMemoryLayout union'ının dışında yaşar ve
//   CardInterface aracılığıyla erişilir.
//
//   Tüm boyut varyantları (EV1 2K–8K, EV2 2K–32K, EV3, Light) aynı struct
//   ile temsil edilir. Kapasite runtime'da GetVersion komutuyla belirlenir.
//
// Adres eşlemesi:
//   readBlock(n) çağrısında DESFire "aktif bağlam" (currentAID + currentFileNo)
//   kullanılarak virtual block erişimi sağlanır:
//     block N  →  offset = N * 16, length = 16
//
// ════════════════════════════════════════════════════════════════════════════════

struct DesfireMemoryLayout {

    // ── Kart Bilgileri ──────────────────────────────────────────────────────

    DesfireVersionInfo versionInfo;          // GetVersion yanıtından
    DesfireVariant     variant = DesfireVariant::EV1;
    size_t             totalMemory = 0;      // byte (2K/4K/8K/16K/32K)
    size_t             freeMemory  = 0;      // GetFreeMemory yanıtından

    // ── PICC Master Key ─────────────────────────────────────────────────────

    DesfireKeyConfig piccKeyConfig;

    // ── Uygulamalar ─────────────────────────────────────────────────────────

    std::vector<DesfireApplication> applications;   // max 28

    // ── Aktif bağlam (virtual block erişimi için) ───────────────────────────

    DesfireAID currentAID  = DesfireAID::picc();
    BYTE       currentFile = 0;

    // ── Başlatma (GetVersion yanıtından) ────────────────────────────────────

    void initFromVersion(const DesfireVersionInfo& vi) {
        versionInfo = vi;
        variant     = vi.guessVariant();
        totalMemory = vi.totalCapacity();
        freeMemory  = totalMemory;          // GetFreeMemory ile güncellenir
    }

    // ── Uygulama Sorguları ──────────────────────────────────────────────────

    DesfireApplication* findApp(const DesfireAID& aid) {
        for (auto& a : applications) {
            if (a.aid == aid) return &a;
        }
        return nullptr;
    }

    const DesfireApplication* findApp(const DesfireAID& aid) const {
        for (const auto& a : applications) {
            if (a.aid == aid) return &a;
        }
        return nullptr;
    }

    bool hasApp(const DesfireAID& aid) const { return findApp(aid) != nullptr; }

    // ── Aktif bağlamdaki dosyaya erişim ─────────────────────────────────────

    DesfireFile* activeFile() {
        auto* app = findApp(currentAID);
        return app ? app->findFile(currentFile) : nullptr;
    }

    const DesfireFile* activeFile() const {
        const auto* app = findApp(currentAID);
        return app ? app->findFile(currentFile) : nullptr;
    }

    // ── Virtual Block Erişimi ───────────────────────────────────────────────
    //
    //   Aktif dosyadaki veriyi 16-byte "blok"lar halinde sunar.
    //   Böylece dışarıdan readBlock(n) / writeBlock(n) çalışır.
    //
    //   block 0 → data[0..15]
    //   block 1 → data[16..31]
    //   ...
    //

    int virtualBlockCount() const {
        const auto* f = activeFile();
        if (!f || f->data.empty()) return 0;
        return static_cast<int>((f->data.size() + 15) / 16);
    }

    // 16-byte virtual block'u hedefe kopyala; kısa blokta kalan byte sıfır
    bool readVirtualBlock(int blockIndex, BYTE out[16]) const {
        const auto* f = activeFile();
        if (!f) return false;

        size_t offset = static_cast<size_t>(blockIndex) * 16;
        if (offset >= f->data.size()) return false;

        std::memset(out, 0, 16);
        size_t toCopy = std::min<size_t>(16, f->data.size() - offset);
        std::memcpy(out, f->data.data() + offset, toCopy);
        return true;
    }

    // 16-byte virtual block yaz
    bool writeVirtualBlock(int blockIndex, const BYTE in[16]) {
        auto* f = activeFile();
        if (!f) return false;

        size_t offset = static_cast<size_t>(blockIndex) * 16;
        if (offset >= f->data.size()) return false;

        size_t toCopy = std::min<size_t>(16, f->data.size() - offset);
        std::memcpy(f->data.data() + offset, in, toCopy);
        return true;
    }
};

#endif // DESFIREMEMORYLAYOUT_H
