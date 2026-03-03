#include "cardreader.h"
#include <winscard.h>
#include <windows.h>
#include <bcrypt.h>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <cwchar>
#include <cstring>

#ifndef BCRYPT_SUCCESS
#define BCRYPT_SUCCESS(Status) (((LONG)(Status)) >= 0)
#endif

// 3DES key: 24 bytes (three 8-byte DES keys)
static const BYTE DES3_KEY[24] = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF,
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF
};

// IV for CBC mode: 8 bytes
static const BYTE DES3_IV[8] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

// AES-128 key: 16 bytes
static const BYTE AES_KEY[16] = {
    0x2B, 0x7E, 0x15, 0x16, 0x28, 0xAE, 0xD2, 0xA6,
    0xAB, 0xF7, 0x15, 0x88, 0x09, 0xCF, 0x4F, 0x3C
};

// IV for AES CBC mode: 16 bytes
static const BYTE AES_IV[16] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F
};

CardReader::CardReader(QObject *parent)
    : QObject(parent)
    , m_cardUID("")
    , m_cardType(MifareDesfire)
    , m_busy(false)
    , m_ultralightRawData("")
    , m_ultralightAESData("")
    , m_ultralightRestoredData("")
{
    m_pollTimer = new QTimer(this);
    m_pollTimer->setInterval(1000); // 1 second
    m_pollTimer->setSingleShot(false);
    connect(m_pollTimer, &QTimer::timeout, this, &CardReader::onPollTimer);
}

void CardReader::appendLog(const QString &entry)
{
    // Clear the log if it exceeds 20 lines
    if (m_cardDataLog.count('\n') >= 20)
    {
        m_cardDataLog.clear();
    }

    if (!m_cardDataLog.isEmpty())
        m_cardDataLog.append("\n");

    m_cardDataLog.append(entry);
    emit cardDataLogChanged();
}

void CardReader::setLog(const QString &log)
{
    m_cardDataLog = log;
    emit cardDataLogChanged();
}

QString CardReader::bytesToHex(const unsigned char *data, unsigned long length)
{
    std::stringstream ss;
    for (unsigned long i = 0; i < length; i++)
    {
        ss << std::hex << std::uppercase << std::setfill('0') << std::setw(2)
           << static_cast<int>(data[i]);
        if (i < length - 1)
            ss << " ";
    }

    return QString::fromStdString(ss.str());
}

QString CardReader::formatAsPages(const unsigned char *data, unsigned long length)
{
    std::stringstream ss;

    unsigned long numPages = (length + 3) / 4; // Round up
    
    for (unsigned long page = 0; page < numPages; page++)
    {
        unsigned long offset = page * 4;
        unsigned long pageLength = (offset + 4 <= length) ? 4 : (length - offset);
        
        ss << "Page " << std::setfill(' ') << std::setw(2) << page << " -->\t";
        

        for (unsigned long i = 0; i < pageLength; i++)
        {
            ss << std::hex << std::uppercase << std::setfill('0') << std::setw(2)
               << static_cast<int>(data[offset + i]);
            if (i < pageLength - 1)
                ss << " ";
        }
        
        if (page < numPages - 1)
            ss << "\n";
    }
    
    return QString::fromStdString(ss.str());
}

void CardReader::setCardType(int type)
{
    if (m_cardType != type)
    {
        m_cardType = type;
        emit cardTypeChanged();

        // Clear previous state
        if (m_cardUID != "")
        {
            m_cardUID = "";
            emit cardUIDChanged();
        }
        m_cardDataLog = "";
        emit cardDataLogChanged();

        // Polling only for DESFire
        if (type == MifareDesfire)
        {
            startPolling();
        }
        else
        {
            stopPolling();
        }
    }
}

void CardReader::readCard()
{
    if (m_busy)
        return;

    if (m_cardType == MifareUltralight)
    {
        readUltralightCard();
    }
    else if (m_cardType == MifareClassic1K)
    {
        readMifareClassicCard();
    }
    else
    {
        readDesfireCard();
    }
}

void CardReader::readDesfireCard()
{
    m_busy = true;

    SCARDCONTEXT hContext = 0;
    LONG lRetValue = SCardEstablishContext(SCARD_SCOPE_USER, nullptr, nullptr, &hContext);

    if (lRetValue != SCARD_S_SUCCESS)
    {
        if (m_cardUID != "")
        {
            m_cardUID = "";
            emit cardUIDChanged();
        }
        m_busy = false;
        return;
    }

    DWORD dwReaders = 0;
    lRetValue = SCardListReadersW(hContext, nullptr, nullptr, &dwReaders);

    if (lRetValue != SCARD_S_SUCCESS || dwReaders == 0)
    {
        SCardReleaseContext(hContext);
        if (m_cardUID != "") {
            m_cardUID = "";
            emit cardUIDChanged();
        }
        m_busy = false;
        return;
    }

    LPWSTR mszReaders = (LPWSTR)malloc(dwReaders * sizeof(wchar_t));
    if (!mszReaders)
    {
        SCardReleaseContext(hContext);
        m_busy = false;
        return;
    }
    lRetValue = SCardListReadersW(hContext, nullptr, mszReaders, &dwReaders);

    if (lRetValue != SCARD_S_SUCCESS)
    {
        free(mszReaders);
        SCardReleaseContext(hContext);
        if (m_cardUID != "") {
            m_cardUID = "";
            emit cardUIDChanged();
        }
        m_busy = false;
        return;
    }

    // Find a contactless reader
    LPWSTR readerName = nullptr;
    LPWSTR currentReader = mszReaders;

    while (*currentReader != L'\0')
    {
        QString readerNameQStr = QString::fromWCharArray(currentReader);
        QString readerNameUpper = readerNameQStr.toUpper();

        if (readerNameUpper.contains("CONTACTLESS") ||
            readerNameUpper.contains("CL") ||
            readerNameUpper.contains("PROXIMITY") ||
            readerNameUpper.contains("NFC") ||
            readerNameUpper.contains("PICC") ||
            readerNameUpper.contains("RFID"))
        {
            readerName = currentReader;
            break;
        }

        currentReader += wcslen(currentReader) + 1;
    }

    if (readerName == nullptr)
    {
        readerName = mszReaders;
    }

    // Connect to the card
    SCARDHANDLE hCard = 0;
    DWORD dwActiveProtocol = 0;
    lRetValue = SCardConnectW(hContext, readerName, SCARD_SHARE_SHARED, SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1, &hCard, &dwActiveProtocol);

    if (lRetValue != SCARD_S_SUCCESS)
    {
        free(mszReaders);
        SCardReleaseContext(hContext);
        if (m_cardUID != "")
        {
            m_cardUID = "";
            emit cardUIDChanged();
        }
        m_busy = false;
        return;
    }

    // APDU: FF CA 00 00 00 (Get Data command for UID)
    BYTE apduGetUID[] = {0xFF, 0xCA, 0x00, 0x00, 0x00};
    BYTE response[256];
    DWORD responseLength = sizeof(response);

    appendLog("TO PICC        -->: " + bytesToHex(apduGetUID, sizeof(apduGetUID)));

    const SCARD_IO_REQUEST* pioSendPci = (dwActiveProtocol == SCARD_PROTOCOL_T0) ? SCARD_PCI_T0 : SCARD_PCI_T1;

    lRetValue = SCardTransmit(hCard, pioSendPci, apduGetUID, sizeof(apduGetUID), nullptr, response, &responseLength);

    if (lRetValue == SCARD_S_SUCCESS && responseLength >= 2)
    {
        appendLog("FROM PICC  -->: " + bytesToHex(response, responseLength));

        if (response[responseLength - 2] == 0x90 && response[responseLength - 1] == 0x00)
        {

            int uidLength = responseLength - 2;
            QString newUID = bytesToHex(response, uidLength);
            if (m_cardUID != newUID)
            {
                m_cardUID = newUID;
                emit cardUIDChanged();
                appendLog("\n****Card UID: " + newUID + "**********");
            }
        }
        else
        {
            appendLog("-- SW: " + bytesToHex(response + responseLength - 2, 2) + " (Error)");
            if (m_cardUID != "")
            {
                m_cardUID = "";
                emit cardUIDChanged();
            }
        }
    }
    else
    {
        appendLog("<< RECV: Transmit failed (0x" + QString::number(static_cast<unsigned long>(lRetValue), 16).toUpper() + ")");
        if (m_cardUID != "")
        {
            m_cardUID = "";
            emit cardUIDChanged();
        }
    }


    SCardDisconnect(hCard, SCARD_LEAVE_CARD);
    free(mszReaders);
    SCardReleaseContext(hContext);
    m_busy = false;
}

