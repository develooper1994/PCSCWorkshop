/** @file MacroRegistry.h
 * @brief APDU makro kaydı, yükleme ve çözümleme.
 *
 * Makro arama sırası:
 *   1. Built-in katalog (bu dosyada)
 *   2. Proje bazlı (./.scard_macros)
 *   3. Global kullanıcı (~/.scard_macros)
 *
 * .scard_macros formatı:
 * @code
 *   # yorum
 *   MY_SELECT    = 00 A4 04 00 05 12 34 56 78 9A 00
 *   MY_READ:page = FF B0 00 {page} 10
 * @endcode
 */
#pragma once
#include "ApduMacro.h"
#include <unordered_map>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <algorithm>
#include <cctype>
#include <deque>
#include <mutex>

namespace fs = std::filesystem;

/** @brief Makro kaydı ve çözümleyici. */
class MacroRegistry {
public:
    /** @brief Singleton instance. */
    static MacroRegistry& instance() {
        static MacroRegistry reg;
        return reg;
    }

    /** @brief Makro adına göre arar (büyük/küçük harf duyarsız).
     * @param name Makro adı (örn. "GETUID", "READ_BINARY")
     * @return Bulunan makro veya nullopt
     */
    std::optional<ApduMacro> find(const std::string& name) const {
        auto key = toUpper(name);
        auto it = index_.find(key);
        if (it != index_.end()) return *it->second;
        return std::nullopt;
    }

    /** @brief Makro çağrısını çöz: "GETUID" veya "READ_BINARY:4:16" → hex APDU.
     *
     * Makro bulunamazsa veya input zaten hex APDU ise passthrough.
     *
     * @param input Makro adı (parametreli veya parametresiz) veya hex string
     * @param[out] resolved Çözümlenmiş hex APDU
     * @param[out] macroUsed Kullanılan makro (loglama için)
     * @return true → çözümlendi, false → hex değildi ve makro bulunamadı
     */
    bool resolve(const std::string& input,
                 std::string& resolved,
                 std::optional<ApduMacro>* macroUsed = nullptr) const {
        // Zaten hex APDU mi?
        std::string stripped;
        for (char c : input) if (c != ' ') stripped += c;
        if (isHexApdu(stripped)) {
            resolved = toUpper(stripped);
            if (macroUsed) *macroUsed = std::nullopt;
            return true;
        }

        // Makro parse et
        std::string macroName;
        std::vector<std::string> args;
        parseMacroCall(input, macroName, args);

        auto macro = find(macroName);
        if (!macro) return false;

        try {
            if (args.empty() && macro->params.empty())
                resolved = macro->resolveFixed();
            else
                resolved = macro->resolve(args);
            if (macroUsed) *macroUsed = macro;
            return true;
        } catch (const std::exception& e) {
            throw std::invalid_argument(std::string(e.what()));
        }
    }

    /** @brief Gruba göre tüm makroları döndür. */
    std::vector<const ApduMacro*> byGroup(const std::string& group) const {
        std::vector<const ApduMacro*> r;
        for (const auto& m : macros_)
            if (group.empty() || m.group == group) r.push_back(&m);
        return r;
    }

    /** @brief Tüm makrolar. */
    const std::deque<ApduMacro>& all() const { return macros_; }

    /** @brief .scard_macros dosyasından kullanıcı makrolarını yükle.
     *
     * Çakışma varsa kullanıcı makrosu built-in'i override eder.
     * @param path Dosya yolu
     * @return Yüklenen makro sayısı
     */
    int loadFile(const std::string& path) {
        std::ifstream f(path);
        if (!f.is_open()) return 0;

        int count = 0;
        std::string line;
        while (std::getline(f, line)) {
            auto trimmed = trim(line);
            if (trimmed.empty() || trimmed[0] == '#') continue;

            // "NAME[:p1[:p2]] = APDU_TEMPLATE"
            auto eq = trimmed.find('=');
            if (eq == std::string::npos) continue;

            std::string lhs = trim(trimmed.substr(0, eq));
            std::string rhs = trim(trimmed.substr(eq + 1));

            // lhs parse: "MY_READ:page" → name="MY_READ", params=["page"]
            std::string macroName;
            std::vector<std::string> params;
            parseMacroCall(lhs, macroName, params);

            if (macroName.empty() || rhs.empty()) continue;

            ApduMacro m;
            m.name         = toUpper(macroName);
            m.apduTemplate = rhs;
            m.params       = params;
            m.description  = "User-defined macro";
            m.group        = "User";
            m.standard     = ".scard_macros";
            addOrReplace(m);
            ++count;
        }
        return count;
    }

