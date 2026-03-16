/** @file StatusWordCatalog.h
 * @brief ISO 7816-4 ve DESFire Status Word tam kataloğu.
 *
 * SW1/SW2 çiftleri için isim, açıklama, kategori ve çözüm önerileri.
 * Wildcard SW2 (0xFF) ile SW1 bazlı eşleşme desteklenir.
 *
 * Kaynaklar:
 *   - ISO/IEC 7816-4:2020 Table 5-6
 *   - PC/SC Workgroup Specification Part 3
 *   - NXP AN10927 (DESFire EV1/EV2/EV3)
 */
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <optional>
#include <sstream>
#include <iomanip>

/// SW2 wildcard: SW1 eşleşirse geçerli, SW2 farklı olabilir
static constexpr uint8_t SW2_ANY = 0xFF;

/** @brief Tek bir Status Word girişi. */
struct SwEntry {
    uint8_t     sw1;          ///< SW1 byte (örn. 0x69)
    uint8_t     sw2;          ///< SW2 byte; SW2_ANY = wildcard
    std::string code;         ///< İnsan okunur kod (örn. "6982")
    std::string name;         ///< Kısa isim
    std::string description;  ///< Detaylı açıklama
    std::string category;     ///< Kategori: Success/Info/Warning/Security/Error/Format/DESFire
    std::string resolution;   ///< Çözüm önerisi (opsiyonel)
    std::vector<std::string> seealso; ///< İlgili SW kodları
};

/** @brief ISO 7816-4 + DESFire Status Word kataloğu. */
class StatusWordCatalog {
public:
    /** @brief SW1+SW2'ye göre en iyi eşleşmeyi döndürür.
     *
     * Arama sırası:
     *   1. Tam eşleşme (sw1 + sw2)
     *   2. Wildcard eşleşme (sw1 + SW2_ANY)
     *
     * @param sw1 Status Word byte 1
     * @param sw2 Status Word byte 2
     * @return Bulunan giriş veya nullopt
     */
    static std::optional<SwEntry> lookup(uint8_t sw1, uint8_t sw2) {
        const auto& tbl = table();
        // Önce tam eşleşme
        for (const auto& e : tbl)
            if (e.sw1 == sw1 && e.sw2 == sw2)
                return e;
        // Sonra wildcard
        for (const auto& e : tbl)
            if (e.sw1 == sw1 && e.sw2 == SW2_ANY)
                return e;
        return std::nullopt;
    }

    /** @brief Hex string'den SW lookup (örn. "6982").
     *  @return nullopt hem "bulunamadı" hem "geçersiz format" durumunda
     *  @note Geçersiz hex için isValidHex() ile önceden kontrol edin. */
    static std::optional<SwEntry> lookup(const std::string& hex) {
        if (hex.size() != 4) return std::nullopt;
        // Validate all chars are hex
        for (char c : hex)
            if (!std::isxdigit(static_cast<unsigned char>(c))) return std::nullopt;
        try {
            uint8_t sw1 = static_cast<uint8_t>(std::stoi(hex.substr(0,2), nullptr, 16));
            uint8_t sw2 = static_cast<uint8_t>(std::stoi(hex.substr(2,2), nullptr, 16));
            return lookup(sw1, sw2);
        } catch (...) { return std::nullopt; }
    }

    /** @brief SW string'in geçerli 4-char hex olup olmadığını kontrol eder. */
    static bool isValidHex(const std::string& hex) {
        if (hex.size() != 4) return false;
        for (char c : hex)
            if (!std::isxdigit(static_cast<unsigned char>(c))) return false;
        return true;
    }

    /** @brief SW başarılı mı? (9000 veya 91 00) */
    static bool isSuccess(uint8_t sw1, uint8_t sw2) {
        return (sw1 == 0x90 && sw2 == 0x00) ||
               (sw1 == 0x91 && sw2 == 0x00);
    }

    /** @brief Kategoriye göre filtrele. */
    static std::vector<SwEntry> byCategory(const std::string& cat) {
        std::vector<SwEntry> r;
        for (const auto& e : table())
            if (e.category == cat) r.push_back(e);
        return r;
    }

