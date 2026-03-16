/** @file InsCatalog.h
 * @brief ISO 7816-4, PC/SC Part 3 ve DESFire INS (instruction) kataloğu.
 *
 * Her INS girişi CLA bağlamı ile birlikte tanımlanmıştır.
 *
 * Kaynaklar:
 *   - ISO/IEC 7816-4:2020 Table 4
 *   - PC/SC Workgroup Specification Part 3 v2.01
 *   - NXP AN10927 DESFire EV1/EV2/EV3
 *   - NXP MF1S503x Mifare Classic
 */
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <optional>

/** @brief INS kataloğu giriş tipi. */
struct InsEntry {
    uint8_t     cla;        ///< CLA bağlamı (0x00=ISO, 0xFF=PC/SC, 0x90=DESFire, 0xFF=any)
    uint8_t     ins;        ///< INS byte
    std::string code;       ///< Hex string (örn. "B0")
    std::string name;       ///< Kısa isim
    std::string description;///< Detaylı açıklama
    std::string standard;   ///< Referans standart
    std::string group;      ///< Grup: ISO/PCSC/DESFire
};

/** @brief INS kataloğu. */
class InsCatalog {
public:
    /** @brief CLA + INS'e göre giriş ara.
     * @param cla CLA byte
     * @param ins INS byte
     * @return Bulunan giriş veya nullopt
     */
    static std::optional<InsEntry> lookup(uint8_t cla, uint8_t ins) {
        for (const auto& e : table())
            if (e.cla == cla && e.ins == ins)
                return e;
        return std::nullopt;
    }

    /** @brief INS hex string ile ara (CLA belirsizse). */
    static std::optional<InsEntry> lookupIns(const std::string& insHex, uint8_t cla = 0x00) {
        try {
            uint8_t ins = static_cast<uint8_t>(std::stoi(insHex, nullptr, 16));
            return lookup(cla, ins);
        } catch (...) { return std::nullopt; }
    }

    /** @brief Gruba göre filtrele ("ISO", "PCSC", "DESFire"). */
    static std::vector<InsEntry> byGroup(const std::string& group) {
        std::vector<InsEntry> r;
        for (const auto& e : table())
            if (e.group == group) r.push_back(e);
        return r;
    }

