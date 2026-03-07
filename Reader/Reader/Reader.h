#ifndef PCSC_WORKSHOP1_READER_H
#define PCSC_WORKSHOP1_READER_H

#include "CardConnection.h"
#include "StatusWordHandler.h"
#include "CardDataTypes.h"
#include "Result.h"
#include <string>
#include <memory>

// ════════════════════════════════════════════════════════════════════════════════
// Reader — Alt seviye PC/SC APDU haberleşme soyutlaması
// ════════════════════════════════════════════════════════════════════════════════
//
// Reader yalnızca PC/SC komutlarını iletir ve durum kodlarını kontrol eder.
// Key yönetimi, auth politikası ve şifreleme üst katmana (CardIO) aittir.
//
// ─── Sorumluluklar ─────────────────────────────────────────────────────────
//   ✓ readPage / writePage  — tek sayfa APDU
//   ✓ readData / writeData  — çok sayfalı convenience
//   ✓ loadKey / auth        — ham PC/SC auth komutları
//   ✓ getLE / setLE         — blok boyutu yapılandırması
//
// ─── Üst katmana bırakılanlar ──────────────────────────────────────────────
//   ✗ Hangi key ile auth yapılacağı (CardIO.ensureAuth)
//   ✗ Key blob yönetimi [keyA|access|keyB] (CardIO.keys_)
//   ✗ Otomatik auth retry (CardIO.doAuth)
//   ✗ Şifreli okuma/yazma (uygulama katmanı)
//
// ─── Kullanım ──────────────────────────────────────────────────────────────
//   ACR1281UReader reader(conn, 16);         // 16 byte blok
//   reader.loadKey(key, KeyStructure::NonVolatile, 0x01);
//   reader.auth(blockNo, KeyType::A, 0x01);
//   auto data = reader.readPage(blockNo);
//   reader.writePage(blockNo, payload);
// ════════════════════════════════════════════════════════════════════════════════

enum class ReaderType {
	Unknown,
	OmniKey,
	ACR1281U
};

inline std::string describe(ReaderType type) {
	switch (type) {
		case ReaderType::ACR1281U: return "ACR1281U";
		case ReaderType::OmniKey:  return "OmniKey";
		default: return "Unknown";
	}
}

// ── Yapılandırılmış APDU yanıtı ───────────────────────────────────────────
struct ReaderResponse {
	BYTEV data;        // SW byte'ları ayrılmış saf veri
	StatusWord sw;

	bool isSuccess()      const noexcept { return sw.isSuccess(); }
	bool isAuthRequired() const noexcept { return sw.isAuthSentinel(); }

	// Orijinal yanıtı yeniden oluştur: data + SW1 + SW2
	BYTEV raw() const {
		BYTEV r = data;
		r.push_back(sw.sw1);
		r.push_back(sw.sw2);
		return r;
	}
};

class Reader {
public:
	virtual ~Reader();

	explicit Reader(CardConnection& c);
	Reader(CardConnection& c, BYTE blockSize);

	Reader(const Reader&) = delete;
	Reader& operator=(const Reader&) = delete;
	Reader(Reader&&) noexcept;
	Reader& operator=(Reader&&) noexcept;

	// ── Generic APDU ─────────────────────────────────────────────────────
	// Herhangi bir APDU gönder, yapılandırılmış yanıt al.
	// OmniKey gibi farklı reader'lar bu metodu override edebilir.
	virtual ReaderResponse transmit(const BYTEV& apdu);

	// ── Core I/O ──────────────────────────────────────────────────────────

	virtual void writePage(BYTE page, const BYTE* data, const BYTEV* customApdu = nullptr);
	virtual BYTEV readPage(BYTE page, const BYTEV* customApdu = nullptr);
	virtual void clearPage(BYTE page);

	// Convenience: container overloads (validate + pad to block size)
	void writePage(BYTE page, const BYTEV& data);
	void writePage(BYTE page, const std::string& s);

	// Convenience: explicit length (temporarily adjusts block size)
	void writePage(BYTE page, const BYTE* data, BYTE len);
	BYTEV readPage(BYTE page, BYTE len);

	// ── Multi-page I/O ────────────────────────────────────────────────────

	virtual void writeData(BYTE startPage, const BYTEV& data);
	void writeData(BYTE startPage, const std::string& s);
	void writeData(BYTE startPage, const BYTEV& data, BYTE le);
	void writeData(BYTE startPage, const std::string& s, BYTE le);
	virtual BYTEV readData(BYTE startPage, size_t length);
	BYTEV readAll(BYTE startPage = 0);

	// ── PC/SC Auth Commands ───────────────────────────────────────────────

	void loadKey(const BYTE* key, KeyStructure ks, BYTE keyNumber);
	void auth(BYTE blockNumber, KeyType keyType, BYTE keyNumber);
	void authNew(BYTE blockNumber, KeyType keyType, BYTE keyNumber);
	void authNew(const BYTE data[5]);

	// ── Exception-free alternatifler (Result<T> döner) ────────────────────

	Result<ReaderResponse> tryTransmit(const BYTEV& apdu);
	Result<BYTEV>          tryReadPage(BYTE page, const BYTEV* customApdu = nullptr);
	Result<void>           tryWritePage(BYTE page, const BYTE* data, const BYTEV* customApdu = nullptr);
	Result<void>           tryClearPage(BYTE page);
	Result<void>           tryLoadKey(const BYTE* key, KeyStructure ks, BYTE keyNumber);
	Result<void>           tryAuth(BYTE blockNumber, KeyType keyType, BYTE keyNumber);
	Result<void>           tryAuthNew(BYTE blockNumber, KeyType keyType, BYTE keyNumber);
	Result<void>           tryAuthNew(const BYTE data[5]);

	// ── Configuration ─────────────────────────────────────────────────────

	BYTE getLE() const noexcept;
	void setLE(BYTE le) noexcept;

	virtual ReaderType getReaderType() const noexcept = 0;

	CardConnection& cardConnection() noexcept;
	const CardConnection& cardConnection() const noexcept;

protected:
	struct Impl;
	std::unique_ptr<Impl> pImpl;

	virtual BYTE mapKeyStructure(KeyStructure structure) const noexcept = 0;
	virtual BYTE mapKeyKind(KeyType kind) const noexcept = 0;

	// RAII guard for temporary LE changes — exception-safe
	struct LEGuard {
		Reader& reader;
		BYTE saved;
		LEGuard(Reader& r, BYTE newLE) : reader(r), saved(r.getLE()) { reader.setLE(newLE); }
		~LEGuard() { reader.setLE(saved); }
		LEGuard(const LEGuard&) = delete;
		LEGuard& operator=(const LEGuard&) = delete;
	};

private:
	BYTEV padToBlock(const BYTE* data, size_t dataLen) const;
};

#endif // PCSC_WORKSHOP1_READER_H
