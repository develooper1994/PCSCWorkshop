#ifndef SIMULATEDREADER_H
#define SIMULATEDREADER_H

#include "MifareCardSimulator.h"
#include "../CardDataTypes.h"
#include <memory>
#include <stdexcept>
#include <iostream>

// ════════════════════════════════════════════════════════════════════════════════
// SimulatedReader — In-Memory Reader for Testing
// ════════════════════════════════════════════════════════════════════════════════
//
// Implements a reader-like interface using MifareCardSimulator.
// Allows testing MifareCardCore without hardware or CardConnection.
//
// Usage:
//   auto cardSim = std::make_shared<MifareCardSimulator>(false);  // 1K
//   SimulatedReader reader(cardSim);
//   MifareCardCore card(reader, false);
//   
//   reader.loadKey({0xFF, ...}, KeyStructure::NonVolatile, 0x01);
//   card.read(1);  // Authenticate and read block 1
//
// ════════════════════════════════════════════════════════════════════════════════
class SimulatedReader {
public:
    // ────────────────────────────────────────────────────────────────────────────
    // Constructor
    // ────────────────────────────────────────────────────────────────────────────
    
    // Create reader for simulated card
    explicit SimulatedReader(std::shared_ptr<MifareCardSimulator> card);

    // ────────────────────────────────────────────────────────────────────────────
    // Reader-Like Interface
    // ────────────────────────────────────────────────────────────────────────────
    
    // Load key into reader memory
    void loadKey(const BYTE* key, KeyStructure ks, BYTE slot);
    
    // Set current key type (A or B)
    void setKeyType(KeyType kt);
    
    // Set current key slot
    void setKeyNumber(BYTE slot);
    
    // Request authentication
    void setAuthRequested(bool requested);
    
    // Perform authentication on block
    void performAuth(BYTE block);
    
    // Read page (block)
    BYTEV readPage(BYTE block);
    
    // Write page (block)
    void writePage(BYTE block, const BYTE* data);
    
    // Connection check
    bool isConnected() const noexcept;
    
    // ────────────────────────────────────────────────────────────────────────────
    // Test/Debug Methods
    // ────────────────────────────────────────────────────────────────────────────
    
    // Get underlying card simulator
    std::shared_ptr<MifareCardSimulator> card() { return card_; }
    
    // Reset reader state
    void reset() noexcept;
    
    // Print reader state
    void printState() const;

private:
    std::shared_ptr<MifareCardSimulator> card_;
    
    // Reader state
    KeyType currentKeyType_ = KeyType::A;
    BYTE currentKeySlot_ = 0x01;
    bool authRequested_ = false;
    int currentBlock_ = -1;
    int currentSector_ = -1;
    
    // Helper: get current sector from block
    int sectorFromBlock(int block) const noexcept;
};

#endif // SIMULATEDREADER_H
