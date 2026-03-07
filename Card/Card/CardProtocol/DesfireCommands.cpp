#include "DesfireCommands.h"
#include <stdexcept>
#include <sstream>
#include <cstring>

// ════════════════════════════════════════════════════════════════════════════════
// Little-endian helpers
// ════════════════════════════════════════════════════════════════════════════════

uint32_t DesfireCommands::readLE24(const BYTE* p) {
    return static_cast<uint32_t>(p[0])
         | (static_cast<uint32_t>(p[1]) << 8)
         | (static_cast<uint32_t>(p[2]) << 16);
}

void DesfireCommands::writeLE24(BYTE* p, uint32_t v) {
    p[0] = static_cast<BYTE>(v & 0xFF);
    p[1] = static_cast<BYTE>((v >> 8) & 0xFF);
    p[2] = static_cast<BYTE>((v >> 16) & 0xFF);
}

// ════════════════════════════════════════════════════════════════════════════════
// APDU Construction
// ════════════════════════════════════════════════════════════════════════════════

BYTEV DesfireCommands::wrapCommand(BYTE ins, const BYTEV& data) {
    BYTEV apdu;
    apdu.push_back(0x90);   // CLA
    apdu.push_back(ins);    // INS
    apdu.push_back(0x00);   // P1
    apdu.push_back(0x00);   // P2
    if (!data.empty()) {
        apdu.push_back(static_cast<BYTE>(data.size()));  // Lc
        apdu.insert(apdu.end(), data.begin(), data.end());
    }
    apdu.push_back(0x00);   // Le
    return apdu;
}

BYTEV DesfireCommands::selectApplication(const DesfireAID& aid) {
    return wrapCommand(0x5A, {aid.aid[0], aid.aid[1], aid.aid[2]});
}

BYTEV DesfireCommands::getVersion() {
    return wrapCommand(0x60);
}

BYTEV DesfireCommands::getApplicationIDs() {
    return wrapCommand(0x6A);
}

BYTEV DesfireCommands::getFileIDs() {
    return wrapCommand(0x6F);
}

BYTEV DesfireCommands::getFileSettings(BYTE fileNo) {
    return wrapCommand(0xF5, {fileNo});
}

BYTEV DesfireCommands::readData(BYTE fileNo, uint32_t offset, uint32_t length) {
    BYTEV data(7);
    data[0] = fileNo;
    writeLE24(&data[1], offset);
    writeLE24(&data[4], length);
    return wrapCommand(0xBD, data);
}

BYTEV DesfireCommands::writeData(BYTE fileNo, uint32_t offset, const BYTEV& payload) {
    BYTEV data;
    data.push_back(fileNo);
    BYTE off[3], len[3];
    writeLE24(off, offset);
    writeLE24(len, static_cast<uint32_t>(payload.size()));
    data.insert(data.end(), off, off + 3);
    data.insert(data.end(), len, len + 3);
    data.insert(data.end(), payload.begin(), payload.end());
    return wrapCommand(0x3D, data);
}

BYTEV DesfireCommands::getValue(BYTE fileNo) {
    return wrapCommand(0x6C, {fileNo});
}

BYTEV DesfireCommands::getFreeMemory() {
    return wrapCommand(0x6E);
}

BYTEV DesfireCommands::additionalFrame() {
    return wrapCommand(0xAF);
}

// ════════════════════════════════════════════════════════════════════════════════
// Application Management
// ════════════════════════════════════════════════════════════════════════════════

BYTEV DesfireCommands::createApplication(const DesfireAID& aid, BYTE keySettings,
                                          BYTE maxKeys, DesfireKeyType keyType) {
    BYTE keyTypeBits = 0;
    switch (keyType) {
        case DesfireKeyType::AES128:  keyTypeBits = 0x80; break;
        case DesfireKeyType::ThreeDES:keyTypeBits = 0x40; break;
        default: keyTypeBits = 0x00; break;  // DES / 2K3DES
    }
    BYTE maxKeysField = (maxKeys & 0x0F) | keyTypeBits;
    return wrapCommand(0xCA, {aid.aid[0], aid.aid[1], aid.aid[2],
                              keySettings, maxKeysField});
}

BYTEV DesfireCommands::deleteApplication(const DesfireAID& aid) {
    return wrapCommand(0xDA, {aid.aid[0], aid.aid[1], aid.aid[2]});
}