    /** @brief Tüm giriş listesi. */
    static const std::vector<InsEntry>& table() {
        static const std::vector<InsEntry> T = {

        // ── ISO 7816-4 CLA=0x00 ──────────────────────────────────────────────

        {0x00,0x01,"01","CREATE FILE (DESFire legacy)","Create EF or DF.",
         "ISO 7816-4","ISO"},
        {0x00,0x04,"04","DEACTIVATE FILE","Deactivate an EF.",
         "ISO 7816-4","ISO"},
        {0x00,0x0E,"0E","ERASE BINARY","Erase transparent EF contents.",
         "ISO 7816-4","ISO"},
        {0x00,0x0F,"0F","ERASE RECORD","Erase one or more records.",
         "ISO 7816-4","ISO"},
        {0x00,0x20,"20","VERIFY","Verify PIN/password.",
         "ISO 7816-4","ISO"},
        {0x00,0x21,"21","VERIFY (odd)","Verify with odd INS variant.",
         "ISO 7816-4","ISO"},
        {0x00,0x22,"22","MANAGE SECURITY ENVIRONMENT","Set security environment for crypto ops.",
         "ISO 7816-4","ISO"},
        {0x00,0x24,"24","CHANGE REFERENCE DATA","Change PIN or key reference data.",
         "ISO 7816-4","ISO"},
        {0x00,0x26,"26","DISABLE VERIFICATION","Disable PIN verification requirement.",
         "ISO 7816-4","ISO"},
        {0x00,0x28,"28","ENABLE VERIFICATION","Re-enable PIN verification requirement.",
         "ISO 7816-4","ISO"},
        {0x00,0x2A,"2A","PERFORM SECURITY OPERATION","Sign, decrypt or encrypt data.",
         "ISO 7816-4","ISO"},
        {0x00,0x2C,"2C","RESET RETRY COUNTER","Unblock a blocked PIN.",
         "ISO 7816-4","ISO"},
        {0x00,0x44,"44","ACTIVATE FILE","Activate a deactivated EF.",
         "ISO 7816-4","ISO"},
        {0x00,0x46,"46","GENERATE ASYMMETRIC KEY PAIR","Generate RSA/ECC key pair.",
         "ISO 7816-4","ISO"},
        {0x00,0x70,"70","MANAGE CHANNEL","Open or close a logical channel.",
         "ISO 7816-4","ISO"},
        {0x00,0x82,"82","EXTERNAL / MUTUAL AUTHENTICATE","External auth or mutual auth step 2.",
         "ISO 7816-4","ISO"},
        {0x00,0x84,"84","GET CHALLENGE","Request random number from card.",
         "ISO 7816-4","ISO"},
        {0x00,0x86,"86","GENERAL AUTHENTICATE","General purpose authentication.",
         "ISO 7816-4","ISO"},
        {0x00,0x87,"87","GENERAL AUTHENTICATE (odd)","General auth odd variant.",
         "ISO 7816-4","ISO"},
        {0x00,0x88,"88","INTERNAL AUTHENTICATE","Card proves knowledge of key.",
         "ISO 7816-4","ISO"},
        {0x00,0xA0,"A0","SEARCH BINARY","Search transparent EF for pattern.",
         "ISO 7816-4","ISO"},
        {0x00,0xA1,"A1","SEARCH BINARY (odd)","Search binary odd variant.",
         "ISO 7816-4","ISO"},
        {0x00,0xA2,"A2","SEARCH RECORD","Search record EF for pattern.",
         "ISO 7816-4","ISO"},
        {0x00,0xA4,"A4","SELECT","Select file or application by ID/AID/path.",
         "ISO 7816-4","ISO"},
        {0x00,0xB0,"B0","READ BINARY","Read data from transparent EF.",
         "ISO 7816-4","ISO"},
        {0x00,0xB1,"B1","READ BINARY (odd)","Read binary with extended offset.",
         "ISO 7816-4","ISO"},
        {0x00,0xB2,"B2","READ RECORD","Read one or more records from record EF.",
         "ISO 7816-4","ISO"},
        {0x00,0xB3,"B3","READ RECORD (odd)","Read record odd variant.",
         "ISO 7816-4","ISO"},
        {0x00,0xC0,"C0","GET RESPONSE","Retrieve remaining response bytes (after 61xx).",
         "ISO 7816-4","ISO"},
        {0x00,0xC2,"C2","ENVELOPE","Encapsulate command in envelope.",
         "ISO 7816-4","ISO"},
        {0x00,0xC3,"C3","ENVELOPE (odd)","Envelope odd variant.",
         "ISO 7816-4","ISO"},
        {0x00,0xCA,"CA","GET DATA","Retrieve data object by tag.",
         "ISO 7816-4","ISO"},
        {0x00,0xCB,"CB","GET DATA (odd)","Get data with extended addressing.",
         "ISO 7816-4","ISO"},
        {0x00,0xD0,"D0","WRITE BINARY","Write to transparent EF (binary OR).",
         "ISO 7816-4","ISO"},
        {0x00,0xD1,"D1","WRITE BINARY (odd)","Write binary odd variant.",
         "ISO 7816-4","ISO"},
        {0x00,0xD2,"D2","WRITE RECORD","Write record to record EF.",
         "ISO 7816-4","ISO"},
        {0x00,0xD6,"D6","UPDATE BINARY","Update (overwrite) transparent EF.",
         "ISO 7816-4","ISO"},
        {0x00,0xD7,"D7","UPDATE BINARY (odd)","Update binary with extended offset.",
         "ISO 7816-4","ISO"},
        {0x00,0xDA,"DA","PUT DATA","Store data object by tag.",
         "ISO 7816-4","ISO"},
        {0x00,0xDB,"DB","PUT DATA (odd)","Put data odd variant.",
         "ISO 7816-4","ISO"},
        {0x00,0xDC,"DC","UPDATE RECORD","Update an existing record.",
         "ISO 7816-4","ISO"},
        {0x00,0xDD,"DD","UPDATE RECORD (odd)","Update record odd variant.",
         "ISO 7816-4","ISO"},
        {0x00,0xE0,"E0","CREATE FILE","Create a new EF or DF.",
         "ISO 7816-4","ISO"},
        {0x00,0xE2,"E2","APPEND RECORD","Append record to cyclic/linear record EF.",
         "ISO 7816-4","ISO"},
        {0x00,0xE4,"E4","DELETE FILE","Delete EF or DF.",
         "ISO 7816-4","ISO"},
        {0x00,0xE6,"E6","TERMINATE DF","Permanently terminate a DF.",
         "ISO 7816-4","ISO"},
        {0x00,0xE8,"E8","TERMINATE EF","Permanently terminate an EF.",
         "ISO 7816-4","ISO"},
        {0x00,0xFE,"FE","TERMINATE CARD USAGE","Permanently terminate card.",
         "ISO 7816-4","ISO"},

        // ── PC/SC CLA=0xFF ───────────────────────────────────────────────────

        {0xFF,0x00,"FF00","ESCAPE (vendor-specific)","Reader-specific vendor command (LED, buzzer, firmware).",
         "PC/SC Part 3","PCSC"},
        {0xFF,0x20,"FF20","VERIFY PIN","Verify cardholder PIN via reader.",
         "PC/SC Part 3","PCSC"},
        {0xFF,0x24,"FF24","CHANGE PIN","Change or unblock cardholder PIN.",
         "PC/SC Part 3","PCSC"},
        {0xFF,0x70,"FF70","MANAGE SESSION","Manage T=CL session / transparent session.",
         "PC/SC Part 3","PCSC"},
        {0xFF,0x82,"FF82","LOAD KEY","Load Mifare key into reader key slot.",
         "PC/SC Part 3","PCSC"},
        {0xFF,0x86,"FF86","GENERAL AUTHENTICATE","Authenticate using new format (Mifare).",
         "PC/SC Part 3","PCSC"},
        {0xFF,0x88,"FF88","AUTHENTICATE (legacy)","Authenticate using legacy Mifare format.",
         "PC/SC Part 3","PCSC"},
        {0xFF,0xB0,"FFB0","READ BINARY","Read page or block from contactless card.",
         "PC/SC Part 3","PCSC"},
        {0xFF,0xB1,"FFB1","READ BINARY (odd)","Read binary long form (>256 bytes).",
         "PC/SC Part 3","PCSC"},
        {0xFF,0xCA,"FFCA","GET DATA","Get UID (P1=00) or ATS/historical bytes (P1=01).",
         "PC/SC Part 3","PCSC"},
        {0xFF,0xD6,"FFD6","UPDATE BINARY","Write to page or block of contactless card.",
         "PC/SC Part 3","PCSC"},
        {0xFF,0xD7,"FFD7","UPDATE BINARY (odd)","Write binary long form.",
         "PC/SC Part 3","PCSC"},

        // ── DESFire native CLA=0x90 ──────────────────────────────────────────

        {0x90,0x02,"9002","CREATE BACKUP FILE","Create backup data file.",
         "NXP AN10927","DESFire"},
        {0x90,0x05,"9005","CREDIT","Increment value file by amount.",
         "NXP AN10927","DESFire"},
        {0x90,0x09,"9009","CHANGE FILE SETTINGS","Modify file access rights.",
         "NXP AN10927","DESFire"},
        {0x90,0x0C,"900C","CLEAR RECORD FILE","Delete all records in record file.",
         "NXP AN10927","DESFire"},
        {0x90,0x0F,"900F","COMMIT TRANSACTION","Commit all pending write operations.",
         "NXP AN10927","DESFire"},
        {0x90,0x1A,"901A","AUTHENTICATE (DES/3DES)","Authenticate with DES or 3DES key.",
         "NXP AN10927","DESFire"},
        {0x90,0x1C,"901C","DEBIT","Decrement value file by amount.",
         "NXP AN10927","DESFire"},
        {0x90,0x27,"9027","CHANGE KEY SETTINGS","Modify application key settings.",
         "NXP AN10927","DESFire"},
        {0x90,0x2A,"902A","SET CONFIGURATION","Configure card-level settings.",
         "NXP AN10927","DESFire"},
        {0x90,0x31,"9031","CREATE LINEAR RECORD FILE","Create linear record file.",
         "NXP AN10927","DESFire"},
        {0x90,0x35,"9035","CREATE CYCLIC RECORD FILE","Create cyclic record file.",
         "NXP AN10927","DESFire"},
        {0x90,0x3B,"903B","WRITE RECORD","Write data to record file.",
         "NXP AN10927","DESFire"},
        {0x90,0x3D,"903D","WRITE DATA","Write data to standard/backup data file.",
         "NXP AN10927","DESFire"},
        {0x90,0x40,"9040","INITIALIZE VALUE FILE","Initialize value file with value.",
         "NXP AN10927","DESFire"},
        {0x90,0x44,"9044","CREATE VALUE FILE","Create a value file.",
         "NXP AN10927","DESFire"},
        {0x90,0x45,"9045","AUTHENTICATE (AES)","Authenticate with AES-128 key.",
         "NXP AN10927","DESFire"},
        {0x90,0x4F,"904F","FORMAT PICC","Wipe all applications and data from card.",
         "NXP AN10927","DESFire"},
        {0x90,0x51,"9051","GET FREE MEMORY","Get available free memory on card.",
         "NXP AN10927","DESFire"},
        {0x90,0x54,"9054","READ RECORDS","Read one or more records from record file.",
         "NXP AN10927","DESFire"},
        {0x90,0x57,"9057","CHANGE KEY","Replace a key in an application.",
         "NXP AN10927","DESFire"},
        {0x90,0x5A,"905A","SELECT APPLICATION","Select application by AID (3 bytes).",
         "NXP AN10927","DESFire"},
        {0x90,0x60,"9060","GET VERSION","Get hardware/software version + UID (3 frames).",
         "NXP AN10927","DESFire"},
        {0x90,0x61,"9061","AUTHENTICATE ISO","Authenticate with 3K3DES ISO key.",
         "NXP AN10927","DESFire"},
        {0x90,0x64,"9064","GET KEY SETTINGS","Get key settings and number of keys.",
         "NXP AN10927","DESFire"},
        {0x90,0x65,"9065","GET KEY VERSION","Get version of a specific key.",
         "NXP AN10927","DESFire"},
        {0x90,0x6A,"906A","GET APPLICATION IDs","Get list of all application IDs on card.",
         "NXP AN10927","DESFire"},
        {0x90,0x6C,"906C","GET VALUE","Read current value from value file.",
         "NXP AN10927","DESFire"},
        {0x90,0x6D,"906D","LIMITED CREDIT","Credit value file with limited amount.",
         "NXP AN10927","DESFire"},
        {0x90,0x6E,"906E","GET FREE MEMORY (alt)","Alternative get free memory command.",
         "NXP AN10927","DESFire"},
        {0x90,0x6F,"906F","GET FILE IDs","List all file IDs in selected application.",
         "NXP AN10927","DESFire"},
        {0x90,0x8D,"908D","CREATE APPLICATION","Create new application with AID.",
         "NXP AN10927","DESFire"},
        {0x90,0x9A,"909A","DEBIT","Debit value file (alias).",
         "NXP AN10927","DESFire"},
        {0x90,0xAA,"90AA","AUTHENTICATE EV2 (AES)","EV2 AES first authentication.",
         "NXP AN10927","DESFire"},
        {0x90,0xAF,"90AF","ADDITIONAL FRAME","Continue multi-frame operation (response to 91AF).",
         "NXP AN10927","DESFire"},
        {0x90,0xBD,"90BD","READ DATA","Read data from standard/backup data file.",
         "NXP AN10927","DESFire"},
        {0x90,0xCA,"90CA","CREATE APPLICATION (alt)","Create application (alternative).",
         "NXP AN10927","DESFire"},
        {0x90,0xCD,"90CD","CHANGE CONFIGURATION","Modify card or application configuration.",
         "NXP AN10927","DESFire"},
        {0x90,0xDA,"90DA","CHANGE KEY (extended)","Change key with explicit key number.",
         "NXP AN10927","DESFire"},
        {0x90,0xDF,"90DF","DELETE APPLICATION","Permanently delete application.",
         "NXP AN10927","DESFire"},
        {0x90,0xF5,"90F5","GET FILE SETTINGS","Get access rights and file type info.",
         "NXP AN10927","DESFire"},
        {0x90,0xFA,"90FA","ABORT TRANSACTION","Abort pending transaction.",
         "NXP AN10927","DESFire"},
        {0x90,0xFC,"90FC","FORMAT CARD","Format card — wipe all data.",
         "NXP AN10927","DESFire"},
        }; // end table
        return T;
    }
};