    /** @brief Tüm giriş listesi. */
    static const std::vector<SwEntry>& table() {
        static const std::vector<SwEntry> T = {

        // ── Success ──────────────────────────────────────────────────────────
        {0x90,0x00,"9000","Normal processing","Command completed successfully.",
         "Success","",{"6100","9100"}},
        {0x91,0x00,"9100","DESFire OK","DESFire command completed successfully.",
         "DESFire","",{"9000"}},

        // ── Info / Continuation ───────────────────────────────────────────────
        {0x61,SW2_ANY,"61xx","Response bytes remaining",
         "xx bytes of response data still available. Send GET RESPONSE (00 C0 00 00 xx).",
         "Info","Send GET RESPONSE: scardtool send-apdu -a GET_RESPONSE:<xx>",{"9000","6C00"}},
        {0x91,0xAF,"91AF","DESFire additional frame",
         "More data available. Send ADDITIONAL FRAME (90 AF 00 00 00) to continue.",
         "DESFire","Send: scardtool send-apdu -a DESFIRE_MOREDATA",{"9100"}},

        // ── Warning — state unchanged ─────────────────────────────────────────
        {0x62,0x00,"6200","No information given (warning)","State unchanged, no info.",
         "Warning","",{}},
        {0x62,0x01,"6201","Part of returned data may be corrupted",
         "Returned data may be unreliable.","Warning","Retry the operation.",{}},
        {0x62,0x02,"6202","End of file / record reached before Le bytes",
         "File shorter than expected Le length.","Warning","Adjust Le value.",{}},
        {0x62,0x03,"6203","Selected file deactivated",
         "File is deactivated and cannot be used.","Warning","Activate file first.",{}},
        {0x62,0x04,"6204","File control info not formatted correctly",
         "FCI TLV structure is malformed.","Warning","",{}},
        {0x62,0x05,"6205","Selected file in termination state",
         "File has been terminated and is permanently unusable.","Warning","",{}},
        {0x62,0x06,"6206","No input data available from a sensor",
         "Sensor data unavailable.","Warning","",{}},
        {0x62,0x81,"6281","Returned data may be corrupted",
         "Response integrity uncertain.","Warning","Retry.",{}},
        {0x62,0x82,"6282","End of file reached before Le bytes read",
         "Less data than requested.","Warning","Reduce Le.",{}},
        {0x62,0x83,"6283","Selected file invalidated",
         "File has been invalidated/deactivated.","Warning","",{}},
        {0x62,0x84,"6284","FCI not formatted per 5.1.5",
         "File control info format error.","Warning","",{}},
        {0x62,0x85,"6285","Selected file in termination state",
         "File permanently terminated.","Warning","",{}},
        {0x62,0xF1,"62F1","More data available","Response truncated, more follows.",
         "Warning","Send GET RESPONSE.",{"61xx"}},
        {0x62,0xF2,"62F2","More data available, SW2 indicates count",
         "Partial response.","Warning","",{}},

        // ── Warning — state changed ────────────────────────────────────────────
        {0x63,0x00,"6300","No information given / Auth sentinel",
         "Warning with state change. In Mifare context: authentication required.",
         "Warning","Run load-key then auth.",{"6982"}},
        {0x63,0x81,"6381","File filled up by last write",
         "File at capacity after this write.","Warning","",{}},
        {0x63,SW2_ANY,"63Cx","Verification failed, x retries remaining",
         "PIN/key verification failed. SW2 low nibble = retries left (0x63 0xCx).",
         "Security","Check key/PIN value. x=0 means blocked.",{"6983"}},

        // ── Execution error — state unchanged ─────────────────────────────────
        {0x64,0x00,"6400","Execution error (state unchanged)",
         "Command failed, card state not modified.","Error","Retry.",{}},
        {0x64,0x01,"6401","Immediate response required",
         "Card requires immediate response.","Error","",{}},
        {0x64,0x81,"6481","Execution error (state unchanged)",
         "Non-volatile memory write error.","Error","Check power/contact quality.",{}},

        // ── Execution error — state changed ───────────────────────────────────
        {0x65,0x00,"6500","Execution error (state changed)",
         "Command failed and card state may have changed.","Error","",{}},
        {0x65,0x01,"6501","Memory failure (state changed)",
         "Non-volatile memory error, state undefined.","Error","",{}},
        {0x65,0x81,"6581","Memory failure","NVM write failure during command.",
         "Error","Check power stability.",{}},

        // ── Security errors ────────────────────────────────────────────────────
        {0x66,0x00,"6600","Security-related issues","Unspecified security error.",
         "Security","",{}},
        {0x66,0x87,"6687","Expected SM data objects missing","Secure messaging required.",
         "Security","",{}},
        {0x66,0x88,"6688","Incorrect SM data objects","Secure messaging error.",
         "Security","",{}},

        // ── Format errors ──────────────────────────────────────────────────────
        {0x67,0x00,"6700","Wrong length (Lc or Le)","Lc/Le value incorrect for command.",
         "Format","Check APDU length field.",{}},

        // ── CLA not supported ─────────────────────────────────────────────────
        {0x68,0x00,"6800","No information given (CLA)",
         "CLA byte not supported.","Format","",{}},
        {0x68,0x81,"6881","Logical channel not supported",
         "Card doesn't support logical channels.","Format","Use channel 0.",{}},
        {0x68,0x82,"6882","Secure messaging not supported",
         "Card doesn't support secure messaging.","Format","",{}},
        {0x68,0x83,"6883","Last command of the chain expected",
         "Command chaining error.","Format","",{}},
        {0x68,0x84,"6884","Command chaining not supported",
         "Card doesn't support command chaining.","Format","",{}},

        // ── Command not allowed ────────────────────────────────────────────────
        {0x69,0x00,"6900","No information given (command)",
         "Command not allowed, no specific reason.","Security","",{}},
        {0x69,0x01,"6901","Command incompatible with file structure",
         "Command not applicable to selected file type.","Security","Select correct file.",{}},
        {0x69,0x81,"6981","Command incompatible with file structure",
         "Wrong file type for this command.","Security","",{}},
        {0x69,0x82,"6982","Security status not satisfied",
         "Authentication required before this command. Key not loaded or wrong key.",
         "Security",
         "Run: scardtool load-key -r 0 -k <key>\n       scardtool auth -r 0 -b <block> -t A",
         {"6300","6983","6985"}},
        {0x69,0x83,"6983","Authentication method blocked",
         "Key/PIN is blocked after too many failed attempts.",
         "Security","Key permanently blocked. Card may need reset.",{"63Cx","6982"}},
        {0x69,0x84,"6984","Reference data not usable",
         "Referenced key/data invalid or expired.","Security","",{}},
        {0x69,0x85,"6985","Conditions of use not satisfied",
         "Preconditions for command not met (wrong state, wrong mode).",
         "Security","Check card state and access conditions.",{"6982"}},
        {0x69,0x86,"6986","Command not allowed (no current EF)",
         "No EF selected, or command requires EF selection.",
         "Security","Select a file first.",{}},
        {0x69,0x87,"6987","Expected secure messaging data objects missing",
         "SM required but not provided.","Security","",{}},
        {0x69,0x88,"6988","Incorrect secure messaging data objects",
         "SM data present but incorrect.","Security","",{}},
        {0x69,0x8D,"698D","Reserved","Reserved.","Security","",{}},

        // ── Wrong parameters ───────────────────────────────────────────────────
        {0x6A,0x00,"6A00","No information given (P1-P2)","Bad parameters, no detail.",
         "Parameter","",{}},
        {0x6A,0x80,"6A80","Incorrect data in command data field",
         "Data field contains invalid content.","Parameter","Check data bytes.",{}},
        {0x6A,0x81,"6A81","Function not supported",
         "Requested function not available on this card.","Parameter","",{}},
        {0x6A,0x82,"6A82","File or application not found",
         "Referenced file/app does not exist.","Parameter",
         "Check AID or file ID. Run: scardtool uid -r 0",{}},
        {0x6A,0x83,"6A83","Record not found",
         "No record matching criteria.","Parameter","",{}},
        {0x6A,0x84,"6A84","Not enough memory space in the file",
         "Insufficient space for write operation.","Parameter","",{}},
        {0x6A,0x85,"6A85","Lc inconsistent with TLV structure",
         "Length field conflicts with data structure.","Parameter","",{}},
        {0x6A,0x86,"6A86","Incorrect parameters P1-P2",
         "P1 or P2 value invalid for this command.","Parameter","Check APDU parameters.",{}},
        {0x6A,0x87,"6A87","Lc inconsistent with P1-P2",
         "Data length not consistent with parameters.","Parameter","",{}},
        {0x6A,0x88,"6A88","Reference data (key/PIN) not found",
         "Key slot empty or key number invalid.","Parameter",
         "Load key first: scardtool load-key -r 0 -k <key>",{}},
        {0x6A,0x89,"6A89","File already exists",
         "Cannot create file — already present.","Parameter","Delete or use existing file.",{}},
        {0x6A,0x8A,"6A8A","DF name already exists",
         "Application/DF name already used.","Parameter","",{}},

        {0x6B,0x00,"6B00","Wrong parameters P1-P2 (offset out of range)",
         "P1-P2 offset exceeds file/memory boundary.","Parameter",
         "Check page/block number is within range.",{}},

        // ── Wrong Le ──────────────────────────────────────────────────────────
        {0x6C,SW2_ANY,"6Cxx","Wrong Le, correct Le = xx",
         "Le byte incorrect. SW2 = correct Le value to use.",
         "Format","Retry with Le=SW2 value.",{"61xx"}},

        // ── INS not supported ─────────────────────────────────────────────────
        {0x6D,0x00,"6D00","INS not supported or invalid",
         "Instruction byte not recognized by card.",
         "Format","Check INS byte. Wrong card type?",{}},

        // ── CLA not supported ─────────────────────────────────────────────────
        {0x6E,0x00,"6E00","CLA not supported",
         "Class byte not supported.","Format","Check CLA byte.",{}},

        // ── Unknown ───────────────────────────────────────────────────────────
        {0x6F,0x00,"6F00","No precise diagnosis",
         "Unspecified error, no additional info available.",
         "Unknown","Check card and reader. Try reconnecting.",{}},

        // ── DESFire specific ──────────────────────────────────────────────────
        {0x91,0xAE,"91AE","DESFire authentication error",
         "Authentication failed — wrong key or MAC verification failed.",
         "DESFire","Check key value and key number.",{"6982"}},
        {0x91,0x9D,"919D","DESFire permission denied",
         "Access rights insufficient for requested operation.",
         "DESFire","Check application access rights.",{"6985"}},
        {0x91,0xA0,"91A0","DESFire application not found",
         "Requested AID not found on card.",
         "DESFire","Check AID. Run: scardtool send-apdu -a DESFIRE_GETAPPIDS",{"6A82"}},
        {0x91,0xF0,"91F0","DESFire file not found",
         "File ID not found in selected application.",
         "DESFire","Run: scardtool send-apdu -a DESFIRE_GETFILEIDS",{"6A82"}},
        {0x91,0xBE,"91BE","DESFire out of boundary",
         "Read/write offset + length exceeds file size.",
         "DESFire","Check offset and length parameters.",{}},
        {0x91,0xDE,"91DE","DESFire duplicate element",
         "AID or file already exists.",
         "DESFire","Use a different AID/file number.",{"6A89"}},
        {0x91,0xEE,"91EE","DESFire memory error",
         "Non-volatile memory error on DESFire card.",
         "DESFire","",{}},
        {0x91,0x1C,"911C","DESFire illegal command code",
         "INS byte not valid for DESFire.","DESFire","",{}},
        {0x91,0x1E,"911E","DESFire integrity error",
         "MAC or CRC check failed.","DESFire","Check secure messaging setup.",{}},
        {0x91,0x40,"9140","DESFire no such key",
         "Key number does not exist in application.","DESFire","",{}},
        {0x91,0x7E,"917E","DESFire length error",
         "Data length out of range.","DESFire","",{}},
        {0x91,0x97,"9197","DESFire integrity error (crypto)",
         "Crypto/MAC verification failed.","DESFire","Check key and auth state.",{}},
        {0x91,0xCA,"91CA","DESFire illegal command",
         "Command not applicable in current state.","DESFire","",{}},
        }; // end table
        return T;
    }

    /** @brief Kısa özet string (send-apdu çıktısı için). */
    static std::string shortDesc(uint8_t sw1, uint8_t sw2) {
        auto e = lookup(sw1, sw2);
        if (!e) return "";
        std::string d = e->name;
        if (e->sw2 == SW2_ANY && sw2 != SW2_ANY) {
            std::ostringstream oss;
            oss << std::hex << std::uppercase << std::setfill('0')
                << std::setw(2) << (int)sw2;
            std::string hexVal = oss.str();
            // name'de xx varsa değiştir
            size_t p = d.find("xx");
            if (p != std::string::npos) {
                d.replace(p, 2, hexVal);
            } else {
                // name'de yoksa değeri sonuna ekle
                d += " (0x" + hexVal + ")";
            }
        }
        return d;
    }
};
