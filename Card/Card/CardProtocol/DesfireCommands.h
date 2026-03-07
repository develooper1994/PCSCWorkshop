#ifndef DESFIRE_COMMANDS_H
#define DESFIRE_COMMANDS_H

#include "CardDataTypes.h"
#include "../CardModel/DesfireMemoryLayout.h"
#include <vector>
#include <functional>

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
    // Transmit callback type
    using TransmitFn = std::function<BYTEV(const BYTEV&)>;

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

    // ── Multi-frame receive ─────────────────────────────────────────────────

    // Send command, then keep requesting additional frames until complete.
    // Returns concatenated data from all frames.
    static BYTEV transceive(const TransmitFn& transmit, const BYTEV& cmd);

    // ── High-level Parsing ──────────────────────────────────────────────────

    // Parse GetVersion 3-part response into DesfireVersionInfo
    static DesfireVersionInfo parseGetVersion(const TransmitFn& transmit);

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

#endif // DESFIRE_COMMANDS_H
