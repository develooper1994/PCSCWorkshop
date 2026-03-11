#ifndef PCSC_COMMANDS_H
#define PCSC_COMMANDS_H

#include "StatusWordHandler.h"
#include "Result.h"
#include <string>

// ════════════════════════════════════════════════════════════════════════════════
// PcscCommands — PC/SC Pseudo-APDU Komut ve Durum Kodu Kataloğu
// ════════════════════════════════════════════════════════════════════════════════
//
// ISO 7816-4 / PC/SC Part 3 standardına dayalı genel amaçlı komutlar.
// Tüm komutlar CLA=0xFF (PC/SC pseudo-APDU) kullanır.
// Reader modelinden bağımsızdır — parametre byte çevrimi Reader alt sınıfına aittir.
//
// ─── Mimari ────────────────────────────────────────────────────────────────
//
//   PcscCommands           Genel PC/SC komutları (bu sınıf)
//       │  APDU üret
//       ▼
//   Reader::transmit()     Virtual — reader'a özel iletim (ACR1281U, OmniKey)
//       │  Gönder + yanıt ayrıştır
//       ▼
//   PcscCommands::check*   Durum kodu kontrolü
//
//   Reader-özel parametre çevrimi:
//     KeyStructure::NonVolatile → 0x20 (ACR1281U) veya farklı (OmniKey)
//     Bu çevrim Reader alt sınıfındaki mapKeyStructure/mapKeyKind ile yapılır.
//     PcscCommands her zaman ham byte alır.
//
// ─── Kullanım ──────────────────────────────────────────────────────────────
//
//   // Doğrudan (Reader metotları bunu zaten yapıyor):
//   auto apdu = PcscCommands::readBinary(4, 16);
//   auto resp = reader.transmit(apdu);
//   PcscCommands::checkRead(resp.sw);
//
//   // Komut tanılama:
//   PcscCommands::describeCommand(0xB0);   // → "READ BINARY"
//   PcscCommands::describeStatus(sw);      // → "0x69 0x82 — Security status not satisfied"
//
// ════════════════════════════════════════════════════════════════════════════════

class PcscCommands {
public:

	// ══════════════════════════════════════════════════════════════════════════
	// INS — Instruction Byte Kataloğu (CLA=0xFF)
	// ══════════════════════════════════════════════════════════════════════════
	//
	//  INS   Komut                   Açıklama
	//  ───── ─────────────────────── ─────────────────────────────────────────
	//  0x00  ESCAPE                  Reader'a özel vendor komutu (LED, buzzer)
	//  0x20  VERIFY                  PIN doğrulama
	//  0x24  CHANGE_PIN              PIN değiştir / engeli kaldır
	//  0x70  MANAGE_SESSION          T=CL oturum yönetimi
	//  0x82  LOAD KEY                Reader belleğine key yükle
	//  0x86  GENERAL AUTHENTICATE    Yeni format kimlik doğrulama
	//  0x88  AUTHENTICATE (Legacy)   Eski format kimlik doğrulama
	//  0xB0  READ BINARY             Sayfa/blok oku
	//  0xB1  READ BINARY (odd)       256+ byte okuma (uzun alan)
	//  0xCA  GET DATA                UID / ATS / Historical bytes sorgula
	//  0xD6  UPDATE BINARY           Sayfa/blok yaz
	//  0xD7  UPDATE BINARY (odd)     256+ byte yazma (uzun alan)

	static constexpr BYTE CLA = 0xFF;

	struct INS {
		static constexpr BYTE ESCAPE             = 0x00;
		static constexpr BYTE VERIFY             = 0x20;
		static constexpr BYTE CHANGE_PIN         = 0x24;
		static constexpr BYTE MANAGE_SESSION     = 0x70;
		static constexpr BYTE LOAD_KEY           = 0x82;
		static constexpr BYTE AUTH_GENERAL       = 0x86;
		static constexpr BYTE AUTH_LEGACY        = 0x88;
		static constexpr BYTE READ_BINARY        = 0xB0;
		static constexpr BYTE READ_BINARY_ODD    = 0xB1;
		static constexpr BYTE GET_DATA           = 0xCA;
		static constexpr BYTE UPDATE_BINARY      = 0xD6;
		static constexpr BYTE UPDATE_BINARY_ODD  = 0xD7;
	};

