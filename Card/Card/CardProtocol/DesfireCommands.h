#ifndef DESFIRE_COMMANDS_H
#define DESFIRE_COMMANDS_H

#include "CardDataTypes.h"
#include "Result.h"
#include "../CardModel/DesfireMemoryLayout.h"
#include <vector>
#include <cstring>

// ════════════════════════════════════════════════════════════════════════════════
// DesfireCommands — DESFire Native APDU Construction & Response Parsing
// ════════════════════════════════════════════════════════════════════════════════
//
// ISO 7816-4 wrapped DESFire native komutları.
// I/O yapmaz — APDU üretir, response parse eder.
//
// APDU yapısı (ISO wrapped):
//   CLA=0x90, INS=cmd, P1=0x00, P2=0x00, Lc=len, Data..., Le=0x00
//
// Response yapısı:
//   [Data...] [SW1=0x91] [SW2]
//   SW2: 0x00=OK, 0xAF=more data, 0xAE=auth error, etc.
//
// ════════════════════════════════════════════════════════════════════════════════

class DesfireCommands {
public:
    // ── Status codes ────────────────────────────────────────────────────────

    static constexpr BYTE SW1      = 0x91;
    static constexpr BYTE OK       = 0x00;
    static constexpr BYTE MORE     = 0xAF;   // Additional frame available
    static constexpr BYTE AUTH_ERR = 0xAE;
    static constexpr BYTE PERM_ERR = 0x9D;
    static constexpr BYTE NO_APP   = 0xA0;   // Application not found
    static constexpr BYTE NO_FILE  = 0xF0;   // File not found

    // ── APDU Construction ───────────────────────────────────────────────────

    // Generic ISO-wrapped command: 90 INS 00 00 [Lc data...] 00
    static BYTEV wrapCommand(BYTE ins, const BYTEV& data = {});

    // SelectApplication: INS=0x5A, data=AID(3 bytes)
    static BYTEV selectApplication(const DesfireAID& aid);

    // GetVersion: INS=0x60 (3-part response, use with getMoreData)
    static BYTEV getVersion();

    // GetApplicationIDs: INS=0x6A
    static BYTEV getApplicationIDs();

    // GetFileIDs: INS=0x6F
    static BYTEV getFileIDs();

    // GetFileSettings: INS=0xF5, data=fileNo(1 byte)
    static BYTEV getFileSettings(BYTE fileNo);

    // ReadData: INS=0xBD, data=fileNo(1) + offset(3 LE) + length(3 LE)
    static BYTEV readData(BYTE fileNo, uint32_t offset, uint32_t length);

    // WriteData: INS=0x3D, data=fileNo(1) + offset(3 LE) + length(3 LE) + data
    static BYTEV writeData(BYTE fileNo, uint32_t offset, const BYTEV& payload);

    // GetValue: INS=0x6C, data=fileNo(1)
    static BYTEV getValue(BYTE fileNo);

    // GetFreeMemory: INS=0x6E
    static BYTEV getFreeMemory();

    // AdditionalFrame (get more data): INS=0xAF
    static BYTEV additionalFrame();

    // ── Application Management ──────────────────────────────────────────────

    // CreateApplication: INS=0xCA
    //   data = AID(3) + keySettings(1) + maxKeys(1)
    //   maxKeys alt 4 bit = key count, üst 4 bit = key type flag
    //     0x00–0x0E = DES/2K3DES, 0x40 = 3K3DES, 0x80 = AES
    static BYTEV createApplication(const DesfireAID& aid, BYTE keySettings,
                                    BYTE maxKeys, DesfireKeyType keyType);

    // DeleteApplication: INS=0xDA, data=AID(3)
    static BYTEV deleteApplication(const DesfireAID& aid);

    // ── File Management ─────────────────────────────────────────────────────

    // CreateStdDataFile: INS=0xCD
    //   fileNo(1) + commMode(1) + accessRights(2) + fileSize(3 LE)
    static BYTEV createStdDataFile(BYTE fileNo, DesfireCommMode comm,
                                    const DesfireAccessRights& access,
                                    uint32_t fileSize);

    // CreateBackupDataFile: INS=0xCB (same structure as StdData)
    static BYTEV createBackupDataFile(BYTE fileNo, DesfireCommMode comm,
                                       const DesfireAccessRights& access,
                                       uint32_t fileSize);