// ════════════════════════════════════════════════════════════════════════════════
// File Management
// ════════════════════════════════════════════════════════════════════════════════

static BYTEV buildFileHeader(BYTE fileNo, DesfireCommMode comm,
                              const DesfireAccessRights& access) {
    auto ar = access.encode();
    return {fileNo, static_cast<BYTE>(comm), ar[0], ar[1]};
}

BYTEV DesfireCommands::createStdDataFile(BYTE fileNo, DesfireCommMode comm,
                                          const DesfireAccessRights& access,
                                          uint32_t fileSize) {
    BYTEV data = buildFileHeader(fileNo, comm, access);
    BYTE sz[3]; writeLE24(sz, fileSize);
    data.insert(data.end(), sz, sz + 3);
    return wrapCommand(0xCD, data);
}

BYTEV DesfireCommands::createBackupDataFile(BYTE fileNo, DesfireCommMode comm,
                                              const DesfireAccessRights& access,
                                              uint32_t fileSize) {
    BYTEV data = buildFileHeader(fileNo, comm, access);
    BYTE sz[3]; writeLE24(sz, fileSize);
    data.insert(data.end(), sz, sz + 3);
    return wrapCommand(0xCB, data);
}

BYTEV DesfireCommands::createValueFile(BYTE fileNo, DesfireCommMode comm,
                                        const DesfireAccessRights& access,
                                        int32_t lowerLimit, int32_t upperLimit,
                                        int32_t value, bool limitedCredit) {
    BYTEV data = buildFileHeader(fileNo, comm, access);
    // 4-byte LE for each value
    auto pushLE32 = [&](int32_t v) {
        uint32_t u = static_cast<uint32_t>(v);
        data.push_back(static_cast<BYTE>(u & 0xFF));
        data.push_back(static_cast<BYTE>((u >> 8) & 0xFF));
        data.push_back(static_cast<BYTE>((u >> 16) & 0xFF));
        data.push_back(static_cast<BYTE>((u >> 24) & 0xFF));
    };
    pushLE32(lowerLimit);
    pushLE32(upperLimit);
    pushLE32(value);
    data.push_back(limitedCredit ? 0x01 : 0x00);
    return wrapCommand(0xCC, data);
}

BYTEV DesfireCommands::createLinearRecordFile(BYTE fileNo, DesfireCommMode comm,
                                               const DesfireAccessRights& access,
                                               uint32_t recordSize, uint32_t maxRecords) {
    BYTEV data = buildFileHeader(fileNo, comm, access);
    BYTE rs[3], mr[3];
    writeLE24(rs, recordSize);
    writeLE24(mr, maxRecords);
    data.insert(data.end(), rs, rs + 3);
    data.insert(data.end(), mr, mr + 3);
    return wrapCommand(0xC1, data);
}

BYTEV DesfireCommands::createCyclicRecordFile(BYTE fileNo, DesfireCommMode comm,
                                               const DesfireAccessRights& access,
                                               uint32_t recordSize, uint32_t maxRecords) {
    BYTEV data = buildFileHeader(fileNo, comm, access);
    BYTE rs[3], mr[3];
    writeLE24(rs, recordSize);
    writeLE24(mr, maxRecords);
    data.insert(data.end(), rs, rs + 3);
    data.insert(data.end(), mr, mr + 3);
    return wrapCommand(0xC0, data);
}

BYTEV DesfireCommands::deleteFile(BYTE fileNo) {
    return wrapCommand(0xDF, {fileNo});
}

// ════════════════════════════════════════════════════════════════════════════════
// Key Management
// ════════════════════════════════════════════════════════════════════════════════

BYTEV DesfireCommands::changeKey(BYTE keyNo, const BYTEV& cryptogram) {
    BYTEV data;
    data.push_back(keyNo);
    data.insert(data.end(), cryptogram.begin(), cryptogram.end());
    return wrapCommand(0xC4, data);
}

BYTEV DesfireCommands::getKeySettings() {
    return wrapCommand(0x45);
}

BYTEV DesfireCommands::changeKeySettings(const BYTEV& encSettings) {
    return wrapCommand(0x54, encSettings);
}

BYTEV DesfireCommands::getKeyVersion(BYTE keyNo) {
    return wrapCommand(0x64, {keyNo});
}

// ════════════════════════════════════════════════════════════════════════════════
// Transaction
// ════════════════════════════════════════════════════════════════════════════════