void CardReader::readMifareClassicCard()
{
    m_busy = true;

    // Clear the display
    setLog("");
    
    // Factory default key: FF FF FF FF FF FF
    const BYTE defaultKey[6] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
    //const BYTE defaultKey[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    SCARDCONTEXT hContext = 0;
    LONG lRetValue = SCardEstablishContext(SCARD_SCOPE_USER, nullptr, nullptr, &hContext);

    if (lRetValue != SCARD_S_SUCCESS)
    {
        if (m_cardUID != "")
        {
            m_cardUID = "";
            emit cardUIDChanged();
        }
        m_busy = false;
        return;
    }

    DWORD dwReaders = 0;
    lRetValue = SCardListReadersW(hContext, nullptr, nullptr, &dwReaders);

    if (lRetValue != SCARD_S_SUCCESS || dwReaders == 0)
    {
        SCardReleaseContext(hContext);
        if (m_cardUID != "")
        {
            m_cardUID = "";
            emit cardUIDChanged();
        }
        m_busy = false;
        return;
    }

    LPWSTR mszReaders = (LPWSTR)malloc(dwReaders * sizeof(wchar_t));
    if (!mszReaders)
    {
        SCardReleaseContext(hContext);
        m_busy = false;
        return;
    }
    lRetValue = SCardListReadersW(hContext, nullptr, mszReaders, &dwReaders);

    if (lRetValue != SCARD_S_SUCCESS)
    {
        free(mszReaders);
        SCardReleaseContext(hContext);
        if (m_cardUID != "")
        {
            m_cardUID = "";
            emit cardUIDChanged();
        }
        m_busy = false;
        return;
    }

    // Find a contactless reader
    LPWSTR readerName = nullptr;
    LPWSTR currentReader = mszReaders;

    while (*currentReader != L'\0') {
        QString readerNameQStr = QString::fromWCharArray(currentReader);
        QString readerNameUpper = readerNameQStr.toUpper();

        if (readerNameUpper.contains("CONTACTLESS") ||
            readerNameUpper.contains("CL") ||
            readerNameUpper.contains("PROXIMITY") ||
            readerNameUpper.contains("NFC") ||
            readerNameUpper.contains("PICC") ||
            readerNameUpper.contains("RFID"))
        {
            readerName = currentReader;
            break;
        }

        currentReader += wcslen(currentReader) + 1;
    }

    if (readerName == nullptr)
    {
        readerName = mszReaders;
    }

    // Connect to the card
    SCARDHANDLE hCard = 0;
    DWORD dwActiveProtocol = 0;
    lRetValue = SCardConnectW(hContext, readerName, SCARD_SHARE_SHARED, SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1, &hCard, &dwActiveProtocol);

    if (lRetValue != SCARD_S_SUCCESS)
    {
        free(mszReaders);
        SCardReleaseContext(hContext);
        if (m_cardUID != "")
        {
            m_cardUID = "";
            emit cardUIDChanged();
        }
        m_busy = false;
        return;
    }

    const SCARD_IO_REQUEST* pioSendPci = (dwActiveProtocol == SCARD_PROTOCOL_T0) ? SCARD_PCI_T0 : SCARD_PCI_T1;
    BYTE response[256];
    DWORD responseLength = sizeof(response);
    QString logResult;

    // Step 1: Get UID using FF CA 00 00 00
    BYTE apduGetUID[] = {0xFF, 0xCA, 0x00, 0x00, 0x00};
    lRetValue = SCardTransmit(hCard, pioSendPci, apduGetUID, sizeof(apduGetUID), nullptr, response, &responseLength);

    if (lRetValue == SCARD_S_SUCCESS && responseLength >= 2)
    {
        if (response[responseLength - 2] == 0x90 && response[responseLength - 1] == 0x00)
        {
            int uidLength = responseLength - 2;
            QString newUID = bytesToHex(response, uidLength);
            if (m_cardUID != newUID)
            {
                m_cardUID = newUID;
                emit cardUIDChanged();
            }
            logResult.append("Card UID: " + newUID + "\n\n");
        }
    }

    // Step 2: Read all sectors (0-15)
    // Mifare Classic 1K has 16 sectors, each with 4 blocks
    for (int sector = 0; sector < 16; sector++)
    {
        // Calculate the first block number of this sector
        int firstBlock = sector * 4;
        
        // Load key into reader: FF 82 00 00 06 [6-byte key]
        BYTE loadKeyCmd[] = {0xFF, 0x82, 0x20, 0x01, 0x06,
                             defaultKey[0], defaultKey[1], defaultKey[2], 
                             defaultKey[3], defaultKey[4], defaultKey[5]};
        responseLength = sizeof(response);
        lRetValue = SCardTransmit(hCard, pioSendPci, loadKeyCmd, sizeof(loadKeyCmd), nullptr, response, &responseLength);

        if (lRetValue != SCARD_S_SUCCESS || responseLength < 2 || 
            response[responseLength - 2] != 0x90 || response[responseLength - 1] != 0x00)
        {
            logResult.append(QString("Sector %1: Failed to load key\n").arg(sector));
            continue;
        }

        // Authenticate with Key A: 80 88 60 01 00 [block] [6-byte key] [key type]
        // Key type: 0x60 for Key A
        BYTE authCmd[] = {0xFF, 0x88, 0x00, static_cast<BYTE>(firstBlock), 0x60,
                          0x01};
        responseLength = sizeof(response);
        lRetValue = SCardTransmit(hCard, pioSendPci, authCmd, sizeof(authCmd), nullptr, response, &responseLength);

        if (lRetValue != SCARD_S_SUCCESS || responseLength < 2 || 
            response[responseLength - 2] != 0x90 || response[responseLength - 1] != 0x00)
        {
            logResult.append(QString("Sector %1: Authentication failed\n").arg(sector));
            continue;
        }

        // Read all 4 blocks in this sector
        logResult.append(QString("Sector %1:\n").arg(sector));
        bool sectorReadSuccess = true;
        
        for (int block = 0; block < 4; block++)
        {
            int blockNum = firstBlock + block;
            
            // Read Binary: FF B0 00 [block] 10 (read 16 bytes)
            BYTE readCmd[] = {0xFF, 0xB0, 0x00, static_cast<BYTE>(blockNum), 0x10};
            responseLength = sizeof(response);
            lRetValue = SCardTransmit(hCard, pioSendPci, readCmd, sizeof(readCmd), nullptr, response, &responseLength);

            if (lRetValue == SCARD_S_SUCCESS && responseLength >= 18 && 
                response[responseLength - 2] == 0x90 && response[responseLength - 1] == 0x00)
            {
                // Successfully read 16 bytes
                QString blockData = bytesToHex(response, 16);
                logResult.append(QString("  Block %1: %2\n").arg(blockNum, 2, 10, QChar('0')).arg(blockData));
            }
            else
            {
                logResult.append(QString("  Block %1: Read failed\n").arg(blockNum, 2, 10, QChar('0')));
                sectorReadSuccess = false;
            }
        }
        
        if (!sectorReadSuccess)
        {
            logResult.append("\n");
        }
        else
        {
            logResult.append("\n");
        }
    }

    setLog(logResult);

    // Cleanup
    SCardDisconnect(hCard, SCARD_LEAVE_CARD);
    free(mszReaders);
    SCardReleaseContext(hContext);
    m_busy = false;
}