	// ══════════════════════════════════════════════════════════════════════════
	// SW — ISO 7816-4 Status Word Kataloğu
	// ══════════════════════════════════════════════════════════════════════════
	//
	//   Kod     Anlamı
	//   ─────── ───────────────────────────────────────────────────────
	//   9000    Başarılı
	//   6300    Auth sentinel (kimlik doğrulama gerekli)
	//   63CX    PIN doğrulama başarısız, X deneme kaldı
	//   6400    Çalıştırma hatası, durum değişmedi
	//   6500    Çalıştırma hatası, durum değişti
	//   6700    Yanlış uzunluk (Lc veya Le)
	//   6800    CLA fonksiyonları desteklenmiyor
	//   6982    Güvenlik durumu karşılanmadı
	//   6983    Kimlik doğrulama yöntemi engellendi
	//   6984    Referans veri geçersiz kılındı
	//   6985    Kullanım koşulları karşılanmadı
	//   6A81    Fonksiyon desteklenmiyor
	//   6A82    Dosya/uygulama bulunamadı
	//   6A86    Hatalı P1-P2 parametreleri
	//   6A87    Lc, P1-P2 ile tutarsız
	//   6B00    Hatalı P1-P2 (offset aralık dışı)
	//   6CXX    Yanlış Le, XX = doğru uzunluk
	//   6D00    INS desteklenmiyor
	//   6E00    CLA desteklenmiyor
	//   6F00    Tanı yok (genel hata)
	//
	//   StatusWord struct'ı (StatusWordHandler.h) bu kodları yorumlar.
	//   Bu sabitler eşleştirme ve loglama için kullanılır.

	struct SW {
		// ── Başarı ──────────────────────────────────────────────
		static constexpr uint16_t SUCCESS                 = 0x9000;
		static constexpr uint16_t DESFIRE_SUCCESS         = 0x9100;

		// ── Uyarı ──────────────────────────────────────────────
		static constexpr uint16_t AUTH_REQUIRED           = 0x6300;

		// ── Çalıştırma Hataları ─────────────────────────────────
		static constexpr uint16_t EXEC_ERROR_UNCHANGED    = 0x6400;
		static constexpr uint16_t EXEC_ERROR_CHANGED      = 0x6500;

		// ── Uzunluk / Format ────────────────────────────────────
		static constexpr uint16_t WRONG_LENGTH            = 0x6700;
		static constexpr uint16_t CLA_FUNC_NOT_SUPPORTED  = 0x6800;

		// ── Güvenlik ────────────────────────────────────────────
		static constexpr uint16_t SECURITY_NOT_SATISFIED  = 0x6982;
		static constexpr uint16_t AUTH_BLOCKED            = 0x6983;
		static constexpr uint16_t REF_DATA_INVALIDATED    = 0x6984;
		static constexpr uint16_t CONDITIONS_NOT_SATISFIED = 0x6985;

		// ── Parametre Hataları ──────────────────────────────────
		static constexpr uint16_t FUNC_NOT_SUPPORTED      = 0x6A81;
		static constexpr uint16_t FILE_NOT_FOUND          = 0x6A82;
		static constexpr uint16_t INCORRECT_P1P2          = 0x6A86;
		static constexpr uint16_t LC_INCONSISTENT         = 0x6A87;
		static constexpr uint16_t WRONG_P1P2              = 0x6B00;
		static constexpr uint16_t INS_NOT_SUPPORTED       = 0x6D00;
		static constexpr uint16_t CLA_NOT_SUPPORTED       = 0x6E00;
		static constexpr uint16_t NO_PRECISE_DIAGNOSIS    = 0x6F00;

		// ── Değişken SW2 ────────────────────────────────────────
		// Bunlar sadece SW1 sabitidir, SW2 değişkendir.
		static constexpr BYTE WRONG_LE_SW1     = 0x6C; // SW2 = doğru Le değeri
		static constexpr BYTE VERIFY_FAIL_SW1  = 0x63; // SW2 & 0xC0 ise, alt 4 bit = kalan deneme
	};

