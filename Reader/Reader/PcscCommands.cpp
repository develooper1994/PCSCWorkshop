#include "PcscCommands.h"

// ============================================================
// APDU Construction — Okuma / Yazma
// ============================================================

BYTEV PcscCommands::readBinary(BYTE page, BYTE le) {
	return {CLA, INS::READ_BINARY, 0x00, page, le};
}

BYTEV PcscCommands::updateBinary(BYTE page, const BYTE* data, BYTE lc) {
	BYTEV apdu{CLA, INS::UPDATE_BINARY, 0x00, page, lc};
	apdu.insert(apdu.end(), data, data + lc);
	return apdu;
}

// ============================================================
// APDU Construction — Key / Auth
// ============================================================

BYTEV PcscCommands::loadKey(BYTE keyStructure, BYTE keyNumber,
							const BYTE* key, BYTE keyLen) {
	BYTEV apdu{CLA, INS::LOAD_KEY, keyStructure, keyNumber, keyLen};
	apdu.insert(apdu.end(), key, key + keyLen);
	return apdu;
}

BYTEV PcscCommands::authLegacy(BYTE blockNumber, BYTE keyType, BYTE keyNumber) {
	return {CLA, INS::AUTH_LEGACY, 0x00, blockNumber, keyType, keyNumber};
}

BYTEV PcscCommands::authGeneral(const BYTE data[5]) {
	BYTE lc = 0x05;
	BYTEV apdu{CLA, INS::AUTH_GENERAL, 0x00, 0x00, lc};
	apdu.insert(apdu.end(), data, data + lc);
	return apdu;
}

// ============================================================
// APDU Construction — Sorgulama
// ============================================================

BYTEV PcscCommands::getUID() {
	return {CLA, INS::GET_DATA, 0x00, 0x00, 0x00};
}

BYTEV PcscCommands::getATS() {
	return {CLA, INS::GET_DATA, 0x01, 0x00, 0x00};
}

// ============================================================
// APDU Construction — Vendor Escape
// ============================================================

BYTEV PcscCommands::escape(const BYTEV& data) {
	BYTEV apdu{CLA, INS::ESCAPE, 0x00, 0x00, static_cast<BYTE>(data.size())};
	apdu.insert(apdu.end(), data.begin(), data.end());
	return apdu;
}

// ============================================================
// Response Evaluation — Exception-free
// ============================================================
Result<void, PcscError> PcscCommands::evaluateRead(const StatusWord& sw)
{
	if (sw.isSuccess()) return Result<void, PcscError>::Ok();
	else if (sw.isAuthSentinel())
		return Result<void, PcscError>::Err(PcscError::from(PcscErrorCode::AuthRequired, {}, sw));
	auto err = fromStatusWord(sw);
	if (err.code == PcscErrorCode::Unknown) err.code = PcscErrorCode::ReadFailed;
	return Result<void, PcscError>::Err(err);
}

Result<void, PcscError> PcscCommands::evaluateWrite(const StatusWord& sw)
{
	if (sw.isSuccess()) return Result<void, PcscError>::Ok();
	else if (sw.isAuthSentinel())
		return Result<void, PcscError>::Err(PcscError::from(PcscErrorCode::AuthRequired, {}, sw));

	auto err = fromStatusWord(sw);

	if (err.code == PcscErrorCode::Unknown) err.code = PcscErrorCode::WriteFailed;
	return Result<void, PcscError>::Err(err);
}

// değiştir: PcscError -> std::optional<PcscError>
Result<void, PcscError> PcscCommands::evaluateLoadKey(const StatusWord& sw) {
	if (sw.isSuccess()) return Result<void, PcscError>::Ok();
	auto err = fromStatusWord(sw);
	if (err.code == PcscErrorCode::AuthRequired || err.code == PcscErrorCode::Unknown)
		err.code = PcscErrorCode::LoadKeyFailed;
	return Result<void, PcscError>::Err(err);
}

Result<void, PcscError> PcscCommands::evaluateAuth(const StatusWord& sw) {
	if (sw.isSuccess()) return Result<void, PcscError>::Ok();
	else if (sw.isAuthSentinel())
		return Result<void, PcscError>::Err(PcscError::from(AuthError::AuthRequired, {}, sw));
	auto err = fromStatusWord(sw);
	if (err.code == PcscErrorCode::Unknown) err.code = PcscErrorCode::AuthFailed;
	return Result<void, PcscError>::Err(err);
}

// ============================================================
// Response Validation — Throwing (evaluate + throwIfError)
// ============================================================

void PcscCommands::checkRead(const StatusWord& sw) { evaluateRead(sw).error().throwIfError(); }

void PcscCommands::checkWrite(const StatusWord& sw) { evaluateWrite(sw).error().throwIfError(); }

void PcscCommands::checkLoadKey(const StatusWord& sw) {
	evaluateLoadKey(sw).error().throwIfError();
}

void PcscCommands::checkAuth(const StatusWord& sw) {
	evaluateAuth(sw).error().throwIfError();
}

void PcscCommands::checkExpected(const StatusWord& sw, uint16_t expected,
								 const std::string& context) {
	evaluateExpected(sw, expected, context).error().throwIfError();
}