    // CreateValueFile: INS=0xCC
    //   fileNo(1) + commMode(1) + accessRights(2) + lowerLimit(4 LE) +
    //   upperLimit(4 LE) + value(4 LE) + limitedCredit(1)
    static BYTEV createValueFile(BYTE fileNo, DesfireCommMode comm,
                                  const DesfireAccessRights& access,
                                  int32_t lowerLimit, int32_t upperLimit,
                                  int32_t value, bool limitedCredit);

    // CreateLinearRecordFile: INS=0xC1
    //   fileNo(1) + commMode(1) + accessRights(2) + recordSize(3 LE) + maxRecords(3 LE)
    static BYTEV createLinearRecordFile(BYTE fileNo, DesfireCommMode comm,
                                         const DesfireAccessRights& access,
                                         uint32_t recordSize, uint32_t maxRecords);

    // CreateCyclicRecordFile: INS=0xC0 (same structure as Linear)
    static BYTEV createCyclicRecordFile(BYTE fileNo, DesfireCommMode comm,
                                         const DesfireAccessRights& access,
                                         uint32_t recordSize, uint32_t maxRecords);

    // DeleteFile: INS=0xDF, data=fileNo(1)
    static BYTEV deleteFile(BYTE fileNo);

    // ── Key Management ──────────────────────────────────────────────────────

    // ChangeKey: INS=0xC4
    //   data = keyNo(1) + cryptogram (encrypted new key XOR old key + CRC)
    //   Cryptogram hazırlığı DesfireCrypto'da yapılır
    static BYTEV changeKey(BYTE keyNo, const BYTEV& cryptogram);

    // GetKeySettings: INS=0x45
    static BYTEV getKeySettings();

    // ChangeKeySettings: INS=0x54, data=encrypted new settings
    static BYTEV changeKeySettings(const BYTEV& encSettings);

    // GetKeyVersion: INS=0x64, data=keyNo(1)
    static BYTEV getKeyVersion(BYTE keyNo);

    // ── Transaction ─────────────────────────────────────────────────────────

    // Credit: INS=0x0C, data=fileNo(1) + value(4 LE)
    static BYTEV credit(BYTE fileNo, int32_t value);

    // Debit: INS=0xDC, data=fileNo(1) + value(4 LE)
    static BYTEV debit(BYTE fileNo, int32_t value);

    // CommitTransaction: INS=0xC7
    static BYTEV commitTransaction();

    // AbortTransaction: INS=0xA7
    static BYTEV abortTransaction();

    // ── Card-level ──────────────────────────────────────────────────────────

    // FormatPICC: INS=0xFC (requires auth to PICC master key)
    static BYTEV formatPICC();

    // ── Record File Operations ──────────────────────────────────────────────

    // ReadRecords: INS=0xBB
    //   data = fileNo(1) + offset(3 LE) + count(3 LE)
    //   offset: starting record number (0-based)
    //   count:  number of records to read (0 = all from offset)
    static BYTEV readRecords(BYTE fileNo, uint32_t offset, uint32_t count);

    // AppendRecord: INS=0x3B
    //   data = fileNo(1) + offset(3 LE, must be 0) + length(3 LE) + recordData
    static BYTEV appendRecord(BYTE fileNo, const BYTEV& recordData);

    // ── Response Parsing ────────────────────────────────────────────────────

    // Extract SW2 from response
    static BYTE statusCode(const BYTEV& response);

    // Check if response is success (SW=9100)
    static bool isOK(const BYTEV& response);

    // Check if more data available (SW=91AF)
    static bool hasMore(const BYTEV& response);

    // Extract data portion (everything except last 2 SW bytes)
    static BYTEV extractData(const BYTEV& response);

    // Validate response and throw on error
    static void checkResponse(const BYTEV& response, const char* context);

    // ── Response Evaluation — Exception-free ────────────────────────────────

    static PcscError evaluateResponse(const BYTEV& response);

    // ── Multi-frame receive ─────────────────────────────────────────────────

    template<typename TransmitFn>
    static BYTEV transceive(TransmitFn&& transmit, const BYTEV& cmd);

    template<typename TryTransmitFn>
    static Result<BYTEV> tryTransceive(TryTransmitFn&& transmit, const BYTEV& cmd);

    // ── High-level Parsing ──────────────────────────────────────────────────

    template<typename TransmitFn>
    static DesfireVersionInfo parseGetVersion(TransmitFn&& transmit);

    template<typename TryTransmitFn>
    static Result<DesfireVersionInfo> tryParseGetVersion(TryTransmitFn&& transmit);

    // Parse GetApplicationIDs response → vector of AIDs
    static std::vector<DesfireAID> parseApplicationIDs(const BYTEV& data);