	// StatusWord ↔ uint16_t çevrimi
	static uint16_t toCode(const StatusWord& sw) noexcept {
		return (static_cast<uint16_t>(sw.sw1) << 8) | sw.sw2;
	}

	static StatusWord fromCode(uint16_t code) noexcept {
		return StatusWord(static_cast<BYTE>(code >> 8),
		                  static_cast<BYTE>(code & 0xFF));
	}

	// ══════════════════════════════════════════════════════════════════════════
	// APDU Construction — Genel PC/SC komutları
	// ══════════════════════════════════════════════════════════════════════════
	//
	// Bu APDU'lar reader-bağımsızdır.
	// Reader-özel parametre mapping (keyStructure, keyType byte değerleri)
	// Reader alt sınıflarındaki mapKeyStructure/mapKeyKind ile çözülür.

	// ── Okuma / Yazma ───────────────────────────────────────────────────

	// FF B0 00 {page} {le}
	static BYTEV readBinary(BYTE page, BYTE le);

	// FF D6 00 {page} {lc} [data...]
	static BYTEV updateBinary(BYTE page, const BYTE* data, BYTE lc);

	// ── Key / Auth ──────────────────────────────────────────────────────

	// FF 82 {keyStructure} {keyNumber} {keyLen} [key...]
	//   keyStructure: Reader-özel byte (ACR1281U: 0x00=Volatile, 0x20=NonVolatile)
	//   keyNumber:    Key slot (00h-1Fh=NonVolatile, 20h=Volatile session key)
	static BYTEV loadKey(BYTE keyStructure, BYTE keyNumber,
	                     const BYTE* key, BYTE keyLen = 6);

	// FF 88 00 {blockNumber} {keyType} {keyNumber}
	//   keyType: Reader-özel byte (ACR1281U: 0x60=KeyA, 0x61=KeyB)
	static BYTEV authLegacy(BYTE blockNumber, BYTE keyType, BYTE keyNumber);

	// FF 86 00 00 05 [01 00 {blockNumber} {keyType} {keyNumber}]
	static BYTEV authGeneral(const BYTE data[5]);

	// ── Sorgulama ───────────────────────────────────────────────────────

	// FF CA 00 00 00 — UID al
	static BYTEV getUID();

	// FF CA 01 00 00 — ATS / Historical bytes al
	static BYTEV getATS();

	// ── Vendor Escape ───────────────────────────────────────────────────

	// FF 00 00 00 {Lc} [data...]
	// Reader-özel komutlar için temel yapı (LED, buzzer, firmware vb.)
	// ACR1281U ve OmniKey farklı data formatları kullanır.
	static BYTEV escape(const BYTEV& data);

	// ══════════════════════════════════════════════════════════════════════════
	// Response Evaluation — Exception-free (PcscError döner, throw etmez)
	// ══════════════════════════════════════════════════════════════════════════

	static Result<void, PcscError> evaluateRead(const StatusWord& sw);
	static Result<void, PcscError> evaluateWrite(const StatusWord& sw);
	static Result<void, PcscError> evaluateLoadKey(const StatusWord& sw);
	static Result<void, PcscError> evaluateAuth(const StatusWord& sw);
	static Result<void, PcscError> evaluateExpected(const StatusWord& sw, uint16_t expected,
									 const std::string& context);

	// ══════════════════════════════════════════════════════════════════════════
	// Response Validation — Throwing (evaluate + throwIfError)
	// ══════════════════════════════════════════════════════════════════════════

	static void checkRead(const StatusWord& sw);
	static void checkWrite(const StatusWord& sw);
	static void checkLoadKey(const StatusWord& sw);
	static void checkAuth(const StatusWord& sw);

	static void checkExpected(const StatusWord& sw, uint16_t expected,
							  const std::string& context);

	// ══════════════════════════════════════════════════════════════════════════
	// Diagnostics
	// ══════════════════════════════════════════════════════════════════════════

	static std::string describeCommand(BYTE ins);
	static std::string describeStatus(const StatusWord& sw);
	static std::string describeStatus(uint16_t code);
};

#endif // PCSC_COMMANDS_H