void CardReader::writeMifareClassicCard(const QString &hexData)
{
    if (m_busy)
        return;
    m_busy = true;

    // Clear the display
    setLog("");

    // Parse Hex String Input ("AB CD EF 01 02 ...")
    QStringList byteStrings = hexData.simplified().split(' ', Qt::SkipEmptyParts);
    if (byteStrings.isEmpty())
    {
        setLog("Write Error: No Data to Write");
        m_busy = false;
        return;
    }

    QByteArray dataBytes;
    for (const QString &byteStr : byteStrings)
    {
        bool ok;
        unsigned char byte = static_cast<unsigned char>(byteStr.toUInt(&ok, 16));
        if (!ok)
        {
            setLog("Write Error: Invalid hex byte '" + byteStr + "'");
            m_busy = false;
            return;
        }
        dataBytes.append(static_cast<char>(byte));
    }

    // Factory default key: FF FF FF FF FF FF
    const BYTE defaultKey[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    SCARDCONTEXT hContext = 0;
    LONG lRetValue = SCardEstablishContext(SCARD_SCOPE_USER, nullptr, nullptr, &hContext);

    if (lRetValue != SCARD_S_SUCCESS)
    {
        setLog("Write Error: Cannot establish smart card context");
        m_busy = false;
        return;
    }

    DWORD dwReaders = 0;
    lRetValue = SCardListReadersW(hContext, nullptr, nullptr, &dwReaders);

    if (lRetValue != SCARD_S_SUCCESS || dwReaders == 0)
    {
        SCardReleaseContext(hContext);
        setLog("Write error: No card readers found");
        m_busy = false;
        return;
    }

    LPWSTR mszReaders = (LPWSTR)malloc(dwReaders * sizeof(wchar_t));
    if (!mszReaders)
    {
        SCardReleaseContext(hContext);
        setLog("Write error: Memory allocation failed");
        m_busy = false;
        return;
    }
    lRetValue = SCardListReadersW(hContext, nullptr, mszReaders, &dwReaders);

    if (lRetValue != SCARD_S_SUCCESS)
    {
        free(mszReaders);
        SCardReleaseContext(hContext);
        setLog("Write error: Failed to list readers");
        m_busy = false;
        return;
    }

    // Find a contactless reader
    LPWSTR readerName = nullptr;
    LPWSTR currentReader = mszReaders;

    while (*currentReader != L'\0') {
        QString readerNameQStr = QString::fromWCharArray(currentReader);
        QString readerNameUpper = readerNameQStr.toUpper();

        if (readerNameUpper.contains("CONTACTLESS") ||
            readerNameUpper.contains("CL") ||
            readerNameUpper.contains("PROXIMITY") ||
            readerNameUpper.contains("NFC") ||
            readerNameUpper.contains("PICC") ||
            readerNameUpper.contains("RFID"))
        {
            readerName = currentReader;
            break;
        }

        currentReader += wcslen(currentReader) + 1;
    }

    if (readerName == nullptr)
    {
        readerName = mszReaders;
    }

    // Connect to the card
    SCARDHANDLE hCard = 0;
    DWORD dwActiveProtocol = 0;
    lRetValue = SCardConnectW(hContext, readerName, SCARD_SHARE_SHARED, SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1, &hCard, &dwActiveProtocol);

    if (lRetValue != SCARD_S_SUCCESS)
    {
        free(mszReaders);
        SCardReleaseContext(hContext);
        setLog("Write error: No card present on reader");
        m_busy = false;
        return;
    }

    const SCARD_IO_REQUEST* pioSendPci = (dwActiveProtocol == SCARD_PROTOCOL_T0) ? SCARD_PCI_T0 : SCARD_PCI_T1;
    BYTE response[256];
    DWORD responseLength = sizeof(response);
    QString logResult;

    // Sector 1 --> block 4
    int startBlock = 4;  // First data block of sector 1
    int firstBlockOfSector = 4;  // First block of sector 1 (for authentication)

    // Load key into reader: FF 82 00 00 06 [6-byte key]
    BYTE loadKeyCmd[] = {0xFF, 0x82, 0x20, 0x01, 0x06,
                         defaultKey[0], defaultKey[1], defaultKey[2],
                         defaultKey[3], defaultKey[4], defaultKey[5]};
    responseLength = sizeof(response);
    lRetValue = SCardTransmit(hCard, pioSendPci, loadKeyCmd, sizeof(loadKeyCmd), nullptr, response, &responseLength);

    if (lRetValue != SCARD_S_SUCCESS || responseLength < 2 || 
        response[responseLength - 2] != 0x90 || response[responseLength - 1] != 0x00)
    {
        QString errorHex = (responseLength >= 2) ? bytesToHex(response + responseLength - 2, 2) : "N/A";
        setLog("Write Error: Failed to load key - SW: " + errorHex);
        SCardDisconnect(hCard, SCARD_LEAVE_CARD);
        free(mszReaders);
        SCardReleaseContext(hContext);
        m_busy = false;
        return;
    }

    // Authenticate with Key A: 80 88 60 01 00 [block] [6-byte key] [key type]
    // Key type: 0x60 for Key A
    BYTE authCmd[] = {0xFF, 0x88, 0x00, static_cast<BYTE>(firstBlockOfSector), 0x60,
                      0x01};

    responseLength = sizeof(response);
    lRetValue = SCardTransmit(hCard, pioSendPci, authCmd, sizeof(authCmd), nullptr, response, &responseLength);

    if (lRetValue != SCARD_S_SUCCESS || responseLength < 2 || 
        response[responseLength - 2] != 0x90 || response[responseLength - 1] != 0x00)
    {
        QString errorHex = (responseLength >= 2) ? bytesToHex(response + responseLength - 2, 2) : "N/A";
        setLog("Write Error: Authentication failed - SW: " + errorHex);
        SCardDisconnect(hCard, SCARD_LEAVE_CARD);
        free(mszReaders);
        SCardReleaseContext(hContext);
        m_busy = false;
        return;
    }

    logResult.append("Writing " + QString::number(dataBytes.size()) + " bytes starting from Sector 1, Block 4...\n");

    // Write data in 16-byte blocks (Mifare Classic blocks are 16 bytes)
    int totalBytes = dataBytes.size();
    int blockIndex = 0;

    for (int offset = 0; offset < totalBytes; offset += 16)
    {
        int currentBlock = startBlock + blockIndex;

        // Don't write to sector trailer (block 7)
        if (currentBlock >= 7)
        {
            logResult.append("\nWrite stopped: Reached sector trailer (block 7)");
            break;
        }

        // Prepare 16-byte block data (pad with zeros if needed)
        BYTE blockData[16] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
        int bytesToWrite = (offset + 16 <= totalBytes) ? 16 : (totalBytes - offset);
        for (int i = 0; i < bytesToWrite; i++)
        {
            blockData[i] = static_cast<BYTE>(dataBytes[offset + i]);
        }

        // UPDATE BINARY: FF D6 00 [block] 10 [16 bytes of data]
        BYTE writeCmd[21] = {0xFF, 0xD6, 0x00, static_cast<BYTE>(currentBlock), 0x10,
                             blockData[0], blockData[1], blockData[2], blockData[3],
                             blockData[4], blockData[5], blockData[6], blockData[7],
                             blockData[8], blockData[9], blockData[10], blockData[11],
                             blockData[12], blockData[13], blockData[14], blockData[15]};
        responseLength = sizeof(response);
        lRetValue = SCardTransmit(hCard, pioSendPci, writeCmd, sizeof(writeCmd), nullptr, response, &responseLength);

        if (lRetValue == SCARD_S_SUCCESS && responseLength >= 2 && 
            response[responseLength - 2] == 0x90 && response[responseLength - 1] == 0x00)
        {
            QString blockNum = QString::number(currentBlock).rightJustified(2, QChar('0'));
            logResult.append("Block " + blockNum + " <-- " + bytesToHex(blockData, 16) + "  OK\n");
        }
        else
        {
            QString errorHex = (responseLength >= 2) ? bytesToHex(response + responseLength - 2, 2) : "N/A";
            logResult.append("Block " + QString::number(currentBlock) + " write FAILED - SW: " + errorHex + "\n");
            break;
        }

        blockIndex++;
    }

    logResult.append("\nWrite complete.");
    setLog(logResult);

    // Cleanup
    SCardDisconnect(hCard, SCARD_LEAVE_CARD);
    free(mszReaders);
    SCardReleaseContext(hContext);
    m_busy = false;
}

void CardReader::writeMifareClassicSectorTrailer()
{
    if (m_busy)
        return;
    m_busy = true;

    // Clear the display
    setLog("");

    // Factory default key: FF FF FF FF FF FF (used for authentication before writing)
    const BYTE defaultKey[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    // Sector 2, Block 11 (sector trailer) data: 01 02 03 04 05 06 FF 07 80 69 01 02 03 04 05 06
    // Key A: 01 02 03 04 05 06 (bytes 0-5)
    // Access bits: FF 07 80 69 (bytes 6-9) - Valid Mifare Classic format
    //   This configuration allows: Read with Key A, Write with Key B
    // Key B: 01 02 03 04 05 06 (bytes 10-15)
    // 
    // Note: Access bits must follow complementary format. FF 07 80 69 is a known valid pattern.
    const BYTE block7Data[16] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06,  // Key A
                                 0xFF, 0x07, 0x80, 0x69,                // Access bits (valid format)
                                 0x01, 0x02, 0x03, 0x04, 0x05, 0x06}; // Key B

    SCARDCONTEXT hContext = 0;
    LONG lRetValue = SCardEstablishContext(SCARD_SCOPE_USER, nullptr, nullptr, &hContext);

    if (lRetValue != SCARD_S_SUCCESS)
    {
        setLog("Write Error: Cannot establish smart card context");
        m_busy = false;
        return;
    }

    DWORD dwReaders = 0;
    lRetValue = SCardListReadersW(hContext, nullptr, nullptr, &dwReaders);

    if (lRetValue != SCARD_S_SUCCESS || dwReaders == 0)
    {
        SCardReleaseContext(hContext);
        setLog("Write error: No card readers found");
        m_busy = false;
        return;
    }

    LPWSTR mszReaders = (LPWSTR)malloc(dwReaders * sizeof(wchar_t));
    if (!mszReaders)
    {
        SCardReleaseContext(hContext);
        setLog("Write error: Memory allocation failed");
        m_busy = false;
        return;
    }
    lRetValue = SCardListReadersW(hContext, nullptr, mszReaders, &dwReaders);

    if (lRetValue != SCARD_S_SUCCESS)
    {
        free(mszReaders);
        SCardReleaseContext(hContext);
        setLog("Write error: Failed to list readers");
        m_busy = false;
        return;
    }

    // Find a contactless reader
    LPWSTR readerName = nullptr;
    LPWSTR currentReader = mszReaders;

    while (*currentReader != L'\0') {
        QString readerNameQStr = QString::fromWCharArray(currentReader);
        QString readerNameUpper = readerNameQStr.toUpper();

        if (readerNameUpper.contains("CONTACTLESS") ||
            readerNameUpper.contains("CL") ||
            readerNameUpper.contains("PROXIMITY") ||
            readerNameUpper.contains("NFC") ||
            readerNameUpper.contains("PICC") ||
            readerNameUpper.contains("RFID"))
        {
            readerName = currentReader;
            break;
        }

        currentReader += wcslen(currentReader) + 1;
    }

    if (readerName == nullptr)
    {
        readerName = mszReaders;
    }

    // Connect to the card
    SCARDHANDLE hCard = 0;
    DWORD dwActiveProtocol = 0;
    lRetValue = SCardConnectW(hContext, readerName, SCARD_SHARE_SHARED, SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1, &hCard, &dwActiveProtocol);

    if (lRetValue != SCARD_S_SUCCESS)
    {
        free(mszReaders);
        SCardReleaseContext(hContext);
        setLog("Write error: No card present on reader");
        m_busy = false;
        return;
    }

    const SCARD_IO_REQUEST* pioSendPci = (dwActiveProtocol == SCARD_PROTOCOL_T0) ? SCARD_PCI_T0 : SCARD_PCI_T1;
    BYTE response[256];
    DWORD responseLength = sizeof(response);
    QString logResult;

    // Sector 2 --> block 8 is first data block, block 11 is sector trailer
    int firstBlockOfSector = 8;  // First block of sector 2 (for authentication)
    int sectorTrailerBlock = 11;  // Sector trailer block for sector 2

    // Load key into reader: FF 82 00 00 06 [6-byte key]
    BYTE loadKeyCmd[] = {0xFF, 0x82, 0x20, 0x01, 0x06,
                         defaultKey[0], defaultKey[1], defaultKey[2],
                         defaultKey[3], defaultKey[4], defaultKey[5]};
    responseLength = sizeof(response);
    lRetValue = SCardTransmit(hCard, pioSendPci, loadKeyCmd, sizeof(loadKeyCmd), nullptr, response, &responseLength);

    if (lRetValue != SCARD_S_SUCCESS || responseLength < 2 || 
        response[responseLength - 2] != 0x90 || response[responseLength - 1] != 0x00)
    {
        QString errorHex = (responseLength >= 2) ? bytesToHex(response + responseLength - 2, 2) : "N/A";
        setLog("Write Error: Failed to load key - SW: " + errorHex);
        SCardDisconnect(hCard, SCARD_LEAVE_CARD);
        free(mszReaders);
        SCardReleaseContext(hContext);
        m_busy = false;
        return;
    }

    // Authenticate with Key A: FF 88 00 [block] 0x60 [key slot]
    BYTE authCmd[] = {0xFF, 0x88, 0x00, static_cast<BYTE>(firstBlockOfSector), 0x60, 0x01};

    responseLength = sizeof(response);
    lRetValue = SCardTransmit(hCard, pioSendPci, authCmd, sizeof(authCmd), nullptr, response, &responseLength);

    if (lRetValue != SCARD_S_SUCCESS || responseLength < 2 || 
        response[responseLength - 2] != 0x90 || response[responseLength - 1] != 0x00)
    {
        QString errorHex = (responseLength >= 2) ? bytesToHex(response + responseLength - 2, 2) : "N/A";
        setLog("Write Error: Authentication failed - SW: " + errorHex);
        SCardDisconnect(hCard, SCARD_LEAVE_CARD);
        free(mszReaders);
        SCardReleaseContext(hContext);
        m_busy = false;
        return;
    }

    logResult.append("Writing Sector 2, Block 11 (Sector Trailer)...\n");


    // UPDATE BINARY: FF D6 00 [block] 10 [16 bytes of data]
    BYTE writeCmd[21] = {0xFF, 0xD6, 0x00, static_cast<BYTE>(sectorTrailerBlock), 0x10,
                         block7Data[0], block7Data[1], block7Data[2], block7Data[3],
                         block7Data[4], block7Data[5], block7Data[6], block7Data[7],
                         block7Data[8], block7Data[9], block7Data[10], block7Data[11],
                         block7Data[12], block7Data[13], block7Data[14], block7Data[15]};
    responseLength = sizeof(response);
    lRetValue = SCardTransmit(hCard, pioSendPci, writeCmd, sizeof(writeCmd), nullptr, response, &responseLength);

    if (lRetValue == SCARD_S_SUCCESS && responseLength >= 2 && 
        response[responseLength - 2] == 0x90 && response[responseLength - 1] == 0x00)
    {
        logResult.append("Block 11 <-- " + bytesToHex(block7Data, 16) + "  OK\n");
        logResult.append("\nSector 2 trailer updated successfully!");

    }
    else
    {
        QString errorHex = (responseLength >= 2) ? bytesToHex(response + responseLength - 2, 2) : "N/A";
        logResult.append("Block 11 write FAILED - SW: " + errorHex + "\n");
        logResult.append("\nWrite failed. Make sure the card is present and authenticated.\n");
    }

    setLog(logResult);

    // Cleanup
    SCardDisconnect(hCard, SCARD_LEAVE_CARD);
    free(mszReaders);
    SCardReleaseContext(hContext);
    m_busy = false;
}

void CardReader::readUltralightCard()
{
    m_busy = true;

    // Clear the display
    setLog("");
    m_lastReadData.clear();

    SCARDCONTEXT hContext = 0;
    LONG lRetValue = SCardEstablishContext(SCARD_SCOPE_USER, nullptr, nullptr, &hContext);

    if (lRetValue != SCARD_S_SUCCESS)
    {
        if (m_cardUID != "")
        {
            m_cardUID = "";
            emit cardUIDChanged();
        }
        m_busy = false;
        return;
    }

    DWORD dwReaders = 0;
    lRetValue = SCardListReadersW(hContext, nullptr, nullptr, &dwReaders);

    if (lRetValue != SCARD_S_SUCCESS || dwReaders == 0)
    {
        SCardReleaseContext(hContext);
        if (m_cardUID != "")
        {
            m_cardUID = "";
            emit cardUIDChanged();
        }
        m_busy = false;
        return;
    }

    LPWSTR mszReaders = (LPWSTR)malloc(dwReaders * sizeof(wchar_t));
    if (!mszReaders)
    {
        SCardReleaseContext(hContext);
        m_busy = false;
        return;
    }

    lRetValue = SCardListReadersW(hContext, nullptr, mszReaders, &dwReaders);

    if (lRetValue != SCARD_S_SUCCESS)
    {
        free(mszReaders);
        SCardReleaseContext(hContext);
        if (m_cardUID != "")
        {
            m_cardUID = "";
            emit cardUIDChanged();
        }
        m_busy = false;
        return;
    }

    // Find a contactless reader
    LPWSTR readerName = nullptr;
    LPWSTR currentReader = mszReaders;

    while (*currentReader != L'\0') {
        QString readerNameQStr = QString::fromWCharArray(currentReader);
        QString readerNameUpper = readerNameQStr.toUpper();

        if (readerNameUpper.contains("CONTACTLESS") ||
            readerNameUpper.contains("CL") ||
            readerNameUpper.contains("PROXIMITY") ||
            readerNameUpper.contains("NFC") ||
            readerNameUpper.contains("PICC") ||
            readerNameUpper.contains("RFID"))
        {
            readerName = currentReader;
            break;
        }

        currentReader += wcslen(currentReader) + 1;
    }

    if (readerName == nullptr)
    {
        readerName = mszReaders;
    }

    // Connect to the card
    SCARDHANDLE hCard = 0;
    DWORD dwActiveProtocol = 0;
    lRetValue = SCardConnectW(hContext, readerName, SCARD_SHARE_SHARED, SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1, &hCard, &dwActiveProtocol);

    if (lRetValue != SCARD_S_SUCCESS)
    {
        free(mszReaders);
        SCardReleaseContext(hContext);
        if (m_cardUID != "")
        {
            m_cardUID = "";
            emit cardUIDChanged();
        }
        m_busy = false;
        return;
    }

    const SCARD_IO_REQUEST* pioSendPci = (dwActiveProtocol == SCARD_PROTOCOL_T0) ? SCARD_PCI_T0 : SCARD_PCI_T1;

    // Step 1: Get UID using FF CA 00 00 00
    BYTE apduGetUID[] = {0xFF, 0xCA, 0x00, 0x00, 0x00};
    BYTE response[256];
    DWORD responseLength = sizeof(response);

    lRetValue = SCardTransmit(hCard, pioSendPci, apduGetUID, sizeof(apduGetUID), nullptr, response, &responseLength);


    QString logResult;

    if (lRetValue == SCARD_S_SUCCESS && responseLength >= 2)
    {
        if (response[responseLength - 2] == 0x90 && response[responseLength - 1] == 0x00)
        {

            int uidLength = responseLength - 2;
            QString newUID = bytesToHex(response, uidLength);
            if (m_cardUID != newUID)
            {
                m_cardUID = newUID;
                emit cardUIDChanged();
            }

            //Read all 16 Ultralight memory pages (0-15)
            for (int page = 0; page < 16; page += 4) {
                BYTE readCmd[] = {0xFF, 0xB0, 0x00, static_cast<BYTE>(page), 0x10};
                DWORD readRespLen = sizeof(response);

                lRetValue = SCardTransmit(hCard, pioSendPci, readCmd, sizeof(readCmd), nullptr, response, &readRespLen);

                if (lRetValue == SCARD_S_SUCCESS && readRespLen >= 2 && response[readRespLen - 2] == 0x90 && response[readRespLen - 1] == 0x00 && readRespLen >= 18)
                {
                    // Store raw page data (16 bytes = 4 pages)
                    m_lastReadData.append(reinterpret_cast<const char*>(response), 16);

                    for (int i = 0; i < 4; i++)
                    {
                        QString pageNum = QString::number(page + i).rightJustified(2, ' ');
                        if (!logResult.isEmpty())
                            logResult.append("\n");
                        logResult.append("Page " + pageNum + "--> " +
                                         bytesToHex(response + (i * 4), 4));
                    }
                }
                else
                {
                    if (!logResult.isEmpty())
                        logResult.append("\n");

                    logResult.append("Page " + QString::number(page) + " read failed");
                    break;
                }
            }

        }
        else
        {
            if (m_cardUID != "")
            {
                m_cardUID = "";

                emit cardUIDChanged();
            }
        }
    }
    else
    {
        if (m_cardUID != "")
        {
            m_cardUID = "";
            emit cardUIDChanged();
        }
    }


    setLog(logResult);


    if (m_lastReadData.size() > 0)
    {
        m_ultralightRawData = formatAsPages(reinterpret_cast<const unsigned char*>(m_lastReadData.constData()), m_lastReadData.size());
        emit ultralightRawDataChanged();
        
        // Encrypt the data (64 bytes = 16 pages * 4 bytes)
        if (m_lastReadData.size() >= 16)
        {
            encryptUltralightData(m_lastReadData);
        } else
        {
            m_ultralightAESData = "Encryption en az 16 byte ile yapilmali!!!";
            emit ultralightAESDataChanged();
        }
    }

    // Cleanup
    SCardDisconnect(hCard, SCARD_LEAVE_CARD);
    free(mszReaders);
    SCardReleaseContext(hContext);
    m_busy = false;
}

void CardReader::startPolling()
{
    if (!m_pollTimer->isActive()) {
        m_pollTimer->start();
        readCard();
    }
}

void CardReader::stopPolling()
{
    if (m_pollTimer->isActive()) {
        m_pollTimer->stop();
    }
}

void CardReader::onPollTimer()
{
    // Only poll in DESFire mode
    if (m_cardType == MifareDesfire) {
        readCard();
    }
}

void CardReader::writeUltralightCard(const QString &hexData)
{
    if (m_busy)
        return;
    m_busy = true;

    // Parse Hex String Input ("AB CD EF 01 02 ...")
    QStringList byteStrings = hexData.simplified().split(' ', Qt::SkipEmptyParts);
    if (byteStrings.isEmpty())
    {
        setLog("Write Error: No Data to Write");
        m_busy = false;
        return;
    }

    QByteArray dataBytes;
    for (const QString &byteStr : byteStrings)
    {
        bool ok;
        unsigned char byte = static_cast<unsigned char>(byteStr.toUInt(&ok, 16));
        if (!ok)
        {
            setLog("Write Error: Invalid hex byte '" + byteStr + "'");
            m_busy = false;
            return;
        }
        dataBytes.append(static_cast<char>(byte));
    }


    SCARDCONTEXT hContext = 0;
    LONG lRetValue = SCardEstablishContext(SCARD_SCOPE_USER, nullptr, nullptr, &hContext);
    if (lRetValue != SCARD_S_SUCCESS)
    {
        setLog("Write Error: Cannot establish smart card context");
        m_busy = false;
        return;
    }

    // List readers
    DWORD dwReaders = 0;
    lRetValue = SCardListReadersW(hContext, nullptr, nullptr, &dwReaders);
    if (lRetValue != SCARD_S_SUCCESS || dwReaders == 0)
    {
        SCardReleaseContext(hContext);
        setLog("Write error: No card readers found");
        m_busy = false;
        return;
    }

    LPWSTR mszReaders = (LPWSTR)malloc(dwReaders * sizeof(wchar_t));
    if (!mszReaders)
    {
        SCardReleaseContext(hContext);
        setLog("Write error: Memory allocation failed");
        m_busy = false;
        return;
    }
    lRetValue = SCardListReadersW(hContext, nullptr, mszReaders, &dwReaders);
    if (lRetValue != SCARD_S_SUCCESS)
    {
        free(mszReaders);
        SCardReleaseContext(hContext);
        setLog("Write error: Failed to list readers");
        m_busy = false;
        return;
    }

    // Contactless reader ismini Bul
    LPWSTR readerName = nullptr;
    LPWSTR currentReader = mszReaders;
    while (*currentReader != L'\0') {
        QString readerNameQStr = QString::fromWCharArray(currentReader);
        QString readerNameUpper = readerNameQStr.toUpper();
        if (readerNameUpper.contains("CONTACTLESS") ||
            readerNameUpper.contains("CL") ||
            readerNameUpper.contains("PROXIMITY") ||
            readerNameUpper.contains("NFC") ||
            readerNameUpper.contains("PICC") ||
            readerNameUpper.contains("RFID"))
        {
            readerName = currentReader;
            break;
        }
        currentReader += wcslen(currentReader) + 1;
    }
    if (readerName == nullptr)
        readerName = mszReaders;

    // Connect to card
    SCARDHANDLE hCard = 0;
    DWORD dwActiveProtocol = 0;
    lRetValue = SCardConnectW(hContext, readerName, SCARD_SHARE_SHARED,
                             SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1,
                             &hCard, &dwActiveProtocol);
    if (lRetValue != SCARD_S_SUCCESS)
    {
        free(mszReaders);
        SCardReleaseContext(hContext);
        setLog("Write error: No card present on reader");
        m_busy = false;
        return;
    }

    const SCARD_IO_REQUEST* pioSendPci = (dwActiveProtocol == SCARD_PROTOCOL_T0) ? SCARD_PCI_T0 : SCARD_PCI_T1;


    QString logResult;

    // Page 5 'ten yazmaya başla
    // PC/SC UPDATE BINARY: FF D6 00 PageNum 04 D0 D1 D2 D3
    int startPage = 5;
    int totalBytes = dataBytes.size();
    int pageIndex = 0;

    logResult.append("Writing " + QString::number(totalBytes) + " bytes starting at page " +
                     QString::number(startPage) + "...");

    for (int offset = 0; offset < totalBytes; offset += 4)
    {
        int currentPage = startPage + pageIndex;

        // Ultralight 16 sayfa
        if (currentPage > 15)
        {
            logResult.append("\nWrite error: Exceeded max page (16)");
            break;
        }

        // 4-byte page data hazirla. Gerisini 0 ile padding
        BYTE pageData[4] = {0x00, 0x00, 0x00, 0x00};
        for (int i = 0; i < 4 && (offset + i) < totalBytes; i++)
        {
            pageData[i] = static_cast<BYTE>(dataBytes[offset + i]);
        }

        // UPDATE BINARY: FF D6 00 PageNum 04 D0 D1 D2 D3
        BYTE writeCmd[] = {0xFF, 0xD6, 0x00, static_cast<BYTE>(currentPage), 0x04, pageData[0], pageData[1], pageData[2], pageData[3]};
        BYTE response[256];
        DWORD responseLength = sizeof(response);

        lRetValue = SCardTransmit(hCard, pioSendPci, writeCmd, sizeof(writeCmd), nullptr, response, &responseLength);

        if (lRetValue == SCARD_S_SUCCESS && responseLength >= 2 && response[responseLength - 2] == 0x90 && response[responseLength - 1] == 0x00)
        {
            QString pageNum = QString::number(currentPage).rightJustified(2, ' ');
            logResult.append("\nPage " + pageNum + " <-- " + bytesToHex(pageData, 4) + "  OK");
        }
        else
        {
            logResult.append("\nPage " + QString::number(currentPage) + " write FAILED");
            break;
        }

        pageIndex++;
    }

    logResult.append("\nWrite complete.");


    setLog(logResult);


    SCardDisconnect(hCard, SCARD_LEAVE_CARD);
    free(mszReaders);
    SCardReleaseContext(hContext);
    m_busy = false;
}

void CardReader::encryptUltralightData(const QByteArray &data)
{
    // (AES-128 16 byte lik bloklarla encrpt edilecek
    
    int dataSize = data.size();
    if (dataSize < 16)
    {
        m_ultralightAESData = "Data en az 16 byte olmali!!!";
        emit ultralightAESDataChanged();
        return;
    }

    // AES provider
    BCRYPT_ALG_HANDLE hAesAlg = nullptr;
    NTSTATUS status = BCryptOpenAlgorithmProvider(&hAesAlg, BCRYPT_AES_ALGORITHM, nullptr, 0);
    
    if (!BCRYPT_SUCCESS(status) || !hAesAlg)
    {
        m_ultralightAESData = "Encryption Hata: Algorithm Provider Error!!!";
        emit ultralightAESDataChanged();
        return;
    }

    // Chaining mode = CBC yapildi
    status = BCryptSetProperty(hAesAlg, BCRYPT_CHAINING_MODE, (PBYTE)BCRYPT_CHAIN_MODE_CBC, sizeof(BCRYPT_CHAIN_MODE_CBC), 0);

    if (!BCRYPT_SUCCESS(status))
    {
        BCryptCloseAlgorithmProvider(hAesAlg, 0);

        m_ultralightAESData = "Encryption Hata: Chaining Mode Error!!!";

        emit ultralightAESDataChanged();
        return;
    }

    // Generate Key Handle
    BCRYPT_KEY_HANDLE hKey = nullptr;
    status = BCryptGenerateSymmetricKey(hAesAlg, &hKey, nullptr, 0, (PBYTE)AES_KEY, sizeof(AES_KEY), 0);

    if (!BCRYPT_SUCCESS(status) || !hKey)
    {
        BCryptCloseAlgorithmProvider(hAesAlg, 0);
        m_ultralightAESData = "Encryption Hata: Key Generation Error!!!";
        emit ultralightAESDataChanged();
        return;
    }

    // Encrypt Data 16-byte Blocks
    QByteArray encryptedResult;
    BYTE iv[16];
    memcpy(iv, AES_IV, sizeof(AES_IV));
    
    int numBlocks = (dataSize + 15) / 16;
    
    for (int block = 0; block < numBlocks; block++)
    {
        int offset = block * 16;
        int blockSize = (offset + 16 <= dataSize) ? 16 : (dataSize - offset);
        
        // Prepare block data
        BYTE blockData[16] = {0};
        memcpy(blockData, data.constData() + offset, blockSize);
        
        BYTE encryptedBlock[16] = {0};
        ULONG encryptedBlockLen = sizeof(encryptedBlock);
        
        // İlk blok için IV kullan daha sonra IV olarak encrypted blok IV olarak kullan
        status = BCryptEncrypt(hKey, blockData, 16, nullptr, iv, sizeof(iv), encryptedBlock, sizeof(encryptedBlock), &encryptedBlockLen, 0);
        
        if (!BCRYPT_SUCCESS(status))
        {
            BCryptDestroyKey(hKey);
            BCryptCloseAlgorithmProvider(hAesAlg, 0);
            m_ultralightAESData = "Encryption Hata Blok = " + QString::number(block);
            emit ultralightAESDataChanged();
            return;
        }
        
        // Encrypted block encryptedResult'a eklendi.
        encryptedResult.append(reinterpret_cast<const char*>(encryptedBlock), 16);
        

        memcpy(iv, encryptedBlock, 16);
    }


    BCryptDestroyKey(hKey);
    BCryptCloseAlgorithmProvider(hAesAlg, 0);

    m_ultralightAESData = formatAsPages(reinterpret_cast<const unsigned char*>(encryptedResult.constData()), encryptedResult.size());
    emit ultralightAESDataChanged();
    
    // Decrypt to Verify
    decryptUltralightData(encryptedResult);
}

void CardReader::decryptUltralightData(const QByteArray &encryptedData)
{
    int encryptedSize = encryptedData.size();

    if (encryptedSize < 16 || (encryptedSize % 16) != 0)
    {
        m_ultralightRestoredData = "Decryption Hata: Gecersiz Data Size (16 kati olmali)!!!";
        emit ultralightRestoredDataChanged();
        return;
    }

    // Initialize AES provider
    BCRYPT_ALG_HANDLE hAesAlg = nullptr;
    NTSTATUS status = BCryptOpenAlgorithmProvider(&hAesAlg, BCRYPT_AES_ALGORITHM, nullptr, 0);
    
    if (!BCRYPT_SUCCESS(status) || !hAesAlg)
    {
        m_ultralightRestoredData = "Decryption Hata: Algorithm Provider Error!!!";
        emit ultralightRestoredDataChanged();
        return;
    }

    // Chaining mode = CBC yapildi
    status = BCryptSetProperty(hAesAlg, BCRYPT_CHAINING_MODE, (PBYTE)BCRYPT_CHAIN_MODE_CBC, sizeof(BCRYPT_CHAIN_MODE_CBC), 0);

    if (!BCRYPT_SUCCESS(status))
    {
        BCryptCloseAlgorithmProvider(hAesAlg, 0);
        m_ultralightRestoredData = "Decryption Hata: Chain Mode Error!!!";
        emit ultralightRestoredDataChanged();
        return;
    }

    // Generate key handle
    BCRYPT_KEY_HANDLE hKey = nullptr;
    status = BCryptGenerateSymmetricKey(hAesAlg, &hKey, nullptr, 0, (PBYTE)AES_KEY, sizeof(AES_KEY), 0);

    if (!BCRYPT_SUCCESS(status) || !hKey)
    {
        BCryptCloseAlgorithmProvider(hAesAlg, 0);
        m_ultralightRestoredData = "Decryption Hata: Key Generation Error!!!";
        emit ultralightRestoredDataChanged();
        return;
    }

    // Decrypt data
    QByteArray decryptedResult;
    BYTE iv[16];
    memcpy(iv, AES_IV, sizeof(AES_IV));
    
    int numBlocks = encryptedSize / 16;
    
    for (int block = 0; block < numBlocks; block++)
    {
        int offset = block * 16;
        
        BYTE decryptedBlock[16] = {0};
        ULONG decryptedBlockLen = sizeof(decryptedBlock);
        
        // Use the same IV for first block, then use previous encrypted block as IV for chaining
        status = BCryptDecrypt(hKey, (PBYTE)encryptedData.constData() + offset, 16, nullptr, iv, sizeof(iv), decryptedBlock, sizeof(decryptedBlock), &decryptedBlockLen, 0);
        
        if (!BCRYPT_SUCCESS(status))
        {
            BCryptDestroyKey(hKey);
            BCryptCloseAlgorithmProvider(hAesAlg, 0);
            m_ultralightRestoredData = "Decryption Hata Block= " + QString::number(block);
            emit ultralightRestoredDataChanged();
            return;
        }
        

        decryptedResult.append(reinterpret_cast<const char*>(decryptedBlock), 16);
        
        memcpy(iv, encryptedData.constData() + offset, 16);
    }


    BCryptDestroyKey(hKey);
    BCryptCloseAlgorithmProvider(hAesAlg, 0);


    int originalSize = m_lastReadData.size();
    if (decryptedResult.size() > originalSize)
    {
        decryptedResult = decryptedResult.left(originalSize);
    }
    

    m_ultralightRestoredData = formatAsPages(reinterpret_cast<const unsigned char*>(decryptedResult.constData()), decryptedResult.size());
    emit ultralightRestoredDataChanged();
}

void CardReader::sign3DES()
{

    //TODO
}