    // Parse GetFileIDs response → vector of file numbers
    static std::vector<BYTE> parseFileIDs(const BYTEV& data);

    // Parse GetFileSettings response → DesfireFileSettings
    static DesfireFileSettings parseFileSettings(const BYTEV& data);

    // Parse GetFreeMemory response → bytes
    static size_t parseFreeMemory(const BYTEV& data);

    // Parse 3-byte little-endian uint
    static uint32_t readLE24(const BYTE* p);

    // Write 3-byte little-endian uint
    static void writeLE24(BYTE* p, uint32_t v);

private:
    DesfireCommands() = delete;
};

// ════════════════════════════════════════════════════════════════════════════════
// Template implementations
// ════════════════════════════════════════════════════════════════════════════════

template<typename TransmitFn>
BYTEV DesfireCommands::transceive(TransmitFn&& transmit, const BYTEV& cmd) {
    auto tryTx = [&](const BYTEV& apdu) -> Result<BYTEV> {
        return {transmit(apdu), PcscError{}};
    };
    return tryTransceive(tryTx, cmd).unwrap();
}

template<typename TryTransmitFn>
Result<BYTEV> DesfireCommands::tryTransceive(TryTransmitFn&& transmit, const BYTEV& cmd) {
    auto txResult = transmit(cmd);
    if (!txResult) return {BYTEV{}, txResult.error()};
    auto err = evaluateResponse(txResult.unwrap());
    if (!err.ok()) return {BYTEV{}, err};
    BYTEV result = extractData(txResult.unwrap());
    while (hasMore(txResult.unwrap())) {
        txResult = transmit(additionalFrame());
        if (!txResult) return {BYTEV{}, txResult.error()};
        err = evaluateResponse(txResult.unwrap());
        if (!err.ok()) return {BYTEV{}, err};
        BYTEV chunk = extractData(txResult.unwrap());
        result.insert(result.end(), chunk.begin(), chunk.end());
    }
    return {std::move(result), PcscError{}};
}

template<typename TransmitFn>
DesfireVersionInfo DesfireCommands::parseGetVersion(TransmitFn&& transmit) {
    auto tryTx = [&](const BYTEV& apdu) -> Result<BYTEV> {
        return {transmit(apdu), PcscError{}};
    };
    return tryParseGetVersion(tryTx).unwrap();
}

template<typename TryTransmitFn>
Result<DesfireVersionInfo> DesfireCommands::tryParseGetVersion(TryTransmitFn&& transmit) {
    DesfireVersionInfo vi;
    auto r1 = transmit(getVersion());
    if (!r1) return {vi, r1.error()};
    auto e1 = evaluateResponse(r1.unwrap());
    if (!e1.ok()) return {vi, e1};
    BYTEV d1 = extractData(r1.unwrap());
    if (d1.size() >= 7) {
        vi.hwVendorID = d1[0]; vi.hwType = d1[1]; vi.hwSubType = d1[2];
        vi.hwMajorVer = d1[3]; vi.hwMinorVer = d1[4];
        vi.hwStorageSize = d1[5]; vi.hwProtocol = d1[6];
    }
    if (!hasMore(r1.unwrap())) return {vi, PcscError{}};
    auto r2 = transmit(additionalFrame());
    if (!r2) return {vi, r2.error()};
    auto e2 = evaluateResponse(r2.unwrap());
    if (!e2.ok()) return {vi, e2};
    BYTEV d2 = extractData(r2.unwrap());
    if (d2.size() >= 7) {
        vi.swVendorID = d2[0]; vi.swType = d2[1]; vi.swSubType = d2[2];
        vi.swMajorVer = d2[3]; vi.swMinorVer = d2[4];
        vi.swStorageSize = d2[5]; vi.swProtocol = d2[6];
    }
    if (!hasMore(r2.unwrap())) return {vi, PcscError{}};
    auto r3 = transmit(additionalFrame());
    if (!r3) return {vi, r3.error()};
    auto e3 = evaluateResponse(r3.unwrap());
    if (!e3.ok()) return {vi, e3};
    BYTEV d3 = extractData(r3.unwrap());
    if (d3.size() >= 14) {
        std::memcpy(vi.uid, d3.data(), 7);
        std::memcpy(vi.batchNo, d3.data() + 7, 5);
        vi.productionWeek = d3[12];
        vi.productionYear = d3[13];
    }
    return {vi, PcscError{}};
}

#endif // DESFIRE_COMMANDS_H
