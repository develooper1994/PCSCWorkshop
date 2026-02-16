#pragma once
#include "CardConnection.h"

// PCSC Reader abstraction: each reader model can implement its own read/write APDUs
class Reader {
public:
    explicit Reader(CardConnection& c) : card(c) {}
    virtual ~Reader() {}

    virtual void writePage(BYTE page, const BYTE data4[4]) = 0;
    virtual BYTEV readPage(BYTE page) = 0;

    // convenience: write multi-page data
    virtual void writeData(BYTE startPage, const BYTEV& data) {
        size_t total = data.size();
        size_t pages = (total + 3) / 4;
        for (size_t i = 0; i < pages; ++i) {
            BYTE page = static_cast<BYTE>(startPage + i);
            BYTE chunk[4] = { 0, 0, 0, 0 };
            for (size_t b = 0; b < 4; ++b) {
                size_t idx = i * 4 + b;
                if (idx < total) chunk[b] = data[idx];
            }
            writePage(page, chunk);
        }
    }

    virtual BYTEV readData(BYTE startPage, size_t length) {
        BYTEV out;
        size_t remaining = length;
        BYTE page = startPage;
        while (remaining > 0) {
            auto p = readPage(page);
            if (p.size() > 2) // exclude SW if present, but readPage should return only data
                out.insert(out.end(), p.begin(), p.end());
            remaining = (remaining > 4) ? (remaining - 4) : 0;
            ++page;
        }
        return out;
    }

protected:
    CardConnection& card;
};

// ACR1281U specific reader
class ACR1281UReader : public Reader {
public:
    explicit ACR1281UReader(CardConnection& c) : Reader(c) {}

    void writePage(BYTE page, const BYTE data4[4]) override {
        if (!card.isConnected()) throw std::runtime_error("Card not connected");
        BYTEV apdu;
        apdu.reserve(5 + 4);
        apdu.push_back(0xFF);        // CLA
        apdu.push_back(0xD6);        // INS = UPDATE BINARY / vendor write
        apdu.push_back(0x00);        // P1
        apdu.push_back(page);        // P2 = page
        apdu.push_back(0x04);        // LC = 4
        apdu.insert(apdu.end(), data4, data4 + 4);

        auto resp = card.transmit(apdu);
        if (resp.size() < 2) throw std::runtime_error("Invalid response for write");
        BYTE sw1 = resp[resp.size()-2], sw2 = resp[resp.size()-1];
        if (!(sw1 == 0x90 && sw2 == 0x00)) {
            std::stringstream ss; ss << "Write failed SW=0x" << std::hex << (int)sw1 << " 0x" << (int)sw2;
            throw std::runtime_error(ss.str());
        }
    }

    BYTEV readPage(BYTE page) override {
        if (!card.isConnected()) throw std::runtime_error("Card not connected");
        BYTEV apdu;
        apdu.reserve(5);
        apdu.push_back(0xFF);    // CLA
        apdu.push_back(0xB0);    // INS = READ BINARY / vendor read
        apdu.push_back(0x00);    // P1
        apdu.push_back(page);    // P2 = page
        apdu.push_back(0x04);    // Le = 4

        auto resp = card.transmit(apdu);
        if (resp.size() < 2) throw std::runtime_error("Invalid response for read");
        BYTE sw1 = resp[resp.size()-2], sw2 = resp[resp.size()-1];
        if (!(sw1 == 0x90 && sw2 == 0x00)) {
            std::stringstream ss; ss << "Read failed SW=0x" << std::hex << (int)sw1 << " 0x" << (int)sw2;
            throw std::runtime_error(ss.str());
        }
        return BYTEV(resp.begin(), resp.end()-2);
    }
};