Result<void, PcscError> PcscCommands::evaluateExpected(const StatusWord& sw, uint16_t expected,
													   const std::string& context)
{
	if (toCode(sw) == expected)
		return Result<void, PcscError>::Ok();
	StatusWord exp = fromCode(expected);
	std::string detail = context + ": beklenen " + exp.toHexFormatted()
		+ ", alinan " + describeStatus(sw);
	auto err = fromStatusWord(sw);
	err.detail = std::move(detail);
	return Result<void, PcscError>::Err(err);
}

// ============================================================
// Diagnostics — Komut tanılama
// ============================================================

std::string PcscCommands::describeCommand(BYTE ins) {
	switch (ins) {
	case INS::ESCAPE:            return "ESCAPE (vendor)";
	case INS::VERIFY:            return "VERIFY (PIN)";
	case INS::CHANGE_PIN:        return "CHANGE/UNBLOCK PIN";
	case INS::MANAGE_SESSION:    return "MANAGE SESSION";
	case INS::LOAD_KEY:          return "LOAD KEY";
	case INS::AUTH_GENERAL:      return "GENERAL AUTHENTICATE";
	case INS::AUTH_LEGACY:       return "AUTHENTICATE (Legacy)";
	case INS::READ_BINARY:       return "READ BINARY";
	case INS::READ_BINARY_ODD:   return "READ BINARY (odd)";
	case INS::GET_DATA:          return "GET DATA";
	case INS::UPDATE_BINARY:     return "UPDATE BINARY";
	case INS::UPDATE_BINARY_ODD: return "UPDATE BINARY (odd)";
	default:
		return "UNKNOWN (0x" + toHex(&ins, 1) + ")";
	}
}

// ============================================================
// Diagnostics — Durum kodu tanılama
// ============================================================

std::string PcscCommands::describeStatus(const StatusWord& sw) {
	uint16_t code = toCode(sw);

	switch (code) {
	case SW::SUCCESS:                  return sw.toHexFormatted() + " — Basarili";
	case SW::DESFIRE_SUCCESS:          return sw.toHexFormatted() + " — Basarili (DESFire)";
	case SW::AUTH_REQUIRED:            return sw.toHexFormatted() + " — Kimlik dogrulama gerekli";
	case SW::EXEC_ERROR_UNCHANGED:     return sw.toHexFormatted() + " — Calistirma hatasi (durum degismedi)";
	case SW::EXEC_ERROR_CHANGED:       return sw.toHexFormatted() + " — Calistirma hatasi (durum degisti)";
	case SW::WRONG_LENGTH:             return sw.toHexFormatted() + " — Yanlis uzunluk (Lc/Le)";
	case SW::CLA_FUNC_NOT_SUPPORTED:   return sw.toHexFormatted() + " — CLA fonksiyonlari desteklenmiyor";
	case SW::SECURITY_NOT_SATISFIED:   return sw.toHexFormatted() + " — Guvenlik durumu karsilanmadi";
	case SW::AUTH_BLOCKED:             return sw.toHexFormatted() + " — Kimlik dogrulama engellendi";
	case SW::REF_DATA_INVALIDATED:     return sw.toHexFormatted() + " — Referans veri gecersiz kilinmis";
	case SW::CONDITIONS_NOT_SATISFIED: return sw.toHexFormatted() + " — Kullanim kosullari karsilanmadi";
	case SW::FUNC_NOT_SUPPORTED:       return sw.toHexFormatted() + " — Fonksiyon desteklenmiyor";
	case SW::FILE_NOT_FOUND:           return sw.toHexFormatted() + " — Dosya/uygulama bulunamadi";
	case SW::INCORRECT_P1P2:           return sw.toHexFormatted() + " — Hatali P1-P2";
	case SW::LC_INCONSISTENT:          return sw.toHexFormatted() + " — Lc, P1-P2 ile tutarsiz";
	case SW::WRONG_P1P2:              return sw.toHexFormatted() + " — Hatali P1-P2 (aralik disi)";
	case SW::INS_NOT_SUPPORTED:        return sw.toHexFormatted() + " — INS desteklenmiyor";
	case SW::CLA_NOT_SUPPORTED:        return sw.toHexFormatted() + " — CLA desteklenmiyor";
	case SW::NO_PRECISE_DIAGNOSIS:     return sw.toHexFormatted() + " — Tani yok (genel hata)";
	default: break;
	}

	// Değişken SW2 kontrolü
	if (sw.sw1 == SW::WRONG_LE_SW1) {
		return sw.toHexFormatted() + " — Yanlis Le (dogru: " +
			   std::to_string(static_cast<int>(sw.sw2)) + ")";
	}
	if (sw.sw1 == SW::VERIFY_FAIL_SW1 && (sw.sw2 & 0xC0) == 0xC0) {
		int retries = sw.sw2 & 0x0F;
		return sw.toHexFormatted() + " — PIN dogrulama basarisiz (" +
			   std::to_string(retries) + " deneme kaldi)";
	}

	return sw.toHexFormatted() + " — Bilinmeyen durum kodu";
}

std::string PcscCommands::describeStatus(uint16_t code) {
	return describeStatus(fromCode(code));
}