    /** @brief Varsayılan konumlardan kullanıcı makrolarını yükle.
     *
     * Sıra: ./.scard_macros → ~/.scard_macros
     * Thread-safe: mutex korumalı, birden fazla MCP call aynı anda çağırabilir.
     * @return Toplam yüklenen makro sayısı
     */
    int loadDefaults() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (defaultsLoaded_) return 0; // Zaten yüklendi
        int total = 0;
        total += loadFile(".scard_macros");
        const char* home =
#ifdef _WIN32
            std::getenv("USERPROFILE");
#else
            std::getenv("HOME");
#endif
        if (home) {
            total += loadFile(std::string(home) + "/.scard_macros");
        }
        defaultsLoaded_ = true;
        return total;
    }

    /** @brief Kullanıcı makro sayısı. */
    int userMacroCount() const {
        int n = 0;
        for (const auto& m : macros_) if (m.group == "User") ++n;
        return n;
    }

private:
    MacroRegistry() { initBuiltins(); }

    std::deque<ApduMacro> macros_;
    std::unordered_map<std::string, const ApduMacro*> index_;
    mutable std::mutex mutex_;
    bool defaultsLoaded_ = false;

    void add(ApduMacro m) {
        macros_.push_back(std::move(m));
        index_[toUpper(macros_.back().name)] = &macros_.back();
    }

    void addOrReplace(ApduMacro m) {
        auto key = toUpper(m.name);
        auto it = index_.find(key);
        if (it != index_.end()) {
            // Var olanı güncelle
            auto* ptr = const_cast<ApduMacro*>(it->second);
            *ptr = std::move(m);
        } else {
            add(std::move(m));
        }
    }

    static std::string toUpper(std::string s) {
        for (char& c : s) c = static_cast<char>(std::toupper(c));
        return s;
    }
    static std::string trim(const std::string& s) {
        auto st = s.find_first_not_of(" \t\r\n");
        if (st == std::string::npos) return "";
        auto en = s.find_last_not_of(" \t\r\n");
        return s.substr(st, en - st + 1);
    }

    // ── Built-in makro kataloğu ───────────────────────────────────────────────
    void initBuiltins() {

        // ── General / ISO 7816-4 ─────────────────────────────────────────────
        add({"GETUID",         "FF CA 00 00 00", {}, "Get card UID",
             "General","4-byte (Classic) or 7-byte (UL/DESFire) UID.",
             "scardtool send-apdu -r 0 -a GETUID","PC/SC Part 3",{"GETATS","card-info"}});

        add({"GETATS",         "FF CA 01 00 00", {}, "Get ATS / historical bytes",
             "General","ISO 14443-4 ATS identifies card type and protocol.",
             "scardtool send-apdu -r 0 -a GETATS","PC/SC Part 3",{"GETUID"}});

        add({"GET_CHALLENGE",  "00 84 00 00 08", {}, "Get 8-byte random number",
             "General","Request random challenge from card (ISO 7816-4 INS 84).",
             "scardtool send-apdu -r 0 -a GET_CHALLENGE","ISO 7816-4",{}});

        add({"GET_RESPONSE",   "00 C0 00 00 {len}", {"len"},
             "Get remaining response bytes (after 61xx)",
             "General","Used after SW=61xx. len=SW2 value.",
             "scardtool send-apdu -r 0 -a GET_RESPONSE:0F","ISO 7816-4",{}});

        add({"SELECT_FILE",    "00 A4 00 00 {id}", {"id"},
             "Select file by short ID (2 bytes)",
             "General","Select EF by file ID.",
             "scardtool send-apdu -r 0 -a SELECT_FILE:0102","ISO 7816-4",{}});

        add({"SELECT_APP",     "00 A4 04 00 {aid}", {"aid"},
             "Select application by AID",
             "General","aid = hex AID bytes (up to 16 bytes). Le=00 appended automatically.",
             "scardtool send-apdu -r 0 -a SELECT_APP:D276000085010000","ISO 7816-4",{}});

        add({"SELECT_NDEF",    "00 A4 04 00 07 D2 76 00 00 85 01 00 00", {},
             "Select NDEF application",
             "General","Selects the standard NFC NDEF application (AID D2760000850100).",
             "scardtool send-apdu -r 0 -a SELECT_NDEF","NFC Forum",{"SELECT_APP"}});

        add({"VERIFY_PIN",     "FF 20 00 00 {pin}", {"pin"},
             "Verify cardholder PIN via reader",
             "General","pin = hex PIN bytes. Length auto-calculated in Lc.",
             "scardtool send-apdu -r 0 -a VERIFY_PIN:313233","PC/SC Part 3",{}});

        add({"GET_DATA",       "00 CA {tag}", {"tag"},
             "Get data object by tag (2 hex chars)",
             "General","Retrieve BER-TLV data object.",
             "scardtool send-apdu -r 0 -a GET_DATA:9F36","ISO 7816-4",{}});

        add({"INTERNAL_AUTH",  "00 88 00 00 08 {challenge} 00", {"challenge"},
             "Internal authenticate (card proves key knowledge)",
             "General","challenge = 8 byte hex random.",
             "scardtool send-apdu -r 0 -a INTERNAL_AUTH:0102030405060708","ISO 7816-4",{}});

        // ── PC/SC CLA=FF ──────────────────────────────────────────────────────

        add({"READ_BINARY",    "FF B0 00 {page} {len}", {"page","len"},
             "Read page/block from contactless card",
             "PCSC","page=block number (decimal), len=bytes to read.",
             "scardtool send-apdu -r 0 -a READ_BINARY:4:16","PC/SC Part 3",
             {"WRITE_BINARY","AUTH_A","AUTH_B"}});

        add({"WRITE_BINARY",   "FF D6 00 {page} {len} {data}", {"page","len","data"},
             "Write page/block to contactless card",
             "PCSC","len must match data byte count.",
             "scardtool send-apdu -r 0 -a WRITE_BINARY:4:10:00112233445566778899AABBCCDDEEFF",
             "PC/SC Part 3",{"READ_BINARY"}});

        add({"LOAD_KEY",       "FF 82 20 01 06 {key}", {"key"},
             "Load Mifare key into reader slot 1 (non-volatile)",
             "PCSC","key = 6-byte (12 hex chars) Mifare key. Uses slot 0x01.",
             "scardtool send-apdu -r 0 -a LOAD_KEY:FFFFFFFFFFFF","PC/SC Part 3",
             {"LOAD_KEY_V","AUTH_A","AUTH_B"}});

        add({"LOAD_KEY_V",     "FF 82 00 20 06 {key}", {"key"},
             "Load Mifare key into volatile session slot (slot 0x20)",
             "PCSC","Volatile: lost on disconnect. Safer for temporary auth.",
             "scardtool send-apdu -r 0 -a LOAD_KEY_V:FFFFFFFFFFFF","PC/SC Part 3",
             {"LOAD_KEY"}});

        add({"LOAD_KEY_SLOT",  "FF 82 20 {slot} 06 {key}", {"slot","key"},
             "Load Mifare key into specific reader slot",
             "PCSC","slot = 01..1F (non-volatile). Allows multiple keys.",
             "scardtool send-apdu -r 0 -a LOAD_KEY_SLOT:02:A0A1A2A3A4A5","PC/SC Part 3",
             {"LOAD_KEY","AUTH_A","AUTH_B"}});

        add({"AUTH_A",         "FF 88 00 {block} 60 01", {"block"},
             "Authenticate block with Key A (legacy format)",
             "PCSC","block = block number (decimal). Uses key slot 0x01.",
             "scardtool send-apdu -r 0 -a AUTH_A:4","PC/SC Part 3",
             {"AUTH_B","LOAD_KEY","READ_BINARY"}});

        add({"AUTH_B",         "FF 88 00 {block} 61 01", {"block"},
             "Authenticate block with Key B (legacy format)",
             "PCSC","block = block number (decimal). Uses key slot 0x01.",
             "scardtool send-apdu -r 0 -a AUTH_B:4","PC/SC Part 3",
             {"AUTH_A","LOAD_KEY","READ_BINARY"}});

        add({"AUTH_A_SLOT",    "FF 88 00 {block} 60 {slot}", {"block","slot"},
             "Authenticate with Key A from specific slot",
             "PCSC","","scardtool send-apdu -r 0 -a AUTH_A_SLOT:4:02","PC/SC Part 3",
             {"AUTH_B_SLOT","LOAD_KEY_SLOT"}});

        add({"AUTH_B_SLOT",    "FF 88 00 {block} 61 {slot}", {"block","slot"},
             "Authenticate with Key B from specific slot",
             "PCSC","","scardtool send-apdu -r 0 -a AUTH_B_SLOT:4:02","PC/SC Part 3",
             {"AUTH_A_SLOT","LOAD_KEY_SLOT"}});

        add({"AUTH_GENERAL_A", "FF 86 00 00 05 01 00 {block} 60 01", {"block"},
             "Authenticate with Key A (general authenticate format)",
             "PCSC","Alternative to AUTH_A for some reader models.",
             "scardtool send-apdu -r 0 -a AUTH_GENERAL_A:4","PC/SC Part 3",
             {"AUTH_A","AUTH_GENERAL_B"}});

        add({"AUTH_GENERAL_B", "FF 86 00 00 05 01 00 {block} 61 01", {"block"},
             "Authenticate with Key B (general authenticate format)",
             "PCSC","","scardtool send-apdu -r 0 -a AUTH_GENERAL_B:4","PC/SC Part 3",
             {"AUTH_B","AUTH_GENERAL_A"}});

        // ── DESFire CLA=90 ────────────────────────────────────────────────────

        add({"DESFIRE_GETVERSION", "90 60 00 00 00", {},
             "Get DESFire hardware/software version (frame 1 of 3)",
             "DESFire",
             "Returns 91AF → send DESFIRE_MOREDATA twice. 3 frames total.\n"
             "Frame 1: HW info, Frame 2: SW info, Frame 3: UID+batch+production.",
             "scardtool send-apdu -r 0 -a DESFIRE_GETVERSION","NXP AN10927",
             {"DESFIRE_MOREDATA","card-info"}});

        add({"DESFIRE_MOREDATA",   "90 AF 00 00 00", {},
             "Request additional frame (after SW=91AF)",
             "DESFire","Send after any 91AF response.",
             "scardtool send-apdu -r 0 -a DESFIRE_MOREDATA","NXP AN10927",
             {"DESFIRE_GETVERSION"}});

        add({"DESFIRE_FREEMEM",    "90 6E 00 00 00", {},
             "Get available free memory on card",
             "DESFire","Returns 3-byte little-endian free memory in bytes.",
             "scardtool send-apdu -r 0 -a DESFIRE_FREEMEM","NXP AN10927",{}});

        add({"DESFIRE_GETAPPIDS",  "90 6A 00 00 00", {},
             "Get all application IDs on card",
             "DESFire","Returns list of 3-byte AIDs.",
             "scardtool send-apdu -r 0 -a DESFIRE_GETAPPIDS","NXP AN10927",
             {"DESFIRE_SELECT"}});

        add({"DESFIRE_SELECT",     "90 5A 00 00 03 {aid} 00", {"aid"},
             "Select DESFire application by AID (3 bytes = 6 hex chars)",
             "DESFire","aid must be exactly 3 bytes (6 hex chars).",
             "scardtool send-apdu -r 0 -a DESFIRE_SELECT:D27600","NXP AN10927",
             {"DESFIRE_GETAPPIDS","DESFIRE_GETFILEIDS"}});

        add({"DESFIRE_GETFILEIDS", "90 6F 00 00 00", {},
             "List all file IDs in selected application",
             "DESFire","Must select application first.",
             "scardtool send-apdu -r 0 -a DESFIRE_GETFILEIDS","NXP AN10927",
             {"DESFIRE_SELECT","DESFIRE_GETFILESETTINGS"}});

        add({"DESFIRE_GETFILESETTINGS", "90 F5 00 00 01 {fileNo} 00", {"fileNo"},
             "Get file type and access rights for a file",
             "DESFire","fileNo = 1 byte file number.",
             "scardtool send-apdu -r 0 -a DESFIRE_GETFILESETTINGS:01","NXP AN10927",
             {"DESFIRE_GETFILEIDS"}});

        add({"DESFIRE_READDATA",   "90 BD 00 00 07 {fileNo} {offset} {length} 00",
             {"fileNo","offset","length"},
             "Read data from standard/backup file",
             "DESFire","offset and length are 3-byte little-endian (6 hex chars each).",
             "scardtool send-apdu -r 0 -a DESFIRE_READDATA:01:000000:000010","NXP AN10927",
             {"DESFIRE_WRITEDATA"}});

        add({"DESFIRE_WRITEDATA",  "90 3D 00 00 {lc} {fileNo} {offset} {length} {data} 00",
             {"lc","fileNo","offset","length","data"},
             "Write data to standard/backup file",
             "DESFire","lc = 7 + data_len. offset/length = 3-byte LE.",
             "","NXP AN10927",{"DESFIRE_READDATA"}});

        add({"DESFIRE_GETVALUE",   "90 6C 00 00 01 {fileNo} 00", {"fileNo"},
             "Read current value from value file",
             "DESFire","Returns 4-byte signed value.",
             "scardtool send-apdu -r 0 -a DESFIRE_GETVALUE:01","NXP AN10927",
             {"DESFIRE_CREDIT","DESFIRE_DEBIT"}});

        add({"DESFIRE_CREDIT",     "90 05 00 00 05 {fileNo} {amount} 00", {"fileNo","amount"},
             "Increment value file by amount",
             "DESFire","amount = 4-byte LE signed integer (8 hex chars).",
             "scardtool send-apdu -r 0 -a DESFIRE_CREDIT:01:00000064","NXP AN10927",
             {"DESFIRE_DEBIT","DESFIRE_COMMIT"}});

        add({"DESFIRE_DEBIT",      "90 DC 00 00 05 {fileNo} {amount} 00", {"fileNo","amount"},
             "Decrement value file by amount",
             "DESFire","amount = 4-byte LE signed integer.",
             "scardtool send-apdu -r 0 -a DESFIRE_DEBIT:01:00000064","NXP AN10927",
             {"DESFIRE_CREDIT","DESFIRE_COMMIT"}});

        add({"DESFIRE_COMMIT",     "90 0F 00 00 00", {},
             "Commit pending transaction (write to NVM)",
             "DESFire","Must be called after write/credit/debit to persist changes.",
             "scardtool send-apdu -r 0 -a DESFIRE_COMMIT","NXP AN10927",
             {"DESFIRE_ABORT"}});

        add({"DESFIRE_ABORT",      "90 FA 00 00 00", {},
             "Abort pending transaction (discard changes)",
             "DESFire","Rolls back all uncommitted changes.",
             "scardtool send-apdu -r 0 -a DESFIRE_ABORT","NXP AN10927",
             {"DESFIRE_COMMIT"}});

        add({"DESFIRE_AUTH_AES",   "90 AA 00 00 01 {keyNo} 00", {"keyNo"},
             "Start AES-128 EV2 authentication",
             "DESFire","keyNo = key number (0x00 = master key).",
             "scardtool send-apdu -r 0 -a DESFIRE_AUTH_AES:00","NXP AN10927",
             {"DESFIRE_AUTH_DES","DESFIRE_MOREDATA"}});

        add({"DESFIRE_AUTH_DES",   "90 1A 00 00 01 {keyNo} 00", {"keyNo"},
             "Start DES/3DES authentication",
             "DESFire","Legacy DES/2K3DES auth.",
             "scardtool send-apdu -r 0 -a DESFIRE_AUTH_DES:00","NXP AN10927",
             {"DESFIRE_AUTH_AES"}});

        add({"DESFIRE_AUTH_ISO",   "90 61 00 00 01 {keyNo} 00", {"keyNo"},
             "Start 3K3DES ISO authentication",
             "DESFire","3-key 3DES authentication.",
             "scardtool send-apdu -r 0 -a DESFIRE_AUTH_ISO:00","NXP AN10927",
             {"DESFIRE_AUTH_AES"}});

        add({"DESFIRE_FORMAT",     "90 FC 00 00 00", {},
             "⚠ FORMAT CARD — wipe ALL applications and data",
             "DESFire",
             "WARNING: Permanently deletes ALL data on card. Requires master key auth.",
             "scardtool send-apdu -r 0 -a DESFIRE_FORMAT","NXP AN10927",{}});

        add({"DESFIRE_FORMAT_ALT", "90 4F 00 00 00", {},
             "⚠ FORMAT PICC — alternative format command",
             "DESFire","Same effect as DESFIRE_FORMAT on some firmware versions.",
             "","NXP AN10927",{"DESFIRE_FORMAT"}});

        add({"DESFIRE_CHANGEKEY",  "90 C4 00 {keyNo} {lc} {keyData} 00",
             {"keyNo","lc","keyData"},
             "Change a key in the selected application",
             "DESFire","Requires current key auth. keyData format depends on key type.",
             "","NXP AN10927",{}});

        add({"DESFIRE_GETKEYSETTINGS","90 45 00 00 00", {},
             "Get key settings and maximum number of keys",
             "DESFire","Returns key settings byte and max keys.",
             "scardtool send-apdu -r 0 -a DESFIRE_GETKEYSETTINGS","NXP AN10927",{}});

        add({"DESFIRE_GETKEYVER",  "90 64 00 00 01 {keyNo} 00", {"keyNo"},
             "Get version of a specific key",
             "DESFire","Returns 1-byte key version.",
             "scardtool send-apdu -r 0 -a DESFIRE_GETKEYVER:00","NXP AN10927",{}});

        add({"DESFIRE_READRECORDS","90 BB 00 00 07 {fileNo} {offset} {count} 00",
             {"fileNo","offset","count"},
             "Read records from linear/cyclic record file",
             "DESFire","offset and count are 3-byte LE.",
             "","NXP AN10927",{"DESFIRE_WRITERECORD"}});

        add({"DESFIRE_WRITERECORD","90 3B 00 00 {lc} {fileNo} {offset} {length} {data} 00",
             {"lc","fileNo","offset","length","data"},
             "Write record to linear/cyclic record file",
             "DESFire","","","NXP AN10927",{"DESFIRE_READRECORDS","DESFIRE_COMMIT"}});

        add({"DESFIRE_CLEARRECORDS","90 EB 00 00 01 {fileNo} 00", {"fileNo"},
             "Clear all records in a record file",
             "DESFire","Requires write permission. Changes committed on DESFIRE_COMMIT.",
             "scardtool send-apdu -r 0 -a DESFIRE_CLEARRECORDS:01","NXP AN10927",
             {"DESFIRE_COMMIT"}});

        add({"DESFIRE_LIMITEDCREDIT","90 1C 00 00 05 {fileNo} {amount} 00",{"fileNo","amount"},
             "Credit value file by limited amount",
             "DESFire","Only usable when limited credit right is granted.",
             "","NXP AN10927",{"DESFIRE_CREDIT"}});

        // ── Ultralight ────────────────────────────────────────────────────────

        add({"UL_GETVERSION",  "60 00 00 00", {},
             "Get Ultralight/NTAG version info (8 bytes)",
             "Ultralight",
             "Native NFC command. May require reader pass-through mode.\n"
             "Returns: header(1) vendor(1) type(1) subtype(1) major(1) minor(1) storage(1) protocol(1).",
             "scardtool send-apdu -r 0 -a UL_GETVERSION","NXP AN12196",{"GETUID","GETATS"}});

        add({"UL_READ",        "30 {page}", {"page"},
             "Read 4 pages (16 bytes) from Ultralight, starting at page",
             "Ultralight","Native NFC. page = 0..15 (UL) or 0..134 (NTAG216).",
             "scardtool send-apdu -r 0 -a UL_READ:04","NXP MF0ICU2",{"UL_WRITE"}});

        add({"UL_WRITE",       "A2 {page} {data}", {"page","data"},
             "Write 4 bytes to Ultralight page",
             "Ultralight","data = exactly 4 bytes (8 hex chars).",
             "scardtool send-apdu -r 0 -a UL_WRITE:04:01020304","NXP MF0ICU2",{"UL_READ"}});

        add({"UL_READ_CNT",    "39 {cntNo}", {"cntNo"},
             "Read Ultralight EV1 counter (0, 1 or 2)",
             "Ultralight","Returns 3-byte counter value.",
             "scardtool send-apdu -r 0 -a UL_READ_CNT:00","NXP MF0UL11",{}});

        add({"UL_INCR_CNT",    "A5 {cntNo} {amount}", {"cntNo","amount"},
             "Increment Ultralight EV1 counter",
             "Ultralight","amount = 3-byte LE value to add (6 hex chars).",
             "scardtool send-apdu -r 0 -a UL_INCR_CNT:00:000001","NXP MF0UL11",
             {"UL_READ_CNT"}});
    }
};