BYTEV DesfireCommands::credit(BYTE fileNo, int32_t value) {
    uint32_t u = static_cast<uint32_t>(value);
    return wrapCommand(0x0C, {
        fileNo,
        static_cast<BYTE>(u & 0xFF),
        static_cast<BYTE>((u >> 8) & 0xFF),
        static_cast<BYTE>((u >> 16) & 0xFF),
        static_cast<BYTE>((u >> 24) & 0xFF)
    });
}

BYTEV DesfireCommands::debit(BYTE fileNo, int32_t value) {
    uint32_t u = static_cast<uint32_t>(value);
    return wrapCommand(0xDC, {
        fileNo,
        static_cast<BYTE>(u & 0xFF),
        static_cast<BYTE>((u >> 8) & 0xFF),
        static_cast<BYTE>((u >> 16) & 0xFF),
        static_cast<BYTE>((u >> 24) & 0xFF)
    });
}

BYTEV DesfireCommands::commitTransaction() {
    return wrapCommand(0xC7);
}

BYTEV DesfireCommands::abortTransaction() {
    return wrapCommand(0xA7);
}

// ════════════════════════════════════════════════════════════════════════════════
// Card-level
// ════════════════════════════════════════════════════════════════════════════════

BYTEV DesfireCommands::formatPICC() {
    return wrapCommand(0xFC);
}

// ════════════════════════════════════════════════════════════════════════════════
// Record File Operations
// ════════════════════════════════════════════════════════════════════════════════

BYTEV DesfireCommands::readRecords(BYTE fileNo, uint32_t offset, uint32_t count) {
    BYTEV data;
    data.push_back(fileNo);
    BYTE off[3], cnt[3];
    writeLE24(off, offset);
    writeLE24(cnt, count);
    data.insert(data.end(), off, off + 3);
    data.insert(data.end(), cnt, cnt + 3);
    return wrapCommand(0xBB, data);
}

BYTEV DesfireCommands::appendRecord(BYTE fileNo, const BYTEV& recordData) {
    BYTEV data;
    data.push_back(fileNo);
    BYTE off[3] = {0, 0, 0};  // offset must be 0
    data.insert(data.end(), off, off + 3);
    BYTE len[3];
    writeLE24(len, static_cast<uint32_t>(recordData.size()));
    data.insert(data.end(), len, len + 3);
    data.insert(data.end(), recordData.begin(), recordData.end());
    return wrapCommand(0x3B, data);
}

// ════════════════════════════════════════════════════════════════════════════════
// Response Parsing
// ════════════════════════════════════════════════════════════════════════════════

BYTE DesfireCommands::statusCode(const BYTEV& response) {
    if (response.size() < 2) return 0xFF;
    return response[response.size() - 1];
}

bool DesfireCommands::isOK(const BYTEV& response) {
    if (response.size() < 2) return false;
    return response[response.size() - 2] == SW1 && response[response.size() - 1] == OK;
}

bool DesfireCommands::hasMore(const BYTEV& response) {
    if (response.size() < 2) return false;
    return response[response.size() - 2] == SW1 && response[response.size() - 1] == MORE;
}

BYTEV DesfireCommands::extractData(const BYTEV& response) {
    if (response.size() < 2) return {};
    return BYTEV(response.begin(), response.end() - 2);
}

void DesfireCommands::checkResponse(const BYTEV& response, const char* context) {
    if (response.size() < 2) {
        std::ostringstream ss;
        ss << context << ": response too short (" << response.size() << " bytes)";
        throw std::runtime_error(ss.str());
    }
    BYTE sw1 = response[response.size() - 2];
    BYTE sw2 = response[response.size() - 1];
    if (sw1 == SW1 && (sw2 == OK || sw2 == MORE)) return;

    std::ostringstream ss;
    ss << context << ": DESFire error 0x" << std::hex << (int)sw1 << (int)sw2;
    throw std::runtime_error(ss.str());
}

// ════════════════════════════════════════════════════════════════════════════════
// Multi-frame receive
// ════════════════════════════════════════════════════════════════════════════════

BYTEV DesfireCommands::transceive(const TransmitFn& transmit, const BYTEV& cmd) {
    BYTEV resp = transmit(cmd);
    checkResponse(resp, "transceive");

    BYTEV result = extractData(resp);

    while (hasMore(resp)) {
        resp = transmit(additionalFrame());
        checkResponse(resp, "transceive(AF)");
        BYTEV chunk = extractData(resp);
        result.insert(result.end(), chunk.begin(), chunk.end());
    }

    return result;
}

