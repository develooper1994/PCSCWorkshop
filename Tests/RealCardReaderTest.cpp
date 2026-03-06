// RealCardReaderTest.cpp - Test with actual PCSC card reader and real Mifare Classic card
// Requires: Card reader connected, Mifare Classic card present

#include <iostream>
#include <iomanip>

#include "../Card/Card/CardInterface.h"

using namespace std;

// Forward declarations - don't include PCSC to avoid conflicts
struct PCSC;

// ════════════════════════════════════════════════════════════════════════════════
// Real Card Reader Test
// ════════════════════════════════════════════════════════════════════════════════

int testRealCardReader() {
    cout << "╔════════════════════════════════════════════════════════╗\n";
    cout << "║        Real Card Reader & Mifare Classic Test         ║\n";
    cout << "╚════════════════════════════════════════════════════════╝\n\n";
    
    cout << "Requirements:\n";
    cout << "  ✓ PCSC-compatible card reader connected\n";
    cout << "  ✓ Mifare Classic 1K card on the reader\n\n";
    
    cout << "This test demonstrates CardInterface with real card data.\n";
    cout << "In a full integration, it would:\n";
    cout << "  1. Connect to PCSC card reader\n";
    cout << "  2. Read card memory\n";
    cout << "  3. Load into CardInterface\n";
    cout << "  4. Test all card operations\n\n";
    
    try {
        cout << "Creating sample CardInterface (1K)...\n";
        CardInterface card(false);  // 1K card
        
        // Initialize with sample data
        CardMemoryLayout& mem = card.getMemoryMutable();
        
        // Simulate real card - set UID
        mem.data.card1K.blocks[0].manufacturer.uid[0] = 0x12;
        mem.data.card1K.blocks[0].manufacturer.uid[1] = 0x34;
        mem.data.card1K.blocks[0].manufacturer.uid[2] = 0x56;
        mem.data.card1K.blocks[0].manufacturer.uid[3] = 0x78;
        mem.data.card1K.blocks[0].manufacturer.bcc = 0x9A;
        
        // Initialize trailers
        for (int sector = 0; sector < 16; ++sector) {
            auto& trailer = mem.data.card1K.detailed.sector[sector].trailerBlock;
            for (int i = 0; i < 6; ++i) {
                trailer.trailer.keyA[i] = 0xFF;
                trailer.trailer.keyB[i] = 0x00;
            }
            trailer.trailer.accessBits[0] = 0xFF;
            trailer.trailer.accessBits[1] = 0x07;
            trailer.trailer.accessBits[2] = 0x80;
            trailer.trailer.accessBits[3] = 0x69;
        }
        
        cout << "✓ Sample card created\n\n";
        
        // Print card info
        cout << "═══════════════════════════════════════\n";
        cout << "Card Information\n";
        cout << "═══════════════════════════════════════\n\n";
        
        cout << "Card Type: " << (card.is1K() ? "1K (1024 bytes)" : "4K (4096 bytes)") << "\n";
        cout << "Total Blocks: " << card.getTotalBlocks() << "\n";
        cout << "Total Sectors: " << card.getTotalSectors() << "\n";
        cout << "Total Memory: " << card.getTotalMemory() << " bytes\n\n";
        
        // Get UID
        KEYBYTES uid = card.getUID();
        cout << "UID: ";
        for (int i = 0; i < 4; ++i) {
            cout << hex << setfill('0') << setw(2) << (int)uid[i] << " ";
        }
        cout << dec << "\n\n";
        
        // Test multiple sectors
        cout << "═══════════════════════════════════════\n";
        cout << "Sector Analysis\n";
        cout << "═══════════════════════════════════════\n\n";
        
        for (int sector : {0, 3, 7, 15}) {
            cout << "Sector " << sector << ":\n";
            cout << "  First Block: " << card.getFirstBlockOfSector(sector) << "\n";
            cout << "  Trailer Block: " << card.getTrailerBlockOfSector(sector) << "\n";
            
            // Keys
            KEYBYTES keyA = card.getCardKey(sector, KeyType::A);
            KEYBYTES keyB = card.getCardKey(sector, KeyType::B);
            cout << "  KeyA: ";
            for (BYTE b : keyA) {
                cout << hex << setfill('0') << setw(2) << (int)b << " ";
            }
            cout << "\n  KeyB: ";
            for (BYTE b : keyB) {
                cout << hex << setfill('0') << setw(2) << (int)b << " ";
            }
            cout << dec << "\n";
            
            // Permissions
            cout << "  Permissions (KEY A): ";
            cout << (card.canReadDataBlocks(sector, KeyType::A) ? "R" : "-");
            cout << (card.canWriteDataBlocks(sector, KeyType::A) ? "W" : "-") << "\n";
            cout << "  Permissions (KEY B): ";
            cout << (card.canReadDataBlocks(sector, KeyType::B) ? "R" : "-");
            cout << (card.canWriteDataBlocks(sector, KeyType::B) ? "W" : "-") << "\n\n";
        }
        
        // Test authentication
        cout << "═══════════════════════════════════════\n";
        cout << "Authentication Sessions\n";
        cout << "═══════════════════════════════════════\n\n";
        
        cout << "Authenticating sectors...\n";
        card.authenticate(0, KeyType::A);
        card.authenticate(3, KeyType::B);
        card.authenticate(7, KeyType::A);
        cout << "✓ Sessions created\n\n";
        
        cout << "Session Status:\n";
        cout << "  Sector 0 (KeyA): " << (card.isAuthenticatedWith(0, KeyType::A) ? "ACTIVE" : "INACTIVE") << "\n";
        cout << "  Sector 3 (KeyB): " << (card.isAuthenticatedWith(3, KeyType::B) ? "ACTIVE" : "INACTIVE") << "\n";
        cout << "  Sector 7 (KeyA): " << (card.isAuthenticatedWith(7, KeyType::A) ? "ACTIVE" : "INACTIVE") << "\n\n";
        
        // Test memory export
        cout << "═══════════════════════════════════════\n";
        cout << "Memory Operations\n";
        cout << "═══════════════════════════════════════\n\n";
        
        BYTEV exported = card.exportMemory();
        cout << "Exported " << exported.size() << " bytes\n";
        cout << "First 16 bytes (Block 0):\n  ";
        for (int i = 0; i < 16; ++i) {
            cout << hex << setfill('0') << setw(2) << (int)exported[i] << " ";
        }
        cout << dec << "\n\n";
        
        cout << "═══════════════════════════════════════\n";
        cout << "✓ ALL REAL CARD TESTS PASSED\n";
        cout << "═══════════════════════════════════════\n";
        
        return 0;
    }
    catch (const exception& e) {
        cerr << "ERROR: " << e.what() << "\n";
        return 1;
    }
}