// ════════════════════════════════════════════════════════════════════════════════
// High-level Parsing
// ════════════════════════════════════════════════════════════════════════════════

DesfireVersionInfo DesfireCommands::parseGetVersion(const TransmitFn& transmit) {
    DesfireVersionInfo vi;

    // Part 1: Hardware info (7 bytes) — response to GetVersion
    BYTEV resp1 = transmit(getVersion());
    checkResponse(resp1, "GetVersion part1");
    BYTEV d1 = extractData(resp1);
    if (d1.size() >= 7) {
        vi.hwVendorID    = d1[0];
        vi.hwType        = d1[1];
        vi.hwSubType     = d1[2];
        vi.hwMajorVer    = d1[3];
        vi.hwMinorVer    = d1[4];
        vi.hwStorageSize = d1[5];
        vi.hwProtocol    = d1[6];
    }

    if (!hasMore(resp1)) return vi;

    // Part 2: Software info (7 bytes) — response to AdditionalFrame
    BYTEV resp2 = transmit(additionalFrame());
    checkResponse(resp2, "GetVersion part2");
    BYTEV d2 = extractData(resp2);
    if (d2.size() >= 7) {
        vi.swVendorID    = d2[0];
        vi.swType        = d2[1];
        vi.swSubType     = d2[2];
        vi.swMajorVer    = d2[3];
        vi.swMinorVer    = d2[4];
        vi.swStorageSize = d2[5];
        vi.swProtocol    = d2[6];
    }

    if (!hasMore(resp2)) return vi;

    // Part 3: UID + batch + production (14 bytes)
    BYTEV resp3 = transmit(additionalFrame());
    checkResponse(resp3, "GetVersion part3");
    BYTEV d3 = extractData(resp3);
    if (d3.size() >= 14) {
        std::memcpy(vi.uid,     d3.data(),     7);
        std::memcpy(vi.batchNo, d3.data() + 7, 5);
        vi.productionWeek = d3[12];
        vi.productionYear = d3[13];
    }

    return vi;
}

std::vector<DesfireAID> DesfireCommands::parseApplicationIDs(const BYTEV& data) {
    std::vector<DesfireAID> aids;
    for (size_t i = 0; i + 2 < data.size(); i += 3) {
        DesfireAID aid;
        aid.aid[0] = data[i];
        aid.aid[1] = data[i + 1];
        aid.aid[2] = data[i + 2];
        aids.push_back(aid);
    }
    return aids;
}

std::vector<BYTE> DesfireCommands::parseFileIDs(const BYTEV& data) {
    return std::vector<BYTE>(data.begin(), data.end());
}

DesfireFileSettings DesfireCommands::parseFileSettings(const BYTEV& data) {
    DesfireFileSettings fs;
    if (data.size() < 7) throw std::runtime_error("FileSettings too short");

    fs.fileType = static_cast<DesfireFileType>(data[0]);
    fs.commMode = static_cast<DesfireCommMode>(data[1]);
    fs.access   = DesfireAccessRights::decode(data[2], data[3]);

    switch (fs.fileType) {
        case DesfireFileType::StandardData:
        case DesfireFileType::BackupData:
            if (data.size() >= 7)
                fs.standard.fileSize = readLE24(&data[4]);
            break;

        case DesfireFileType::Value:
            if (data.size() >= 17) {
                std::memcpy(&fs.value.lowerLimit, &data[4], 4);
                std::memcpy(&fs.value.upperLimit, &data[8], 4);
                std::memcpy(&fs.value.value,      &data[12], 4);
                fs.value.limitedCreditEnabled = (data[16] != 0);
            }
            break;

        case DesfireFileType::LinearRecord:
        case DesfireFileType::CyclicRecord:
            if (data.size() >= 13) {
                fs.record.recordSize     = readLE24(&data[4]);
                fs.record.maxRecords     = readLE24(&data[7]);
                fs.record.currentRecords = readLE24(&data[10]);
            }
            break;
    }
    return fs;
}

size_t DesfireCommands::parseFreeMemory(const BYTEV& data) {
    if (data.size() < 3) throw std::runtime_error("FreeMem too short");
    return static_cast<size_t>(readLE24(data.data()));
}
