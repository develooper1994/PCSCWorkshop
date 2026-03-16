/** @file Commands.h
 * @brief All scardtool command implementations. */
#pragma once
#include "ICommand.h"
#include "Mcp/HexUtils.h"
#include "Card/CardMath.h"
#include "Card/TrailerFormat.h"
#include "Card/AccessBitsHelper.h"
#include "Catalog/ApduCatalog.h"
#include "Crypto/PayloadCipher.h"
#include "PcscCommands.h"
#include <sstream>
#include <algorithm>

#ifdef _WIN32
#  include <winscard.h>
#else
#  include <PCSC/winscard.h>
#  include <PCSC/wintypes.h>
#endif

namespace scardtool::cmds {

// ── Shared SW helper ─────────────────────────────────────────────────────────
inline uint16_t checkSW(const BYTEV& resp) {
    if (resp.size() < 2) return 0;
    return (static_cast<uint16_t>(resp[resp.size()-2]) << 8) | resp[resp.size()-1];
}

// ── Shared crypto helper ──────────────────────────────────────────────────────
// Şifreleme algoritmasını çöz:
//   - CipherAlgo::Detect → UID+ATS ile kart tespiti, önerilen algo seçilir
//   - Explicit algo     → direkt kullan
//   - password boşsa   → hata
// Döner: {algo, uid} veya nullopt
inline std::optional<std::pair<CipherAlgo,std::string>>
resolvePayloadCipher(App& app, const std::string& /*readerVal*/) {
    if (app.flags.password.empty()) {
        std::cerr << "Error: Encryption requires --password/-P\n";
        return std::nullopt;
    }

    // UID al
    std::string uid;
    {
        BYTEV apdu = {0xFF,0xCA,0x00,0x00,0x00};
        auto r = app.pcsc.trySendCommand(apdu, false);
        if (r.is_ok() && checkSW(r.unwrap()) == 0x9000) {
            const BYTEV& resp = r.unwrap();
            uid = mcp::toHex(BYTEV(resp.begin(), resp.end()-2));
        }
    }

    CipherAlgo algo = app.flags.cipher;

    if (algo == CipherAlgo::Detect || algo == CipherAlgo::None) {
        // Kart tipini tespit et
        CardType cardType = CardType::Unknown;
        // DESFire check
        auto r = app.pcsc.trySendCommand({0x90,0x60,0x00,0x00,0x00}, false);
        if (r.is_ok()) {
            uint16_t sw = checkSW(r.unwrap());
            if (sw == 0x91AF || sw == 0x9100)
                cardType = CardType::MifareDesfire;
        }
        // Ultralight check
        if (cardType == CardType::Unknown) {
            auto rv = app.pcsc.trySendCommand({0x60,0x00,0x00,0x00}, false);
            if (rv.is_ok() && checkSW(rv.unwrap()) == 0x9000 && rv.unwrap().size() >= 9)
                cardType = CardType::MifareUltralight;
        }
        // Classic fallback (4-byte UID)
        if (cardType == CardType::Unknown && uid.size() == 8)
            cardType = CardType::MifareClassic1K;

        int blockSz = CardMath::blockSize(
            cardType == CardType::Unknown ? CardType::MifareClassic1K : cardType);
        auto detect = CardCrypto::analyze(cardType, blockSz, uid);
        algo = detect.recommended;
        app.fmt.verbose("detect-cipher → " + CardCrypto::toString(algo));
    } else {
        app.fmt.verbose("cipher = " + CardCrypto::toString(algo));
    }

    if (algo == CipherAlgo::None) {
        std::cerr << "Error: --encrypt set but --cipher none. "
                     "Remove --encrypt or choose a cipher.\n";
        return std::nullopt;
    }
    return std::make_pair(algo, uid);
}


// Mode: All
// Required: —
// Optional: --json/-j
// ════════════════════════════════════════════════════════════════════════════════

class ListReadersCmd : public ICommand {
public:
    std::string name()        const override { return "list-readers"; }
    std::string description() const override {
        return "List all available PC/SC smart card readers on this system.";
    }
    std::string usage() const override {
        return "scardtool list-readers [-j]";
    }
    CommandMode mode() const override { return CommandMode::All; }

    std::vector<ParamDef> params() const override { return {}; }
    std::vector<ArgDef>   argDefs() const override {
        return { {"json", 'j', false} };
    }

    ExitCode execute(CommandContext& ctx) override {
        auto& app = ctx.app;

        if (app.flags.dryRun) {
            app.fmt.dryRun("SCardListReaders()");
            return ExitCode::Success;
        }

        if (!app.pcsc.hasContext()) {
            if (!app.pcsc.establishContext())
                return fail(ExitCode::PcscError,
                            "Failed to establish PC/SC context. "
                            "Is the PC/SC service running?");
        }

        auto list = app.pcsc.listReaders();
        if (!list.ok)
            return fail(ExitCode::PcscError,
                        "SCardListReaders failed. "
                        "Is a reader plugged in?");

        if (list.names.empty())
            return fail(ExitCode::GeneralError,
                        "No PC/SC readers found. Is the reader plugged in?");

        // Cache güncelle
        app.readers = list.names;

        json arr = json::array();
        for (size_t i = 0; i < list.names.size(); ++i) {
            arr.push_back({
                {"index", i},
                {"name",  App::wideToUtf8(list.names[i])}
            });
        }

        app.fmt.result({ {"readers", arr} });
        return ExitCode::Success;
    }
};

// ════════════════════════════════════════════════════════════════════════════════
// ConnectCmd — connect
// Mode: All
// Required: --reader/-r
// Optional: --retry-ms, --max-retries
// ════════════════════════════════════════════════════════════════════════════════

class ConnectCmd : public ICommand {
public:
    std::string name()        const override { return "connect"; }
    std::string description() const override {
        return "Connect to a smart card on the specified reader.\n"
               "      Use list-readers first to see available reader indices.";
    }
    std::string usage() const override {
        return "scardtool connect -r <index|name> [--retry-ms N] [--max-retries N]";
    }
    CommandMode mode() const override { return CommandMode::All; }

    std::vector<ParamDef> params() const override {
        return {
            {"reader",      'r', "Reader index or name substring", true,  ""},
            {"retry-ms",    0,   "Retry interval ms (default 500)", false, "500"},
            {"max-retries", 0,   "Max attempts, 0=infinite (default 3)", false, "3"},
        };
    }
    std::vector<ArgDef> argDefs() const override {
        return {
            {"reader",      'r', true},
            {"retry-ms",    0,   true},
            {"max-retries", 0,   true},
            {"json",        'j', false},
        };
    }

    ExitCode execute(CommandContext& ctx) override {
        auto& app = ctx.app;
        ParamResolver pr(ctx);

        auto readerVal = pr.require("reader",
            "Reader index or name (run list-readers first): ", 'r');
        if (!readerVal) return ExitCode::InvalidArgs;

        int retryMs    = std::stoi(pr.resolve("retry-ms").value_or("500"));
        int maxRetries = std::stoi(pr.resolve("max-retries").value_or("3"));

        if (app.flags.dryRun) {
            app.fmt.dryRun("connect reader=" + *readerVal +
                           " retry-ms=" + std::to_string(retryMs) +
                           " max-retries=" + std::to_string(maxRetries));
            return ExitCode::Success;
        }

        if (!app.pcsc.hasContext()) {
            if (!app.pcsc.establishContext())
                return fail(ExitCode::PcscError,
                            "Failed to establish PC/SC context.");
        }

        auto readerName = app.resolveReader(*readerVal);
        if (!readerName)
            return fail(ExitCode::GeneralError,
                        "Reader not found: '" + *readerVal +
                        "'. Run list-readers to see available readers.");

        if (!app.pcsc.chooseReader(
                findReaderIndex(app, *readerName) + 1)) {
            return fail(ExitCode::PcscError, "Failed to select reader.");
        }

        app.fmt.verbose("Connecting to: " + App::wideToUtf8(*readerName));

        if (!app.pcsc.connectToCard(retryMs, maxRetries))
            return fail(ExitCode::GeneralError,
                        "No card detected on reader '" +
                        App::wideToUtf8(*readerName) +
                        "'. Place a card and retry.");

        json result = {
            {"connected", true},
            {"reader",    App::wideToUtf8(*readerName)},
            {"protocol",  app.pcsc.protocol() == SCARD_PROTOCOL_T0 ? "T=0" : "T=1"}
        };
        app.fmt.result(result);
        return ExitCode::Success;
    }

private:
    size_t findReaderIndex(const App& app, const std::wstring& name) {
        for (size_t i = 0; i < app.readers.size(); ++i)
            if (app.readers[i] == name) return i;
        return 0;
    }
};

// ════════════════════════════════════════════════════════════════════════════════
// DisconnectCmd — disconnect
// Mode: All
// Required: —
// ════════════════════════════════════════════════════════════════════════════════

class DisconnectCmd : public ICommand {
public:
    std::string name()        const override { return "disconnect"; }
    std::string description() const override {
        return "Disconnect from the current smart card and release PC/SC context.";
    }
    std::string usage() const override { return "scardtool disconnect"; }
    CommandMode mode() const override { return CommandMode::All; }

    std::vector<ParamDef> params() const override { return {}; }
    std::vector<ArgDef>   argDefs() const override { return {}; }

    ExitCode execute(CommandContext& ctx) override {
        auto& app = ctx.app;

        if (app.flags.dryRun) {
            app.fmt.dryRun("SCardDisconnect() + SCardReleaseContext()");
            return ExitCode::Success;
        }

        app.pcsc.disconnect();
        app.pcsc.releaseContext();
        app.readers.clear();

        app.fmt.result({ {"disconnected", true} });
        return ExitCode::Success;
    }
};

// ════════════════════════════════════════════════════════════════════════════════
// UidCmd — uid
// Mode: All
// Required: --reader/-r
// ════════════════════════════════════════════════════════════════════════════════

class UidCmd : public ICommand {
public:
    std::string name()        const override { return "uid"; }
    std::string description() const override {
        return "Read the UID of the card on the specified reader (FF CA 00 00 00).";
    }
    std::string usage() const override { return "scardtool uid -r <index|name> [-j]"; }
    CommandMode mode() const override { return CommandMode::All; }

    std::vector<ParamDef> params() const override {
        return { {"reader", 'r', "Reader index or name", true, ""} };
    }
    std::vector<ArgDef> argDefs() const override {
        return { {"reader", 'r', true}, {"json", 'j', false} };
    }

    ExitCode execute(CommandContext& ctx) override {
        return sendAndDisplay(ctx, "uid",
            {0xFF, 0xCA, 0x00, 0x00, 0x00},
            [](const BYTEV& data, Formatter& fmt) {
                fmt.result({
                    {"uid",    mcp::toHex(data)},
                    {"length", data.size()}
                });
            });
    }

public:
    // reader bağlantısını kurar, APDU gönderir, sonucu formatlar
    ExitCode sendAndDisplay(CommandContext& ctx,
                            const std::string& opName,
                            const BYTEV& apdu,
                            std::function<void(const BYTEV&, Formatter&)> onSuccess) {
        auto& app = ctx.app;
        ParamResolver pr(ctx);

        auto readerVal = pr.require("reader", "Reader index or name: ", 'r');
        if (!readerVal) return ExitCode::InvalidArgs;

        if (app.flags.dryRun) {
            app.fmt.dryRun(opName + " reader=" + *readerVal +
                           " apdu=" + mcp::toHex(apdu));
            return ExitCode::Success;
        }

        auto ec = ensureConnected(app, *readerVal);
        if (ec != ExitCode::Success) return ec;

        auto result = app.pcsc.trySendCommand(apdu, true);
        if (!result.is_ok())
            return fail(ExitCode::CardCommError, result.error().message());

        const BYTEV& resp = result.unwrap();
        if (resp.size() < 2)
            return fail(ExitCode::CardCommError, "Response too short");

        BYTE sw1 = resp[resp.size() - 2];
        BYTE sw2 = resp[resp.size() - 1];
        if (sw1 != 0x90 || sw2 != 0x00)
            return fail(ExitCode::CardCommError,
                        "Card returned SW=" + mcp::toHex({sw1, sw2}));

        BYTEV data(resp.begin(), resp.end() - 2);
        onSuccess(data, app.fmt);
        return ExitCode::Success;
    }

    static ExitCode ensureConnected(App& app, const std::string& readerVal) {
        if (!app.pcsc.hasContext()) {
            if (!app.pcsc.establishContext())
                return fail(ExitCode::PcscError, "Failed to establish PC/SC context.");
        }

        if (!app.pcsc.isConnected()) {
            auto readerName = app.resolveReader(readerVal);
            if (!readerName)
                return fail(ExitCode::GeneralError,
                            "Reader not found: '" + readerVal + "'");

            // Index bul (1-based için chooseReader)
            size_t idx = 0;
            for (size_t i = 0; i < app.readers.size(); ++i)
                if (app.readers[i] == *readerName) { idx = i; break; }

            if (!app.pcsc.chooseReader(idx + 1))
                return fail(ExitCode::PcscError, "Failed to select reader.");

            if (!app.pcsc.connectToCard(500, 3))
                return fail(ExitCode::GeneralError,
                            "No card detected on reader '" +
                            App::wideToUtf8(*readerName) +
                            "'. Place a card and retry.");
        }
        return ExitCode::Success;
    }
};

// ════════════════════════════════════════════════════════════════════════════════
// AtsCmd — ats
// Mode: All
// Required: --reader/-r
//
// ATS (Answer To Select): kart ISO 14443-4 oturumu sırasında gönderdiği
// tanıtım dizisi. Kart tipini, protokol versiyonunu ve desteklenen
// özellikleri içerir. Mifare Classic, DESFire ve diğer ISO 14443-4 kartları
// bu komutla tanımlanabilir. APDU: FF CA 01 00 00
// ════════════════════════════════════════════════════════════════════════════════

class AtsCmd : public UidCmd {
public:
    std::string name()        const override { return "ats"; }
    std::string description() const override {
        return "Read ATS (Answer To Select) / historical bytes from the card\n"
               "      (FF CA 01 00 00). Identifies card type and supported protocols.";
    }
    std::string usage() const override { return "scardtool ats -r <index|name> [-j]"; }

    ExitCode execute(CommandContext& ctx) override {
        return sendAndDisplay(ctx, "ats",
            {0xFF, 0xCA, 0x01, 0x00, 0x00},
            [](const BYTEV& data, Formatter& fmt) {
                fmt.result({
                    {"ats",    mcp::toHex(data)},
                    {"length", data.size()}
                });
            });
    }
};

// ════════════════════════════════════════════════════════════════════════════════
// SendApduCmd — send-apdu
// Mode: All
// Required: --reader/-r, --apdu/-a
// Optional: --no-chain, --json/-j
// ════════════════════════════════════════════════════════════════════════════════

class SendApduCmd : public ICommand {
public:
    std::string name()        const override { return "send-apdu"; }
    std::string description() const override {
        return "Send a raw APDU command to the card. Returns SW + response data.\n"
               "      APDU format: hex string, e.g. 00A4040007D2760000850100";
    }
    std::string usage() const override {
        return "scardtool send-apdu -r <index|name> -a <hex> [--no-chain] [-j]";
    }
    CommandMode mode() const override { return CommandMode::All; }

    std::vector<ParamDef> params() const override {
        return {
            {"reader",   'r', "Reader index or name",        true,  ""},
            {"apdu",     'a', "APDU bytes in hex",           true,  ""},
            {"no-chain", 0,   "Disable automatic GET RESPONSE for 61xx", false, ""},
        };
    }
    std::vector<ArgDef> argDefs() const override {
        return {
            {"reader",   'r', true},
            {"apdu",     'a', true},
            {"no-chain", 0,   false},
            {"json",     'j', false},
        };
    }

    ExitCode execute(CommandContext& ctx) override {
        auto& app = ctx.app;
        ParamResolver pr(ctx);

        auto readerVal = pr.require("reader", "Reader index or name: ", 'r');
        if (!readerVal) return ExitCode::InvalidArgs;

        auto apduInput = pr.require("apdu", "APDU hex or macro: ", 'a');
        if (!apduInput) return ExitCode::InvalidArgs;

        bool followChain = !pr.flag("no-chain");

        // ── Makro çözümleme ────────────────────────────────────────────────
        std::string resolvedHex;
        std::optional<ApduMacro> usedMacro;
        try {
            if (!MacroRegistry::instance().resolve(*apduInput, resolvedHex, &usedMacro)) {
                return fail(ExitCode::InvalidArgs,
                    "Unknown macro or invalid hex: '" + *apduInput + "'\n"
                    "       Run 'scardtool list-macros' to see available macros.");
            }
        } catch (const std::exception& e) {
            return fail(ExitCode::InvalidArgs, e.what());
        }

        // ── Verbose: makro ve APDU detayı ─────────────────────────────────
        if (usedMacro) {
            app.fmt.verbose("Macro resolved: " + *apduInput + " → " + resolvedHex);
        }
        app.fmt.verbose("→ " + resolvedHex);
        app.fmt.verbose("  " + describeApdu(resolvedHex));

        BYTEV apduBytes;
        try {
            apduBytes = mcp::fromHex(resolvedHex);
        } catch (const std::exception& e) {
            return fail(ExitCode::InvalidArgs,
                        std::string("Invalid APDU hex: ") + e.what());
        }

        if (app.flags.dryRun) {
            app.fmt.dryRun("send-apdu reader=" + *readerVal +
                           " apdu=" + resolvedHex +
                           (usedMacro ? " (macro: " + *apduInput + ")" : ""));
            return ExitCode::Success;
        }

        auto ec = UidCmd::ensureConnected(app, *readerVal);
        if (ec != ExitCode::Success) return ec;

        // ── APDU gönder + SW bazlı auto-chain ────────────────────────────────
        // PCSC::trySendCommand: 91AF → otomatik 90AF gönderir (DESFire)
        // 61xx → GET RESPONSE burada handle edilir (ISO 7816-4)
        BYTEV fullData;
        std::string finalSw = "????";

        auto result = app.pcsc.trySendCommand(apduBytes, followChain);

        // 61xx handler: trySendCommand 61xx'i handle etmez, biz yaparız
        if (!result.is_ok()) {
            return fail(ExitCode::CardCommError, result.error().message());
        }

        const BYTEV& rawResp = result.unwrap();
        BYTEV resp = rawResp;

        // 61xx: "xx bytes remaining" → GET RESPONSE zinciri
        if (followChain && resp.size() >= 2) {
            while (resp[resp.size()-2] == 0x61) {
                uint8_t le = resp[resp.size()-1];
                app.fmt.verbose("← 61" + mcp::toHex({le}) +
                    " — " + std::to_string(le) + " bytes remaining, sending GET RESPONSE");
                // Biriktir
                fullData.insert(fullData.end(), resp.begin(), resp.end()-2);
                // GET RESPONSE
                BYTEV getResp = {0x00, 0xC0, 0x00, 0x00, le};
                auto gr = app.pcsc.trySendCommand(getResp, false);
                if (!gr.is_ok()) break;
                resp = gr.unwrap();
            }
        }

        // Son data + SW
        if (resp.size() >= 2) {
            fullData.insert(fullData.end(), resp.begin(), resp.end()-2);
            uint8_t sw1 = resp[resp.size()-2], sw2 = resp[resp.size()-1];
            finalSw = mcp::toHex({sw1, sw2});
            app.fmt.verbose("← " + describeSw(sw1, sw2));
        } else {
            fullData.insert(fullData.end(), resp.begin(), resp.end());
        }

        std::string sw  = finalSw;
        std::string data = mcp::toHex(fullData);

        uint8_t sw1 = resp.size()>=2 ? resp[resp.size()-2] : 0;
        uint8_t sw2 = resp.size()>=2 ? resp[resp.size()-1] : 0;
        bool success = StatusWordCatalog::isSuccess(sw1, sw2);

        app.fmt.result({
            {"sw",      sw},
            {"sw_desc", StatusWordCatalog::shortDesc(sw1, sw2)},
            {"data",    data},
            {"raw",     mcp::toHex(resp)},
            {"success", success}
        });

        if (!success) {
            std::cerr << "Warning: " << describeSw(sw1, sw2) << "\n";
        }

        return ExitCode::Success;
    }
};

// ════════════════════════════════════════════════════════════════════════════════
// TransactionCmd — begin-transaction / end-transaction
// Mode: PersistentOnly (interactive + MCP)
//
// NOT: CLI terminal modunda begin/end farklı process'lerde çalışır.
// SCardBeginTransaction handle'a bağlıdır ve process kapanınca sona erer.
// Bu komutlar yalnızca 'interactive' veya '--mcp' modunda anlamlıdır.
// ════════════════════════════════════════════════════════════════════════════════

class BeginTransactionCmd : public ICommand {
public:
    std::string name()        const override { return "begin-transaction"; }
    std::string description() const override {
        return "Begin an exclusive PC/SC transaction. No other process can access\n"
               "      the card until end-transaction is called.\n"
               "      NOTE: Only meaningful in 'interactive' or '--mcp' mode\n"
               "      (requires persistent process — different CLI calls use different handles).";
    }
    std::string usage() const override { return "scardtool begin-transaction -r <index|name>"; }
    CommandMode mode() const override { return CommandMode::PersistentOnly; }

    std::vector<ParamDef> params() const override {
        return { {"reader", 'r', "Reader index or name", true, ""} };
    }
    std::vector<ArgDef> argDefs() const override {
        return { {"reader", 'r', true} };
    }

    ExitCode execute(CommandContext& ctx) override {
        auto& app = ctx.app;
        ParamResolver pr(ctx);

        auto readerVal = pr.require("reader", "Reader index or name: ", 'r');
        if (!readerVal) return ExitCode::InvalidArgs;

        if (!ctx.isPersistent()) {
            app.fmt.warn("begin-transaction has no effect in stateless terminal mode.\n"
                         "Use 'scardtool interactive' for persistent sessions.");
        }

        if (app.flags.dryRun) {
            app.fmt.dryRun("SCardBeginTransaction()");
            return ExitCode::Success;
        }

        auto ec = UidCmd::ensureConnected(app, *readerVal);
        if (ec != ExitCode::Success) return ec;

        LONG rv = SCardBeginTransaction(app.pcsc.handle());
        if (rv != SCARD_S_SUCCESS) {
            std::ostringstream oss;
            oss << "SCardBeginTransaction failed: 0x"
                << std::hex << std::uppercase << rv;
            return fail(ExitCode::PcscError, oss.str());
        }

        app.fmt.result({ {"transaction", "begun"} });
        return ExitCode::Success;
    }
};

class EndTransactionCmd : public ICommand {
public:
    std::string name()        const override { return "end-transaction"; }
    std::string description() const override {
        return "End the current PC/SC transaction.\n"
               "      Disposition: leave (default) | reset | unpower | eject";
    }
    std::string usage() const override {
        return "scardtool end-transaction [-d leave|reset|unpower|eject]";
    }
    CommandMode mode() const override { return CommandMode::PersistentOnly; }

    std::vector<ParamDef> params() const override {
        return { {"disposition", 'd', "Card disposition after transaction", false, "leave"} };
    }
    std::vector<ArgDef> argDefs() const override {
        return { {"disposition", 'd', true} };
    }

    ExitCode execute(CommandContext& ctx) override {
        auto& app = ctx.app;
        ParamResolver pr(ctx);

        std::string dispStr = pr.resolve("disposition").value_or("leave");

        static const std::map<std::string, DWORD> dispMap = {
            {"leave",   SCARD_LEAVE_CARD},
            {"reset",   SCARD_RESET_CARD},
            {"unpower", SCARD_UNPOWER_CARD},
            {"eject",   SCARD_EJECT_CARD}
        };

        auto it = dispMap.find(dispStr);
        if (it == dispMap.end())
            return fail(ExitCode::InvalidArgs,
                        "Invalid disposition: " + dispStr +
                        ". Use: leave, reset, unpower, eject");
        DWORD disp = it->second;

        if (app.flags.dryRun) {
            app.fmt.dryRun("SCardEndTransaction(disposition=" + dispStr + ")");
            return ExitCode::Success;
        }

        if (!app.pcsc.isConnected())
            return fail(ExitCode::PcscError, "Not connected.");

        LONG rv = SCardEndTransaction(app.pcsc.handle(), disp);
        if (rv != SCARD_S_SUCCESS) {
            std::ostringstream oss;
            oss << "SCardEndTransaction failed: 0x"
                << std::hex << std::uppercase << rv;
            return fail(ExitCode::PcscError, oss.str());
        }

        app.fmt.result({ {"transaction", "ended"}, {"disposition", dispStr} });
        return ExitCode::Success;
    }
};

// ════════════════════════════════════════════════════════════════════════════════
// LoadKeyCmd — load-key
// Reader'a Mifare key yükler (FF 82 structure slot keyLen [key])
//
// Reader-agnostic: KeyStructure byte mapping reader'a bırakılmıştır.
// ACR1281U: Volatile=0x00, NonVolatile=0x20  (PcscCommands::loadKey kullanır)
//
// Required: --reader/-r, --key/-k (12 hex char = 6 byte)
// Optional: --key-num/-n (slot, default 0x01), --volatile/-L (volatile slot)
// ════════════════════════════════════════════════════════════════════════════════

class LoadKeyCmd : public ICommand {
public:
    std::string name()        const override { return "load-key"; }
    std::string description() const override {
        return "Load a Mifare key into the reader's key slot (FF 82).\n"
               "       Key: 12 hex chars = 6 bytes, e.g. FFFFFFFFFFFF";
    }
    std::string usage() const override {
        return "scardtool load-key -r <reader> -k <hex6> [-n <slot>] [-L]";
    }
    CommandMode mode() const override { return CommandMode::All; }

    std::vector<ParamDef> params() const override {
        return {
            {"reader",   'r', "Reader index or name",              true,  ""},
            {"key",      'k', "Key bytes as 12-char hex (6 bytes)", true,  ""},
            {"key-num",  'n', "Key slot number (default 0x01)",    false, "01"},
            {"volatile", 'L', "Load into volatile slot (session)", false, ""},
        };
    }
    std::vector<ArgDef> argDefs() const override {
        return {
            {"reader",   'r', true},
            {"key",      'k', true},
            {"key-num",  'n', true},
            {"volatile", 'L', false},
            {"json",     'j', false},
        };
    }

    ExitCode execute(CommandContext& ctx) override {
        auto& app = ctx.app;
        ParamResolver pr(ctx);

        auto readerVal = pr.require("reader", "Reader index or name: ", 'r');
        if (!readerVal) return ExitCode::InvalidArgs;

        auto keyHex = pr.require("key", "Key hex (12 chars, e.g. FFFFFFFFFFFF): ", 'k');
        if (!keyHex) return ExitCode::InvalidArgs;

        auto keyOpt = TrailerFormat::parseKey(*keyHex);
        if (!keyOpt)
            return fail(ExitCode::InvalidArgs,
                        "Invalid key: must be exactly 12 hex chars (6 bytes). Got: " + *keyHex);

        bool isVolatile = pr.flag("volatile", 'L');
        BYTE keyNum = 0x01;
        if (auto n = pr.resolve("key-num", 'n')) {
            try { keyNum = static_cast<BYTE>(std::stoi(*n, nullptr, 16)); }
            catch (...) { keyNum = static_cast<BYTE>(std::stoi(*n)); }
        }

        // KeyStructure: Volatile=0x00, NonVolatile=0x20 (ACR1281U)
        // Reader-agnostic mapping PcscCommands::loadKey'e bırakılmıştır.
        BYTE structure = isVolatile ? 0x00 : 0x20;

        if (app.flags.dryRun) {
            app.fmt.dryRun("load-key reader=" + *readerVal +
                           " key=" + *keyHex +
                           " slot=" + std::to_string(keyNum) +
                           (isVolatile ? " volatile" : " non-volatile"));
            return ExitCode::Success;
        }

        auto ec = UidCmd::ensureConnected(app, *readerVal);
        if (ec != ExitCode::Success) return ec;

        BYTEV apdu = PcscCommands::loadKey(structure, keyNum,
                                           keyOpt->data(),
                                           static_cast<BYTE>(keyOpt->size()));
        auto result = app.pcsc.trySendCommand(apdu, false);
        if (!result.is_ok())
            return fail(ExitCode::CardCommError, result.error().message());

        const BYTEV& resp = result.unwrap();
        if (resp.size() < 2)
            return fail(ExitCode::CardCommError, "Response too short");

        BYTE sw1 = resp[resp.size()-2], sw2 = resp[resp.size()-1];
        if (sw1 != 0x90 || sw2 != 0x00)
            return fail(ExitCode::CardCommError,
                        "load-key failed SW=" + mcp::toHex({sw1, sw2}));

        app.fmt.result({
            {"loaded",    true},
            {"slot",      keyNum},
            {"structure", isVolatile ? "volatile" : "non-volatile"}
        });
        return ExitCode::Success;
    }
};

// ════════════════════════════════════════════════════════════════════════════════
// AuthCmd — auth
// Belirtilen block için Mifare auth yapar.
// Legacy (FF 88) veya General (FF 86) — ikisi de desteklenir.
//
// Required: --reader/-r, --block/-b
// Optional: --key-type/-t (A/B, default A), --key-num/-n (default 0x01),
//           --general/-G (FF 86 kullan, default FF 88)
// ════════════════════════════════════════════════════════════════════════════════

class AuthCmd : public ICommand {
public:
    std::string name()        const override { return "auth"; }
    std::string description() const override {
        return "Authenticate to a Mifare Classic block (FF 88 legacy or FF 86 general).\n"
               "       Run load-key first to load the key into the reader slot.";
    }
    std::string usage() const override {
        return "scardtool auth -r <reader> -b <block> [-t A|B] [-n <slot>] [-G]";
    }
    CommandMode mode() const override { return CommandMode::All; }

    std::vector<ParamDef> params() const override {
        return {
            {"reader",   'r', "Reader index or name",              true,  ""},
            {"block",    'b', "Block number to authenticate",      true,  ""},
            {"key-type", 't', "Key type: A or B (default A)",      false, "A"},
            {"key-num",  'n', "Key slot number (default 0x01)",    false, "01"},
            {"general",  'G', "Use General Authenticate (FF 86)", false, ""},
        };
    }
    std::vector<ArgDef> argDefs() const override {
        return {
            {"reader",   'r', true},
            {"block",    'b', true},
            {"key-type", 't', true},
            {"key-num",  'n', true},
            {"general",  'G', false},
            {"json",     'j', false},
        };
    }

    ExitCode execute(CommandContext& ctx) override {
        auto& app = ctx.app;
        ParamResolver pr(ctx);

        auto readerVal = pr.require("reader", "Reader index or name: ", 'r');
        if (!readerVal) return ExitCode::InvalidArgs;

        auto blockStr = pr.require("block", "Block number: ", 'b');
        if (!blockStr) return ExitCode::InvalidArgs;

        int blockNum = 0;
        try { blockNum = std::stoi(*blockStr); }
        catch (...) {
            return fail(ExitCode::InvalidArgs, "Invalid block number: " + *blockStr);
        }

        std::string keyTypeStr = pr.resolve("key-type", 't').value_or("A");
        std::transform(keyTypeStr.begin(), keyTypeStr.end(), keyTypeStr.begin(), ::toupper);
        if (keyTypeStr != "A" && keyTypeStr != "B")
            return fail(ExitCode::InvalidArgs, "Key type must be A or B");

        BYTE keyNum = 0x01;
        if (auto n = pr.resolve("key-num", 'n')) {
            try { keyNum = static_cast<BYTE>(std::stoi(*n, nullptr, 16)); }
            catch (...) { keyNum = static_cast<BYTE>(std::stoi(*n)); }
        }

        bool useGeneral = pr.flag("general", 'G');

        // ACR1281U: KeyA=0x60, KeyB=0x61  (reader-specific mapping)
        BYTE keyTypeByte = (keyTypeStr == "A") ? 0x60 : 0x61;

        if (app.flags.dryRun) {
            app.fmt.dryRun("auth reader=" + *readerVal +
                           " block=" + std::to_string(blockNum) +
                           " key=" + keyTypeStr +
                           " slot=" + std::to_string(keyNum) +
                           (useGeneral ? " general" : " legacy"));
            return ExitCode::Success;
        }

        auto ec = UidCmd::ensureConnected(app, *readerVal);
        if (ec != ExitCode::Success) return ec;

        BYTEV apdu;
        if (useGeneral) {
            BYTE data[5] = { 0x01, 0x00,
                             static_cast<BYTE>(blockNum),
                             keyTypeByte, keyNum };
            apdu = PcscCommands::authGeneral(data);
        } else {
            apdu = PcscCommands::authLegacy(
                static_cast<BYTE>(blockNum), keyTypeByte, keyNum);
        }

        auto result = app.pcsc.trySendCommand(apdu, false);
        if (!result.is_ok())
            return fail(ExitCode::CardCommError, result.error().message());

        const BYTEV& resp = result.unwrap();
        if (resp.size() < 2)
            return fail(ExitCode::CardCommError, "Response too short");

        BYTE sw1 = resp[resp.size()-2], sw2 = resp[resp.size()-1];
        if (sw1 != 0x90 || sw2 != 0x00)
            return fail(ExitCode::CardCommError,
                        "Auth failed SW=" + mcp::toHex({sw1, sw2}) +
                        " (wrong key or block number?)");

        app.fmt.result({
            {"authenticated", true},
            {"block",         blockNum},
            {"key_type",      keyTypeStr},
            {"method",        useGeneral ? "general" : "legacy"}
        });
        return ExitCode::Success;
    }
};

// ════════════════════════════════════════════════════════════════════════════════
// ReadCmd — read
// Ham block okuma: FF B0 00 <page> <length>
//
// Required: --reader/-r, --page/-p
// Optional: --length/-l (default 16)
// ════════════════════════════════════════════════════════════════════════════════

class ReadCmd : public ICommand {
public:
    std::string name()        const override { return "read"; }
    std::string description() const override {
        return "Read raw bytes from a card page/block (FF B0 00 <page> <length>).\n"
               "       For Mifare Classic: run auth first.\n"
               "       For Ultralight: no auth needed, length typically 4.";
    }
    std::string usage() const override {
        return "scardtool read -r <reader> -p <page> [-l <length>] [-j]";
    }
    CommandMode mode() const override { return CommandMode::All; }

    std::vector<ParamDef> params() const override {
        return {
            {"reader",  'r', "Reader index or name",     true,  ""},
            {"page",    'p', "Page/block number",        true,  ""},
            {"length",  'l', "Bytes to read (default 16)",false, "16"},
        };
    }
    std::vector<ArgDef> argDefs() const override {
        return {
            {"reader",  'r', true},
            {"page",    'p', true},
            {"length",  'l', true},
            {"json",    'j', false},
        };
    }

    ExitCode execute(CommandContext& ctx) override {
        auto& app = ctx.app;
        ParamResolver pr(ctx);

        auto readerVal = pr.require("reader", "Reader: ", 'r');
        if (!readerVal) return ExitCode::InvalidArgs;

        auto pageStr = pr.require("page", "Page/block number: ", 'p');
        if (!pageStr) return ExitCode::InvalidArgs;

        int page = 0, length = 16;
        try { page = std::stoi(*pageStr); }
        catch (...) { return fail(ExitCode::InvalidArgs, "Invalid page: " + *pageStr); }

        if (auto l = pr.resolve("length", 'l')) {
            try { length = std::stoi(*l); }
            catch (...) { return fail(ExitCode::InvalidArgs, "Invalid length: " + *l); }
        }
        if (length < 1 || length > 255)
            return fail(ExitCode::InvalidArgs, "Length must be 1-255");

        if (app.flags.dryRun) {
            app.fmt.dryRun("read reader=" + *readerVal +
                           " page=" + std::to_string(page) +
                           " length=" + std::to_string(length));
            return ExitCode::Success;
        }

        auto ec = UidCmd::ensureConnected(app, *readerVal);
        if (ec != ExitCode::Success) return ec;

        BYTEV apdu = PcscCommands::readBinary(
            static_cast<BYTE>(page), static_cast<BYTE>(length));
        auto result = app.pcsc.trySendCommand(apdu, false);
        if (!result.is_ok())
            return fail(ExitCode::CardCommError, result.error().message());

        const BYTEV& resp = result.unwrap();
        if (resp.size() < 2)
            return fail(ExitCode::CardCommError, "Response too short");

        BYTE sw1 = resp[resp.size()-2], sw2 = resp[resp.size()-1];
        if (sw1 != 0x90 || sw2 != 0x00)
            return fail(ExitCode::CardCommError,
                        "Read failed SW=" + mcp::toHex({sw1, sw2}) +
                        " (auth required? wrong page?)");

        BYTEV data(resp.begin(), resp.end() - 2);

        // ── Şifre çözme ──────────────────────────────────────────────────────
        std::string cipherInfo;
        if (app.flags.encrypt) {
            auto res = decryptPayload(app, data, page, *readerVal, cipherInfo);
            if (!res) return ExitCode::CardCommError;
            data = *res;
        }

        app.fmt.result({
            {"page",    page},
            {"length",  static_cast<int>(data.size())},
            {"data",    mcp::toHex(data)},
            {"cipher",  app.flags.encrypt ? cipherInfo : "none"}
        });
        return ExitCode::Success;
    }

private:
    static std::optional<BYTEV> decryptPayload(App& app, const BYTEV& raw,
                                                int page, const std::string& readerVal,
                                                std::string& cipherInfo) {
        auto res = resolvePayloadCipher(app, readerVal);
        if (!res) return std::nullopt;
        auto [algo, uid] = *res;
        cipherInfo = CardCrypto::toString(algo);
        try {
            PayloadCipher pc(app.flags.password, uid, algo);
            return pc.decrypt(raw, page);
        } catch (const std::exception& e) {
            std::cerr << "Error: Decrypt failed — wrong password or corrupted data\n"
                      << "       (" << e.what() << ")\n";
            return std::nullopt;
        }
    }
};

// ════════════════════════════════════════════════════════════════════════════════
// WriteCmd — write
// Ham block yazma: FF D6 00 <page> <lc> [data]
//
// Required: --reader/-r, --page/-p, --data/-d
// ════════════════════════════════════════════════════════════════════════════════

class WriteCmd : public ICommand {
public:
    std::string name()        const override { return "write"; }
    std::string description() const override {
        return "Write raw bytes to a card page/block (FF D6 00 <page> <lc> [data]).\n"
               "       For Mifare Classic: run auth first.\n"
               "       Data must match block size (Classic=16, Ultralight=4 bytes).";
    }
    std::string usage() const override {
        return "scardtool write -r <reader> -p <page> -d <hexdata>";
    }
    CommandMode mode() const override { return CommandMode::All; }

    std::vector<ParamDef> params() const override {
        return {
            {"reader", 'r', "Reader index or name",    true, ""},
            {"page",   'p', "Page/block number",       true, ""},
            {"data",   'd', "Data bytes as hex string",true, ""},
        };
    }
    std::vector<ArgDef> argDefs() const override {
        return {
            {"reader", 'r', true},
            {"page",   'p', true},
            {"data",   'd', true},
            {"json",   'j', false},
        };
    }

    ExitCode execute(CommandContext& ctx) override {
        auto& app = ctx.app;
        ParamResolver pr(ctx);

        auto readerVal = pr.require("reader", "Reader: ", 'r');
        if (!readerVal) return ExitCode::InvalidArgs;

        auto pageStr = pr.require("page", "Page/block: ", 'p');
        if (!pageStr) return ExitCode::InvalidArgs;

        auto dataHex = pr.require("data", "Data (hex): ", 'd');
        if (!dataHex) return ExitCode::InvalidArgs;

        int page = 0;
        try { page = std::stoi(*pageStr); }
        catch (...) { return fail(ExitCode::InvalidArgs, "Invalid page: " + *pageStr); }

        BYTEV data;
        try { data = mcp::fromHex(*dataHex); }
        catch (const std::exception& e) {
            return fail(ExitCode::InvalidArgs, std::string("Invalid hex data: ") + e.what());
        }
        if (data.empty())
            return fail(ExitCode::InvalidArgs, "Data cannot be empty");
        if (data.size() > 255)
            return fail(ExitCode::InvalidArgs, "Data too long (max 255 bytes)");

        // ── Trailer blok kontrolü ─────────────────────────────────────────────
        if (app.flags.encrypt) {
            // card-type bilinmiyorsa ClassicUnknown varsay
            if (CardCrypto::isTrailerBlock(page, CardType::MifareClassic1K)) {
                return fail(ExitCode::InvalidArgs,
                    "Refusing to encrypt/write trailer block " +
                    std::to_string(page) + ". Trailer blocks must never be encrypted.");
            }
        }

        if (app.flags.dryRun) {
            std::string cipherNote = app.flags.encrypt
                ? " [encrypted:" + CardCrypto::toString(app.flags.cipher) + "]" : "";
            app.fmt.dryRun("write reader=" + *readerVal +
                           " page=" + std::to_string(page) +
                           " data=" + *dataHex + cipherNote);
            return ExitCode::Success;
        }

        auto ec = UidCmd::ensureConnected(app, *readerVal);
        if (ec != ExitCode::Success) return ec;

        // ── Şifreleme ─────────────────────────────────────────────────────────
        std::string cipherInfo = "none";
        if (app.flags.encrypt) {
            auto res = resolvePayloadCipher(app, *readerVal);
            if (!res) return ExitCode::InvalidArgs;
            auto [algo, uid] = *res;
            cipherInfo = CardCrypto::toString(algo);

            // Kart blok boyutu ile uyumlu mu?
            int blockSize = static_cast<int>(data.size());
            if (!CardCrypto::isSuitable(algo, CardType::Unknown, blockSize)) {
                if (!app.flags.noWarn)
                    std::cerr << "Warning: " << CardCrypto::displayName(algo)
                              << " may not be suitable for " << blockSize
                              << "-byte blocks. Consider aes-ctr.\n";
            }

            try {
                PayloadCipher pc(app.flags.password, uid, algo);
                data = pc.encrypt(data, page);
            } catch (const std::exception& e) {
                return fail(ExitCode::CardCommError,
                            std::string("Encrypt failed: ") + e.what());
            }

            if (data.size() > 255)
                return fail(ExitCode::CardCommError,
                    "Encrypted data too large for card block (" +
                    std::to_string(data.size()) + " bytes). "
                    "Use aes-ctr for fixed-size output.");
        }

        BYTEV apdu = PcscCommands::updateBinary(
            static_cast<BYTE>(page), data.data(), static_cast<BYTE>(data.size()));
        auto result = app.pcsc.trySendCommand(apdu, false);
        if (!result.is_ok())
            return fail(ExitCode::CardCommError, result.error().message());

        const BYTEV& resp = result.unwrap();
        if (resp.size() < 2)
            return fail(ExitCode::CardCommError, "Response too short");

        BYTE sw1 = resp[resp.size()-2], sw2 = resp[resp.size()-1];
        if (sw1 != 0x90 || sw2 != 0x00)
            return fail(ExitCode::CardCommError,
                        "Write failed SW=" + mcp::toHex({sw1, sw2}) +
                        " (auth required? read-only block?)");

        app.fmt.result({
            {"written",  true},
            {"page",     page},
            {"length",   static_cast<int>(data.size())},
            {"cipher",   cipherInfo}
        });
        return ExitCode::Success;
    }
};

// ════════════════════════════════════════════════════════════════════════════════
// ReadSectorCmd — read-sector
// Bir sektörün tüm bloklarını auth + read ile okur.
// CardMath sektör→blok hesabını yapar (kart-agnostic).
//
// Required: --reader/-r, --sector/-s, --key/-k
// Optional: --key-type/-t (A/B), --key-num/-n, --card-type/-c
// ════════════════════════════════════════════════════════════════════════════════

class ReadSectorCmd : public ICommand {
public:
    std::string name()        const override { return "read-sector"; }
    std::string description() const override {
        return "Auth and read all blocks of a Mifare sector.\n"
               "       Automatically calculates block addresses from sector number.";
    }
    std::string usage() const override {
        return "scardtool read-sector -r <reader> -s <sector> -k <hex6> [-t A|B] [-c classic1k]";
    }
    CommandMode mode() const override { return CommandMode::All; }

    std::vector<ParamDef> params() const override {
        return {
            {"reader",    'r', "Reader index or name",            true,  ""},
            {"sector",    's', "Sector number",                   true,  ""},
            {"key",       'k', "Auth key (12 hex chars)",         true,  ""},
            {"key-type",  't', "Key type: A or B (default A)",   false, "A"},
            {"key-num",   'n', "Key slot (default 0x01)",        false, "01"},
            {"card-type", 'c', "classic1k|classic4k|ultralight", false, "classic1k"},
        };
    }
    std::vector<ArgDef> argDefs() const override {
        return {
            {"reader",    'r', true}, {"sector",    's', true},
            {"key",       'k', true}, {"key-type",  't', true},
            {"key-num",   'n', true}, {"card-type", 'c', true},
            {"json",      'j', false},
        };
    }

    ExitCode execute(CommandContext& ctx) override {
        auto& app = ctx.app;
        ParamResolver pr(ctx);

        auto readerVal  = pr.require("reader",  "Reader: ", 'r');
        auto sectorStr  = pr.require("sector",  "Sector: ", 's');
        auto keyHex     = pr.require("key",     "Key hex: ",'k');
        if (!readerVal || !sectorStr || !keyHex) return ExitCode::InvalidArgs;

        CardType cardType = CardMath::fromString(
            pr.resolve("card-type", 'c').value_or("classic1k"));
        if (cardType == CardType::Unknown)
            return fail(ExitCode::InvalidArgs, "Unknown card type. Use: classic1k, classic4k, ultralight");

        int sector = 0;
        try { sector = std::stoi(*sectorStr); }
        catch (...) { return fail(ExitCode::InvalidArgs, "Invalid sector: " + *sectorStr); }

        if (!CardMath::validSector(sector, cardType))
            return fail(ExitCode::InvalidArgs,
                        "Sector " + std::to_string(sector) + " out of range for " +
                        CardMath::toString(cardType));

        auto keyOpt = TrailerFormat::parseKey(*keyHex);
        if (!keyOpt)
            return fail(ExitCode::InvalidArgs, "Invalid key: " + *keyHex);

        std::string keyTypeStr = pr.resolve("key-type", 't').value_or("A");
        std::transform(keyTypeStr.begin(), keyTypeStr.end(), keyTypeStr.begin(), ::toupper);
        BYTE keyNum = 0x01;
        if (auto n = pr.resolve("key-num", 'n')) {
            try { keyNum = static_cast<BYTE>(std::stoi(*n, nullptr, 16)); }
            catch (...) { keyNum = static_cast<BYTE>(std::stoi(*n)); }
        }

        auto blocks = CardMath::sectorBlocks(sector, cardType);
        int trailerBlk = CardMath::trailerBlock(sector, cardType);

        if (app.flags.dryRun) {
            app.fmt.dryRun("read-sector sector=" + std::to_string(sector) +
                           " blocks=[" + std::to_string(blocks.front()) + ".." +
                           std::to_string(blocks.back()) + "]");
            return ExitCode::Success;
        }

        auto ec = UidCmd::ensureConnected(app, *readerVal);
        if (ec != ExitCode::Success) return ec;

        // load-key + auth (trailer bloğu üzerinden)
        BYTE structure = 0x20; // NonVolatile
        BYTE keyTypeByte = (keyTypeStr == "A") ? 0x60 : 0x61;
        BYTEV loadApdu = PcscCommands::loadKey(structure, keyNum,
                                               keyOpt->data(),
                                               static_cast<BYTE>(keyOpt->size()));
        auto lr = app.pcsc.trySendCommand(loadApdu, false);
        if (!lr.is_ok() || checkSW(lr.unwrap()) != 0x9000)
            return fail(ExitCode::CardCommError, "load-key failed");

        BYTEV authApdu = PcscCommands::authLegacy(
            static_cast<BYTE>(trailerBlk), keyTypeByte, keyNum);
        auto ar = app.pcsc.trySendCommand(authApdu, false);
        if (!ar.is_ok() || checkSW(ar.unwrap()) != 0x9000)
            return fail(ExitCode::CardCommError,
                        "Auth failed on block " + std::to_string(trailerBlk) +
                        " (wrong key?)");

        // Tüm blokları oku
        json sectorJson = json::array();
        // Şifreleme varsa PayloadCipher hazırla
        std::unique_ptr<PayloadCipher> payloadCipher;
        std::string cipherInfo = "none";
        if (app.flags.encrypt) {
            auto res = resolvePayloadCipher(app, *readerVal);
            if (!res) return ExitCode::InvalidArgs;
            cipherInfo = CardCrypto::toString(res->first);
            payloadCipher = std::make_unique<PayloadCipher>(
                app.flags.password, res->second, res->first);
        }

        for (int blk : blocks) {
            BYTEV apdu = PcscCommands::readBinary(
                static_cast<BYTE>(blk), static_cast<BYTE>(CardMath::blockSize(cardType)));
            auto rr = app.pcsc.trySendCommand(apdu, false);
            if (!rr.is_ok() || checkSW(rr.unwrap()) != 0x9000) {
                std::cerr << "Warning: block " << blk << " read failed\n";
                sectorJson.push_back({ {"block", blk}, {"data", nullptr}, {"error", true} });
                continue;
            }
            const BYTEV& resp = rr.unwrap();
            BYTEV data(resp.begin(), resp.end() - 2);

            // Trailer bloğunu asla çözme
            bool isTrailer = (blk == trailerBlk);
            std::string rawHex = mcp::toHex(data);
            std::string plainHex = rawHex;

            if (payloadCipher && !isTrailer) {
                try {
                    auto plain = payloadCipher->decrypt(data, blk);
                    plainHex = mcp::toHex(plain);
                } catch (const std::exception& e) {
                    if (!app.flags.noWarn)
                        std::cerr << "Warning: block " << blk
                                  << " decrypt failed: " << e.what() << "\n";
                }
            }

            json blkJson = {
                {"block",   blk},
                {"data",    plainHex},
                {"raw",     isTrailer || !payloadCipher ? plainHex : rawHex},
                {"kind",    isTrailer ? "trailer" : "data"},
                {"cipher",  isTrailer ? "none" : cipherInfo}
            };
            sectorJson.push_back(blkJson);
        }

        app.fmt.result({
            {"sector",     sector},
            {"card_type",  CardMath::toString(cardType)},
            {"blocks",     sectorJson}
        });
        return ExitCode::Success;
    }
};

// ════════════════════════════════════════════════════════════════════════════════
// WriteSectorCmd — write-sector
// Bir sektörün data bloklarını auth + write ile yazar.
// Trailer bloğunu YaZMAZ (güvenlik).
//
// Required: --reader/-r, --sector/-s, --key/-k, --data/-d
// Optional: --key-type/-t, --key-num/-n, --card-type/-c
// ════════════════════════════════════════════════════════════════════════════════

class WriteSectorCmd : public ICommand {
public:
    std::string name()        const override { return "write-sector"; }
    std::string description() const override {
        return "Auth and write data blocks of a Mifare sector.\n"
               "       Trailer block is NOT written (use write-trailer for that).\n"
               "       Data length must be: (blocks-1) * blockSize bytes.";
    }
    std::string usage() const override {
        return "scardtool write-sector -r <reader> -s <sector> -k <hex6> -d <hexdata> [-t A|B]";
    }
    CommandMode mode() const override { return CommandMode::All; }

    std::vector<ParamDef> params() const override {
        return {
            {"reader",    'r', "Reader",                          true,  ""},
            {"sector",    's', "Sector number",                   true,  ""},
            {"key",       'k', "Auth key (12 hex chars)",         true,  ""},
            {"data",      'd', "Data hex (without trailer block)",true,  ""},
            {"key-type",  't', "A or B (default A)",              false, "A"},
            {"key-num",   'n', "Key slot (default 0x01)",         false, "01"},
            {"card-type", 'c', "classic1k|classic4k (default 1k)",false,"classic1k"},
        };
    }
    std::vector<ArgDef> argDefs() const override {
        return {
            {"reader",'r',true},{"sector",'s',true},{"key",'k',true},
            {"data",'d',true},{"key-type",'t',true},{"key-num",'n',true},
            {"card-type",'c',true},{"json",'j',false},
        };
    }

    ExitCode execute(CommandContext& ctx) override {
        auto& app = ctx.app;
        ParamResolver pr(ctx);

        auto readerVal = pr.require("reader", "Reader: ", 'r');
        auto sectorStr = pr.require("sector", "Sector: ", 's');
        auto keyHex    = pr.require("key",    "Key: ",    'k');
        auto dataHex   = pr.require("data",   "Data hex: ",'d');
        if (!readerVal || !sectorStr || !keyHex || !dataHex) return ExitCode::InvalidArgs;

        CardType cardType = CardMath::fromString(
            pr.resolve("card-type",'c').value_or("classic1k"));
        int sector = std::stoi(*sectorStr);
        if (!CardMath::validSector(sector, cardType))
            return fail(ExitCode::InvalidArgs, "Sector out of range");

        auto keyOpt = TrailerFormat::parseKey(*keyHex);
        if (!keyOpt) return fail(ExitCode::InvalidArgs, "Invalid key");

        BYTEV allData;
        try { allData = mcp::fromHex(*dataHex); }
        catch (const std::exception& e) {
            return fail(ExitCode::InvalidArgs, std::string("Invalid hex: ") + e.what());
        }

        auto blocks = CardMath::sectorBlocks(sector, cardType);
        int  dataBlockCount = static_cast<int>(blocks.size()) - 1; // trailer hariç
        int  blockSz = CardMath::blockSize(cardType);
        int  expectedLen = dataBlockCount * blockSz;

        if (static_cast<int>(allData.size()) != expectedLen)
            return fail(ExitCode::InvalidArgs,
                        "Data must be " + std::to_string(expectedLen) +
                        " bytes (" + std::to_string(dataBlockCount) +
                        " blocks × " + std::to_string(blockSz) + " bytes). Got " +
                        std::to_string(allData.size()));

        std::string keyTypeStr = pr.resolve("key-type",'t').value_or("A");
        std::transform(keyTypeStr.begin(), keyTypeStr.end(), keyTypeStr.begin(), ::toupper);
        BYTE keyNum = 0x01;
        if (auto n = pr.resolve("key-num",'n')) {
            try { keyNum = static_cast<BYTE>(std::stoi(*n, nullptr, 16)); }
            catch (...) { keyNum = static_cast<BYTE>(std::stoi(*n)); }
        }
        BYTE keyTypeByte = (keyTypeStr == "A") ? 0x60 : 0x61;
        int trailerBlk = CardMath::trailerBlock(sector, cardType);

        if (app.flags.dryRun) {
            std::string cipherNote = app.flags.encrypt
                ? " [encrypted:" + CardCrypto::toString(app.flags.cipher) + "]" : "";
            app.fmt.dryRun("write-sector sector=" + std::to_string(sector) +
                           " data_blocks=" + std::to_string(dataBlockCount) + cipherNote);
            return ExitCode::Success;
        }

        auto ec = UidCmd::ensureConnected(app, *readerVal);
        if (ec != ExitCode::Success) return ec;

        // Şifreleme: PayloadCipher hazırla
        std::unique_ptr<PayloadCipher> payloadCipher;
        std::string cipherInfo = "none";
        if (app.flags.encrypt) {
            auto res = resolvePayloadCipher(app, *readerVal);
            if (!res) return ExitCode::InvalidArgs;
            cipherInfo = CardCrypto::toString(res->first);
            payloadCipher = std::make_unique<PayloadCipher>(
                app.flags.password, res->second, res->first);
        }

        // load-key + auth
        BYTEV ldApdu = PcscCommands::loadKey(0x20, keyNum, keyOpt->data(),
                                             static_cast<BYTE>(keyOpt->size()));
        auto lr = app.pcsc.trySendCommand(ldApdu, false);
        if (!lr.is_ok() || checkSW(lr.unwrap()) != 0x9000)
            return fail(ExitCode::CardCommError, "load-key failed");

        BYTEV authApdu = PcscCommands::authLegacy(
            static_cast<BYTE>(trailerBlk), keyTypeByte, keyNum);
        auto ar = app.pcsc.trySendCommand(authApdu, false);
        if (!ar.is_ok() || checkSW(ar.unwrap()) != 0x9000)
            return fail(ExitCode::CardCommError, "Auth failed");

        // Data bloklarını yaz (trailer hariç)
        int written = 0;
        for (int i = 0; i < dataBlockCount; ++i) {
            int blk = blocks[i];
            BYTEV chunk(allData.begin() + i * blockSz,
                        allData.begin() + (i + 1) * blockSz);

            // Şifrele (trailer bloğunu asla şifreleme — ama burası zaten data bloku)
            if (payloadCipher) {
                try {
                    BYTEV encrypted = payloadCipher->encrypt(chunk, blk);
                    if (encrypted.size() != static_cast<size_t>(blockSz)) {
                        std::cerr << "Warning: block " << blk
                                  << " encrypted size mismatch ("
                                  << encrypted.size() << " != " << blockSz
                                  << "). Use aes-ctr for fixed-size output.\n";
                    } else {
                        chunk = encrypted;
                    }
                } catch (const std::exception& e) {
                    return fail(ExitCode::CardCommError,
                        "Encrypt block " + std::to_string(blk) + ": " + e.what());
                }
            }

            BYTEV wApdu = PcscCommands::updateBinary(
                static_cast<BYTE>(blk), chunk.data(), static_cast<BYTE>(chunk.size()));
            auto wr = app.pcsc.trySendCommand(wApdu, false);
            if (!wr.is_ok() || checkSW(wr.unwrap()) != 0x9000) {
                std::cerr << "Warning: block " << blk << " write failed\n";
            } else {
                ++written;
            }
        }

        app.fmt.result({
            {"sector",          sector},
            {"blocks_written",  written},
            {"blocks_total",    dataBlockCount},
            {"cipher",          cipherInfo}
        });
        return ExitCode::Success;
    }
};

// ════════════════════════════════════════════════════════════════════════════════
// ReadTrailerCmd — read-trailer
// Trailer bloğunu okur ve parse eder: KeyA/B + access bits
//
// Required: --reader/-r, --sector/-s, --key/-k
// Optional: --key-type/-t, --card-type/-c
// ════════════════════════════════════════════════════════════════════════════════

class ReadTrailerCmd : public ICommand {
public:
    std::string name()        const override { return "read-trailer"; }
    std::string description() const override {
        return "Read and parse the trailer block of a Mifare sector.\n"
               "       Shows KeyA (masked), KeyB, access bytes and their meaning.";
    }
    std::string usage() const override {
        return "scardtool read-trailer -r <reader> -s <sector> -k <hex6> [-t A|B] [-j]";
    }
    CommandMode mode() const override { return CommandMode::All; }

    std::vector<ParamDef> params() const override {
        return {
            {"reader",    'r', "Reader",              true,  ""},
            {"sector",    's', "Sector number",       true,  ""},
            {"key",       'k', "Auth key (12 hex)",   true,  ""},
            {"key-type",  't', "A or B (default A)",  false, "A"},
            {"key-num",   'n', "Key slot",            false, "01"},
            {"card-type", 'c', "classic1k|classic4k", false, "classic1k"},
        };
    }
    std::vector<ArgDef> argDefs() const override {
        return {
            {"reader",'r',true},{"sector",'s',true},{"key",'k',true},
            {"key-type",'t',true},{"key-num",'n',true},
            {"card-type",'c',true},{"json",'j',false},
        };
    }

    ExitCode execute(CommandContext& ctx) override {
        auto& app = ctx.app;
        ParamResolver pr(ctx);

        auto readerVal = pr.require("reader", "Reader: ", 'r');
        auto sectorStr = pr.require("sector", "Sector: ", 's');
        auto keyHex    = pr.require("key",    "Key: ",    'k');
        if (!readerVal || !sectorStr || !keyHex) return ExitCode::InvalidArgs;

        CardType cardType = CardMath::fromString(
            pr.resolve("card-type",'c').value_or("classic1k"));
        int sector = std::stoi(*sectorStr);
        if (!CardMath::validSector(sector, cardType))
            return fail(ExitCode::InvalidArgs, "Sector out of range");

        auto keyOpt = TrailerFormat::parseKey(*keyHex);
        if (!keyOpt) return fail(ExitCode::InvalidArgs, "Invalid key");

        std::string keyTypeStr = pr.resolve("key-type",'t').value_or("A");
        std::transform(keyTypeStr.begin(), keyTypeStr.end(), keyTypeStr.begin(), ::toupper);
        BYTE keyNum = 0x01;
        if (auto n = pr.resolve("key-num",'n')) {
            try { keyNum = static_cast<BYTE>(std::stoi(*n, nullptr, 16)); }
            catch (...) { keyNum = static_cast<BYTE>(std::stoi(*n)); }
        }
        BYTE keyTypeByte = (keyTypeStr == "A") ? 0x60 : 0x61;
        int trailerBlk = CardMath::trailerBlock(sector, cardType);

        if (app.flags.dryRun) {
            app.fmt.dryRun("read-trailer sector=" + std::to_string(sector) +
                           " block=" + std::to_string(trailerBlk));
            return ExitCode::Success;
        }

        auto ec = UidCmd::ensureConnected(app, *readerVal);
        if (ec != ExitCode::Success) return ec;

        BYTEV ldApdu = PcscCommands::loadKey(0x20, keyNum, keyOpt->data(),
                                             static_cast<BYTE>(keyOpt->size()));
        auto lr = app.pcsc.trySendCommand(ldApdu, false);
        if (!lr.is_ok() || checkSW(lr.unwrap()) != 0x9000)
            return fail(ExitCode::CardCommError, "load-key failed");

        BYTEV authApdu = PcscCommands::authLegacy(
            static_cast<BYTE>(trailerBlk), keyTypeByte, keyNum);
        auto ar = app.pcsc.trySendCommand(authApdu, false);
        if (!ar.is_ok() || checkSW(ar.unwrap()) != 0x9000)
            return fail(ExitCode::CardCommError, "Auth failed");

        BYTEV rdApdu = PcscCommands::readBinary(
            static_cast<BYTE>(trailerBlk), 16);
        auto rr = app.pcsc.trySendCommand(rdApdu, false);
        if (!rr.is_ok() || checkSW(rr.unwrap()) != 0x9000)
            return fail(ExitCode::CardCommError, "Read trailer failed");

        const BYTEV& resp = rr.unwrap();
        BYTEV raw(resp.begin(), resp.end() - 2);
        auto td = TrailerFormat::parse(raw);

        json result = {
            {"sector",        sector},
            {"trailer_block", trailerBlk},
            {"raw",           mcp::toHex(raw)},
            {"key_a",         "000000000000"},  // okunduğunda masked
            {"key_b",         TrailerFormat::keyToHex(td.keyB)},
            {"access_bytes",  TrailerFormat::accessToHex(td.accessBytes)},
            {"access_valid",  td.valid}
        };

        if (td.valid) {
            json conditions = json::array();
            auto& dc = AccessBitsHelper::dataConditions();
            auto& tc = AccessBitsHelper::trailerConditions();
            for (int i = 0; i < 3; ++i) {
                uint8_t bits = td.access.data[i].bits();
                conditions.push_back({
                    {"block",       trailerBlk - (3-i)},
                    {"kind",        "data"},
                    {"condition",   bits},
                    {"description", bits < dc.size() ? dc[bits].label : "?"}
                });
            }
            {
                uint8_t bits = td.access.trailer.bits();
                conditions.push_back({
                    {"block",       trailerBlk},
                    {"kind",        "trailer"},
                    {"condition",   bits},
                    {"description", bits < tc.size() ? tc[bits].label : "?"}
                });
            }
            result["access_conditions"] = conditions;
        } else {
            result["access_error"] = td.parseError;
        }

        app.fmt.result(result);
        return ExitCode::Success;
    }
};

// ════════════════════════════════════════════════════════════════════════════════
// WriteTrailerCmd — write-trailer
// Trailer bloğunu yazar: KeyA + KeyB + access bytes
//
// UYARI: Yanlış access byte kartı kalıcı kilitler!
//        --dry-run ile önce doğrulayın.
//
// Required: --reader/-r, --sector/-s, --key-a, --key-b, --access (8 hex)
// Optional: --auth-key/-k (auth için kullanılacak key, default --key-a),
//           --key-type/-t, --card-type/-c
// ════════════════════════════════════════════════════════════════════════════════

class WriteTrailerCmd : public ICommand {
public:
    std::string name()        const override { return "write-trailer"; }
    std::string description() const override {
        return "Write the trailer block (KeyA + KeyB + access bytes).\n"
               "       WARNING: Wrong access bytes will permanently lock the card!\n"
               "       Use 'make-access' to calculate access bytes safely.\n"
               "       Use --dry-run to verify before writing.";
    }
    std::string usage() const override {
        return "scardtool write-trailer -r <reader> -s <sector>\n"
               "         --key-a <hex6> --key-b <hex6> --access <hex8>\n"
               "         [-k <auth-key>] [-t A|B] [-c classic1k]";
    }
    CommandMode mode() const override { return CommandMode::All; }

    std::vector<ParamDef> params() const override {
        return {
            {"reader",    'r', "Reader",                          true,  ""},
            {"sector",    's', "Sector number",                   true,  ""},
            {"key-a",     0,   "New KeyA (12 hex chars)",         true,  ""},
            {"key-b",     0,   "New KeyB (12 hex chars)",         true,  ""},
            {"access",    0,   "Access bytes + GPB (8 hex chars)",true,  ""},
            {"key",       'k', "Auth key (default: new KeyA)",    false, ""},
            {"key-type",  't', "Auth key type: A or B",          false, "A"},
            {"key-num",   'n', "Key slot",                        false, "01"},
            {"card-type", 'c', "classic1k|classic4k",            false, "classic1k"},
        };
    }
    std::vector<ArgDef> argDefs() const override {
        return {
            {"reader",'r',true},{"sector",'s',true},
            {"key-a",0,true},{"key-b",0,true},{"access",0,true},
            {"key",'k',true},{"key-type",'t',true},{"key-num",'n',true},
            {"card-type",'c',true},{"json",'j',false},
        };
    }

    ExitCode execute(CommandContext& ctx) override {
        auto& app = ctx.app;
        ParamResolver pr(ctx);

        auto readerVal  = pr.require("reader",  "Reader: ",         'r');
        auto sectorStr  = pr.require("sector",  "Sector: ",         's');
        auto keyAHex    = pr.require("key-a",   "New KeyA (hex6): ");
        auto keyBHex    = pr.require("key-b",   "New KeyB (hex6): ");
        auto accessHex  = pr.require("access",  "Access bytes (hex8 from make-access): ");
        if (!readerVal || !sectorStr || !keyAHex || !keyBHex || !accessHex)
            return ExitCode::InvalidArgs;

        auto keyA = TrailerFormat::parseKey(*keyAHex);
        auto keyB = TrailerFormat::parseKey(*keyBHex);
        if (!keyA) return fail(ExitCode::InvalidArgs, "Invalid KeyA: " + *keyAHex);
        if (!keyB) return fail(ExitCode::InvalidArgs, "Invalid KeyB: " + *keyBHex);

        auto abOpt = TrailerFormat::parseAccessBytes(*accessHex);
        if (!abOpt)
            return fail(ExitCode::InvalidArgs,
                        "Invalid access bytes: must be 8 hex chars (4 bytes). Got: " + *accessHex);

        // Access bytes bütünlük kontrolü
        auto saOpt = TrailerFormat::decodeAccess(*abOpt);
        if (!saOpt)
            return fail(ExitCode::InvalidArgs,
                        "Access bytes failed integrity check (bit complement mismatch).\n"
                        "       Use 'scardtool make-access' to generate valid access bytes.");

        CardType cardType = CardMath::fromString(
            pr.resolve("card-type",'c').value_or("classic1k"));
        int sector = std::stoi(*sectorStr);
        if (!CardMath::validSector(sector, cardType))
            return fail(ExitCode::InvalidArgs, "Sector out of range");

        // Auth için kullanılacak key (default: yeni KeyA)
        std::string authKeyHex = pr.resolve("key",'k').value_or(*keyAHex);
        auto authKey = TrailerFormat::parseKey(authKeyHex);
        if (!authKey) return fail(ExitCode::InvalidArgs, "Invalid auth key");

        std::string keyTypeStr = pr.resolve("key-type",'t').value_or("A");
        std::transform(keyTypeStr.begin(), keyTypeStr.end(), keyTypeStr.begin(), ::toupper);
        BYTE keyNum = 0x01;
        if (auto n = pr.resolve("key-num",'n')) {
            try { keyNum = static_cast<BYTE>(std::stoi(*n, nullptr, 16)); }
            catch (...) { keyNum = static_cast<BYTE>(std::stoi(*n)); }
        }
        BYTE keyTypeByte = (keyTypeStr == "A") ? 0x60 : 0x61;
        int trailerBlk = CardMath::trailerBlock(sector, cardType);

        // Trailer verisi
        BYTEV trailerData(16, 0);
        std::copy(keyA->begin(), keyA->end(), trailerData.begin());
        trailerData[6] = (*abOpt)[0];
        trailerData[7] = (*abOpt)[1];
        trailerData[8] = (*abOpt)[2];
        trailerData[9] = (*abOpt)[3]; // GPB
        std::copy(keyB->begin(), keyB->end(), trailerData.begin() + 10);

        if (app.flags.dryRun) {
            std::cout << "Trailer data that WOULD be written:\n"
                      << "  Block:   " << trailerBlk << "\n"
                      << "  KeyA:    " << *keyAHex << "\n"
                      << "  KeyB:    " << *keyBHex << "\n"
                      << "  Access:  " << *accessHex << "\n"
                      << "  Trailer: " << mcp::toHex(trailerData) << "\n";
            // Access condition summary
            if (!app.flags.jsonOutput) {
                AccessBitsHelper::printSummary(*saOpt);
            }
            return ExitCode::Success;
        }

        auto ec = UidCmd::ensureConnected(app, *readerVal);
        if (ec != ExitCode::Success) return ec;

        BYTEV ldApdu = PcscCommands::loadKey(0x20, keyNum, authKey->data(),
                                             static_cast<BYTE>(authKey->size()));
        auto lr = app.pcsc.trySendCommand(ldApdu, false);
        if (!lr.is_ok() || checkSW(lr.unwrap()) != 0x9000)
            return fail(ExitCode::CardCommError, "load-key failed");

        BYTEV authApdu = PcscCommands::authLegacy(
            static_cast<BYTE>(trailerBlk), keyTypeByte, keyNum);
        auto ar = app.pcsc.trySendCommand(authApdu, false);
        if (!ar.is_ok() || checkSW(ar.unwrap()) != 0x9000)
            return fail(ExitCode::CardCommError, "Auth failed");

        BYTEV wApdu = PcscCommands::updateBinary(
            static_cast<BYTE>(trailerBlk), trailerData.data(), 16);
        auto wr = app.pcsc.trySendCommand(wApdu, false);
        if (!wr.is_ok() || checkSW(wr.unwrap()) != 0x9000)
            return fail(ExitCode::CardCommError, "Write trailer failed");

        app.fmt.result({
            {"written",       true},
            {"sector",        sector},
            {"trailer_block", trailerBlk},
            {"key_a",         *keyAHex},
            {"key_b",         *keyBHex},
            {"access",        *accessHex}
        });
        return ExitCode::Success;
    }
};

// ════════════════════════════════════════════════════════════════════════════════
// MakeAccessCmd — make-access
// Mifare Classic access byte hesaplayıcı.
// --b0..--b3 ile her block için condition seçilir veya --interactive ile.
//
// Çıktı: 8 hex char (4 byte: C1byte C2byte C3byte GPB)
// write-trailer --access parametresine doğrudan geçilebilir.
//
// DESFire NOT: DESFire'da access rights uygulama bazlı ve farklı format.
//              Bu komut yalnızca Mifare Classic destekler.
// ════════════════════════════════════════════════════════════════════════════════

class MakeAccessCmd : public ICommand {
public:
    std::string name()        const override { return "make-access"; }
    std::string description() const override {
        return "Calculate Mifare Classic access bytes from human-readable conditions.\n"
               "       Output: 8-char hex suitable for write-trailer --access.\n"
               "       NOTE: For DESFire, access rights are application-level (different format).";
    }
    std::string usage() const override {
        return "scardtool make-access [--b0 <0-7>] [--b1 <0-7>] [--b2 <0-7>] [--bt <0-7>]\n"
               "  or:  scardtool make-access --interactive\n"
               "  or:  scardtool make-access --table (show condition table only)";
    }
    CommandMode mode() const override { return CommandMode::All; }

    std::vector<ParamDef> params() const override {
        return {
            {"b0",          0,   "Data block 0 condition (0-7)",  false, "0"},
            {"b1",          0,   "Data block 1 condition (0-7)",  false, "0"},
            {"b2",          0,   "Data block 2 condition (0-7)",  false, "0"},
            {"bt",          0,   "Trailer block condition (0-7)", false, "1"},
            {"gpb",         0,   "General Purpose Byte (hex, default 00)", false, "00"},
            {"interactive", 'i', "Interactive selection mode",    false, ""},
            {"table",       0,   "Show condition table and exit", false, ""},
        };
    }
    std::vector<ArgDef> argDefs() const override {
        return {
            {"b0",0,true},{"b1",0,true},{"b2",0,true},{"bt",0,true},
            {"gpb",0,true},{"interactive",'i',false},{"table",0,false},
            {"json",'j',false},
        };
    }

    ExitCode execute(CommandContext& ctx) override {
        auto& app = ctx.app;
        ParamResolver pr(ctx);

        // --table: sadece tabloyu göster
        if (pr.flag("table")) {
            AccessBitsHelper::printDataTable();
            AccessBitsHelper::printTrailerTable();
            return ExitCode::Success;
        }

        SectorAccess sa;

        if (pr.flag("interactive", 'i')) {
            // Interactive: terminal promptFn veya interactive mode promptFn
            if (!ctx.promptFn)
                return fail(ExitCode::InvalidArgs,
                            "Interactive mode requires 'scardtool interactive' or MCP mode");
            sa = AccessBitsHelper::interactiveSelect(ctx.promptFn);
        } else {
            // Parametreden oku
            auto parseCondition = [](const std::string& s) -> std::optional<uint8_t> {
                try {
                    int v = std::stoi(s);
                    if (v >= 0 && v <= 7) return static_cast<uint8_t>(v);
                } catch (...) {}
                return std::nullopt;
            };

            auto b0 = parseCondition(pr.resolve("b0").value_or("0"));
            auto b1 = parseCondition(pr.resolve("b1").value_or("0"));
            auto b2 = parseCondition(pr.resolve("b2").value_or("0"));
            auto bt = parseCondition(pr.resolve("bt").value_or("1"));

            if (!b0||!b1||!b2||!bt)
                return fail(ExitCode::InvalidArgs,
                            "Conditions must be 0-7. Use --table to see options.");

            sa.data[0] = AccessBitsHelper::fromBits(*b0);
            sa.data[1] = AccessBitsHelper::fromBits(*b1);
            sa.data[2] = AccessBitsHelper::fromBits(*b2);
            sa.trailer = AccessBitsHelper::fromBits(*bt);
        }

        uint8_t gpb = 0x00;
        if (auto g = pr.resolve("gpb")) {
            try { gpb = static_cast<uint8_t>(std::stoi(*g, nullptr, 16)); }
            catch (...) {}
        }

        auto ab = TrailerFormat::encodeAccess(sa, gpb);
        std::string hexStr = TrailerFormat::accessToHex(ab);

        if (app.flags.jsonOutput) {
            app.fmt.result({
                {"access_hex",   hexStr},
                {"gpb",          gpb},
                {"b0_condition", sa.data[0].bits()},
                {"b1_condition", sa.data[1].bits()},
                {"b2_condition", sa.data[2].bits()},
                {"bt_condition", sa.trailer.bits()},
            });
        } else {
            std::cout << "Access bytes: " << hexStr << "\n\n";
            AccessBitsHelper::printSummary(sa);
            std::cout << "\nUse with:\n"
                      << "  scardtool write-trailer ... --access " << hexStr << "\n";
        }

        return ExitCode::Success;
    }
};

// ════════════════════════════════════════════════════════════════════════════════
// CardInfoCmd — card-info
//
// Karta bağlanır, mümkün olan tüm bilgi sorgularını dener ve özetler.
// Her sorgu bağımsız denenir — başarısız olanlar "not supported" olarak
// işaretlenir, tüm komut başarısız olmaz.
//
// Denenen sorgular:
//   1. UID             (FF CA 00 00 00)  — tüm PC/SC kartlar
//   2. ATS             (FF CA 01 00 00)  — ISO 14443-4 kartlar
//   3. GET VERSION     (90 60 00 00 00)  — DESFire EV1/EV2/EV3
//                      + (90 AF 00 00 00) × 2 (3 parçalı yanıt)
//   4. NXP GET VERSION (60 00 00 00)     — Ultralight EV1/C/Nano
//   5. GET FREE MEMORY (90 6E 00 00 00)  — DESFire
//
// DESFire GET VERSION yanıtı 3 çerçevede gelir (SW=91AF → devam):
//   Frame 1: 7 byte hardware info
//   Frame 2: 7 byte software info
//   Frame 3: 14 byte UID + batch + production date
//
// Ultralight GET VERSION yanıtı 8 byte:
//   [0]   fixed header (0x00)
//   [1]   vendor ID   (0x04 = NXP)
//   [2]   product type
//   [3]   product subtype
//   [4]   major version
//   [5]   minor version
//   [6]   storage size
//   [7]   protocol type
//
// Required: --reader/-r
// Optional: --card-type/-c (hint), --json/-j
// ════════════════════════════════════════════════════════════════════════════════

class CardInfoCmd : public ICommand {
public:
    std::string name()        const override { return "card-info"; }
    std::string description() const override {
        return "Query all available card information (UID, ATS, version, capacity).\n"
               "       Probes multiple APDUs — unsupported ones are silently skipped.\n"
               "       Supported: all PC/SC cards, DESFire EV1/EV2/EV3, Ultralight EV1/C/Nano.";
    }
    std::string usage() const override {
        return "scardtool card-info -r <reader> [-c classic1k|classic4k|ultralight|desfire] [-j]";
    }
    CommandMode mode() const override { return CommandMode::All; }

    std::vector<ParamDef> params() const override {
        return {
            {"reader",    'r', "Reader index or name",              true,  ""},
            {"card-type", 'c', "Card type hint (auto-detect if omitted)", false, ""},
        };
    }
    std::vector<ArgDef> argDefs() const override {
        return {
            {"reader",       'r', true},
            {"card-type",    'c', true},
            {"json",         'j', false},
            {"save-session", 0,   false},
        };
    }

    ExitCode execute(CommandContext& ctx) override {
        auto& app = ctx.app;
        ParamResolver pr(ctx);

        auto readerVal = pr.require("reader", "Reader: ", 'r');
        if (!readerVal) return ExitCode::InvalidArgs;

        // Kart tipi belirtilmişse validate et (dry-run'da da kontrol yapılır)
        std::string cardTypeStr = pr.resolve("card-type", 'c').value_or("");
        if (!cardTypeStr.empty()) {
            auto ct = CardMath::fromString(cardTypeStr);
            if (ct == CardType::Unknown)
                return fail(ExitCode::InvalidArgs,
                    "Unknown card type: '" + cardTypeStr + "'. Use: classic1k, classic4k, ultralight, desfire");
        }

        if (app.flags.dryRun) {
            app.fmt.dryRun("card-info reader=" + *readerVal +
                           " (probes: UID, ATS, DESFire-GetVersion, UL-GetVersion, FreeMemory)");
            return ExitCode::Success;
        }

        auto ec = UidCmd::ensureConnected(app, *readerVal);
        if (ec != ExitCode::Success) return ec;

        json info = json::object();
        info["reader"] = *readerVal;

        // ── 1. UID ───────────────────────────────────────────────────────────
        {
            auto r = app.pcsc.trySendCommand({0xFF,0xCA,0x00,0x00,0x00}, false);
            if (r.is_ok() && checkSW(r.unwrap()) == 0x9000) {
                const BYTEV& resp = r.unwrap();
                BYTEV data(resp.begin(), resp.end()-2);
                info["uid"] = mcp::toHex(data);
                info["uid_length"] = static_cast<int>(data.size());
            } else {
                info["uid"] = nullptr;
            }
        }

        // ── 2. ATS (ISO 14443-4) ─────────────────────────────────────────────
        {
            auto r = app.pcsc.trySendCommand({0xFF,0xCA,0x01,0x00,0x00}, false);
            if (r.is_ok() && checkSW(r.unwrap()) == 0x9000) {
                const BYTEV& resp = r.unwrap();
                BYTEV data(resp.begin(), resp.end()-2);
                info["ats"] = mcp::toHex(data);
            } else {
                info["ats"] = nullptr;
            }
        }

        // ── 3. DESFire GET VERSION (90 60 00 00 00) ──────────────────────────
        // 3 frame: hardware → software → uid/batch
        // Her frame 91 AF ile biter, son frame 91 00 ile biter
        {
            auto df = tryDesfireGetVersion(app);
            if (df) {
                info["desfire_version"] = *df;
            }
        }

        // ── 4. NXP Ultralight GET VERSION (60 00 00 00) ──────────────────────
        // Bazı reader'lar native NFC komutunu doğrudan geçirir
        {
            auto ul = tryUltralightGetVersion(app);
            if (ul) {
                info["ultralight_version"] = *ul;
            }
        }

        // ── 5. DESFire GET FREE MEMORY (90 6E 00 00 00) ──────────────────────
        {
            auto r = app.pcsc.trySendCommand({0x90,0x6E,0x00,0x00,0x00}, false);
            if (r.is_ok() && checkSW(r.unwrap()) == 0x9100) {
                const BYTEV& resp = r.unwrap();
                if (resp.size() >= 5) {
                    uint32_t free_mem =
                        static_cast<uint32_t>(resp[0]) |
                        (static_cast<uint32_t>(resp[1]) << 8) |
                        (static_cast<uint32_t>(resp[2]) << 16);
                    info["desfire_free_memory"] = free_mem;
                }
            }
        }

        // ── Card type guess ──────────────────────────────────────────────────
        std::string detectedType = detectType(info);
        info["detected_type"] = detectedType;

        if (!app.flags.jsonOutput) {
            printHuman(info);
        } else {
            app.fmt.result(info);
        }

        // ── --save-session: reader + detected card-type kaydet ───────────────
        if (pr.flag("save-session")) {
            app.session.set("reader", *readerVal);
            if (!detectedType.empty() && detectedType != "unknown") {
                app.session.set("card-type", detectedType);
                std::cout << "Saved: reader=" << *readerVal
                          << " card-type=" << detectedType << " to session\n";
            } else {
                std::cout << "Saved: reader=" << *readerVal << " to session\n";
            }
            app.session.save();
        }

        return ExitCode::Success;
    }

private:
    // DESFire GET VERSION — 3 parçalı yanıt
    std::optional<json> tryDesfireGetVersion(App& app) {
        // Frame 1: hardware (90 60 00 00 00)
        auto r1 = app.pcsc.trySendCommand({0x90,0x60,0x00,0x00,0x00}, false);
        if (!r1.is_ok()) return std::nullopt;
        const BYTEV& raw1 = r1.unwrap();
        if (raw1.size() < 2) return std::nullopt;
        uint16_t sw1 = checkSW(raw1);
        if (sw1 != 0x91AF && sw1 != 0x9100) return std::nullopt; // DESFire değil
        BYTEV hw(raw1.begin(), raw1.end()-2);

        // Frame 2: software (90 AF 00 00 00)
        auto r2 = app.pcsc.trySendCommand({0x90,0xAF,0x00,0x00,0x00}, false);
        if (!r2.is_ok() || checkSW(r2.unwrap()) != 0x91AF) return std::nullopt;
        BYTEV sw(r2.unwrap().begin(), r2.unwrap().end()-2);

        // Frame 3: uid+batch (90 AF 00 00 00)
        auto r3 = app.pcsc.trySendCommand({0x90,0xAF,0x00,0x00,0x00}, false);
        if (!r3.is_ok() || checkSW(r3.unwrap()) != 0x9100) return std::nullopt;
        BYTEV uid(r3.unwrap().begin(), r3.unwrap().end()-2);

        json v;
        // Hardware frame (7 bytes)
        if (hw.size() >= 7) {
            v["hw_vendor_id"]    = hw[0];
            v["hw_type"]         = hw[1];
            v["hw_subtype"]      = hw[2];
            v["hw_version"]      = std::to_string(hw[3]) + "." + std::to_string(hw[4]);
            v["hw_storage_size"] = storageSizeToBytes(hw[5]);
            v["hw_protocol"]     = hw[6];
        }
        // Software frame (7 bytes)
        if (sw.size() >= 7) {
            v["sw_vendor_id"]    = sw[0];
            v["sw_type"]         = sw[1];
            v["sw_subtype"]      = sw[2];
            v["sw_version"]      = std::to_string(sw[3]) + "." + std::to_string(sw[4]);
            v["sw_storage_size"] = storageSizeToBytes(sw[5]);
            v["sw_protocol"]     = sw[6];
            // EV variant tahmini
            if      (sw[3] >= 3) v["variant"] = "DESFire EV3";
            else if (sw[3] >= 2) v["variant"] = "DESFire EV2";
            else if (sw[3] == 1) v["variant"] = "DESFire EV1";
            else                 v["variant"] = "DESFire Light";
        }
        // UID+batch frame (14 bytes)
        if (uid.size() >= 14) {
            BYTEV uidBytes(uid.begin(), uid.begin()+7);
            BYTEV batchBytes(uid.begin()+7, uid.begin()+12);
            v["uid"]              = mcp::toHex(uidBytes);
            v["batch_no"]         = mcp::toHex(batchBytes);
            v["production_week"]  = uid[12];
            v["production_year"]  = 2000 + uid[13];
        }
        v["raw_hw"]  = mcp::toHex(hw);
        v["raw_sw"]  = mcp::toHex(sw);
        v["raw_uid"] = mcp::toHex(uid);
        return v;
    }

    // NXP Ultralight/NTAG GET VERSION (60 00 00 00)
    std::optional<json> tryUltralightGetVersion(App& app) {
        // Bazı reader'lar 4 byte LE'li ISO komut olarak gönderir
        auto r = app.pcsc.trySendCommand({0x60, 0x00, 0x00, 0x00}, false);
        if (!r.is_ok()) return std::nullopt;
        const BYTEV& raw = r.unwrap();
        if (raw.size() < 2) return std::nullopt;
        if (checkSW(raw) != 0x9000) return std::nullopt;
        BYTEV data(raw.begin(), raw.end()-2);
        if (data.size() < 8) return std::nullopt; // beklenen 8 byte

        json v;
        v["raw"]          = mcp::toHex(data);
        v["vendor_id"]    = data[1]; // 0x04 = NXP
        v["product_type"] = data[2];
        v["product_sub"]  = data[3];
        v["version"]      = std::to_string(data[4]) + "." + std::to_string(data[5]);
        v["storage_size"] = ultralightStorageSize(data[6]);
        v["protocol"]     = data[7];

        // Product type decode
        v["product_name"] = decodeUltralightProduct(data[2], data[3]);
        return v;
    }

    // DESFire storage size byte → bytes
    static uint32_t storageSizeToBytes(BYTE ss) {
        switch (ss) {
            case 0x16: return 2048;
            case 0x18: return 4096;
            case 0x1A: return 8192;
            case 0x1C: return 16384;
            case 0x1E: return 32768;
            default:   return 0;
        }
    }

    // Ultralight storage size byte → bytes
    static uint32_t ultralightStorageSize(BYTE ss) {
        // NXP AN12196: ss byte — 2^((ss>>1)+1) bytes
        // Bilinen değerler:
        switch (ss) {
            case 0x0B: return 48;    // Ultralight (64 byte - 16 overhead)
            case 0x0E: return 128;   // Ultralight C / EV1 80p
            case 0x11: return 256;   // NTAG210
            case 0x13: return 512;   // NTAG213
            case 0x15: return 1024;  // NTAG215
            case 0x17: return 2048;  // NTAG216
            case 0x1A: return 4096;  // NTAG I2C 1k
            default:   return 0;
        }
    }

    // Ultralight product type decode
    static std::string decodeUltralightProduct(BYTE type, BYTE subtype) {
        if (type == 0x03) {
            switch (subtype) {
                case 0x01: return "Ultralight EV1";
                case 0x02: return "Ultralight EV1 (48 byte)";
                case 0x03: return "Ultralight C";
                case 0x04: return "Ultralight Nano";
                default:   return "Ultralight (unknown subtype)";
            }
        }
        if (type == 0x04) return "NTAG";
        if (type == 0x05) return "NTAG I2C";
        return "Unknown NXP product";
    }

    // ATS/versiyondan kart tipi tahmini
    static std::string detectType(const json& info) {
        if (!info["desfire_version"].is_null())
            return info["desfire_version"].value("variant", "DESFire");
        if (!info["ultralight_version"].is_null())
            return info["ultralight_version"].value("product_name", "Ultralight");
        // ATS'den tahmin
        if (!info["ats"].is_null()) {
            std::string ats = info["ats"].get<std::string>();
            if (ats.substr(0,2) == "06") return "Mifare Classic (ISO 14443-3)";
            return "ISO 14443-4 compatible";
        }
        if (!info["uid"].is_null()) {
            int uid_len = info.value("uid_length", 0);
            if (uid_len == 4) return "Mifare Classic 1K/4K (likely)";
            if (uid_len == 7) return "Mifare Ultralight / NTAG (likely)";
        }
        return "Unknown";
    }

    // Human-readable çıktı
    static void printHuman(const json& info) {
        std::cout << "┌─ Card Information ─────────────────────────────────────┐\n";
        auto print = [](const std::string& k, const std::string& v) {
            std::cout << "│  " << std::left << std::setw(22) << (k+":") << " " << v << "\n";
        };

        print("Detected Type", info.value("detected_type", "?"));
        if (!info["uid"].is_null())
            print("UID", info["uid"].get<std::string>() +
                  " (" + std::to_string(info.value("uid_length",0)) + " bytes)");
        if (!info["ats"].is_null())
            print("ATS", info["ats"].get<std::string>());

        // DESFire
        if (!info["desfire_version"].is_null()) {
            auto& dv = info["desfire_version"];
            std::cout << "│\n│  ── DESFire Version ──────────────────────────────────\n";
            if (dv.contains("variant"))    print("  Variant",    dv["variant"]);
            if (dv.contains("hw_version")) print("  HW Version", dv["hw_version"]);
            if (dv.contains("sw_version")) print("  SW Version", dv["sw_version"]);
            if (dv.contains("hw_storage_size") && dv["hw_storage_size"] > 0)
                print("  Capacity",   std::to_string(dv["hw_storage_size"].get<uint32_t>()/1024) + " KB");
            if (dv.contains("production_year"))
                print("  Produced",   "Week " + std::to_string(dv["production_week"].get<int>()) +
                                      " / " + std::to_string(dv["production_year"].get<int>()));
            if (dv.contains("batch_no"))   print("  Batch No",   dv["batch_no"]);
        }

        // Ultralight
        if (!info["ultralight_version"].is_null()) {
            auto& uv = info["ultralight_version"];
            std::cout << "│\n│  ── Ultralight Version ───────────────────────────────\n";
            if (uv.contains("product_name")) print("  Product",  uv["product_name"]);
            if (uv.contains("version"))      print("  Version",  uv["version"]);
            if (uv.contains("storage_size") && uv["storage_size"] > 0)
                print("  Storage",   std::to_string(uv["storage_size"].get<uint32_t>()) + " bytes");
            print("  Raw", uv.value("raw", "?"));
        }

        // DESFire free memory
        if (info.contains("desfire_free_memory") && !info["desfire_free_memory"].is_null())
            print("Free Memory", std::to_string(info["desfire_free_memory"].get<uint32_t>()) + " bytes");

        std::cout << "└────────────────────────────────────────────────────────┘\n";
    }
};

// ════════════════════════════════════════════════════════════════════════════════
// ExplainSwCmd — explain-sw <SW>
// ════════════════════════════════════════════════════════════════════════════════
class ExplainSwCmd : public ICommand {
public:
    std::string name()        const override { return "explain-sw"; }
    std::string description() const override {
        return "Explain a Status Word code (e.g. 6982, 9000, 91AF).\n"
               "       Shows name, category, description and resolution hints.";
    }
    std::string usage() const override {
        return "scardtool explain-sw <SW>   (e.g. 6982 or 9000)";
    }
    CommandMode mode() const override { return CommandMode::All; }
    std::vector<ParamDef> params() const override {
        return { {"sw", 0, "4-char hex SW code", true, ""} };
    }
    std::vector<ArgDef> argDefs() const override {
        return { {"json", 'j', false} };
    }

    ExitCode execute(CommandContext& ctx) override {
        auto& app = ctx.app;
        if (ctx.args.positional.empty())
            return fail(ExitCode::InvalidArgs, "Usage: " + usage());

        std::string swStr;
        for (char c : ctx.args.positional[0])
            if (c != ' ') swStr += static_cast<char>(std::toupper(c));

        if (swStr.size() != 4 || !StatusWordCatalog::isValidHex(swStr))
            return fail(ExitCode::InvalidArgs,
                "SW must be 4 hex chars (e.g. 6982). Got: '" +
                ctx.args.positional[0] + "'");

        auto entry = StatusWordCatalog::lookup(swStr);
        if (!entry) {
            app.fmt.result({ {"sw", swStr}, {"known", false}, {"message", "Unknown SW"} });
            return ExitCode::Success;
        }

        if (app.flags.jsonOutput) {
            app.fmt.result({
                {"sw", swStr}, {"known", true},
                {"name", entry->name}, {"category", entry->category},
                {"description", entry->description},
                {"resolution", entry->resolution},
                {"seealso", entry->seealso}
            });
        } else {
            std::cout << "\n  SW:         " << swStr
                      << "\n  Name:       " << entry->name
                      << "\n  Category:   " << entry->category
                      << "\n\n  Description:\n    " << entry->description << "\n";
            if (!entry->resolution.empty())
                std::cout << "\n  Resolution:\n    " << entry->resolution << "\n";
            if (!entry->seealso.empty()) {
                std::cout << "\n  See also:   ";
                for (size_t i = 0; i < entry->seealso.size(); ++i) {
                    if (i) std::cout << ", ";
                    std::cout << entry->seealso[i];
                }
                std::cout << "\n";
            }
            std::cout << "\n";
        }
        return ExitCode::Success;
    }
};

// ════════════════════════════════════════════════════════════════════════════════
// ExplainApduCmd — explain-apdu <hex|MACRO>
// ════════════════════════════════════════════════════════════════════════════════
class ExplainApduCmd : public ICommand {
public:
    std::string name()        const override { return "explain-apdu"; }
    std::string description() const override {
        return "Parse and explain an APDU command (hex or macro name).\n"
               "       Shows CLA, INS, P1, P2 decoded from the catalog.";
    }
    std::string usage() const override {
        return "scardtool explain-apdu <hex|MACRO>  (e.g. FFCA000000 or GETUID)";
    }
    CommandMode mode() const override { return CommandMode::All; }
    std::vector<ParamDef> params() const override {
        return { {"apdu", 0, "Hex APDU or macro name", true, ""} };
    }
    std::vector<ArgDef> argDefs() const override {
        return { {"json", 'j', false} };
    }

    ExitCode execute(CommandContext& ctx) override {
        auto& app = ctx.app;
        if (ctx.args.positional.empty())
            return fail(ExitCode::InvalidArgs, "Usage: " + usage());

        std::string input = ctx.args.positional[0];
        std::string resolvedHex;
        std::optional<ApduMacro> usedMacro;

        MacroRegistry::instance().loadDefaults();
        try {
            if (!MacroRegistry::instance().resolve(input, resolvedHex, &usedMacro))
                return fail(ExitCode::InvalidArgs,
                    "Unknown macro or invalid hex: '" + input + "'");
        } catch (const std::exception& e) {
            return fail(ExitCode::InvalidArgs, e.what());
        }
        if (resolvedHex.size() < 8)
            return fail(ExitCode::InvalidArgs, "APDU must be at least 4 bytes");

        uint8_t cla = static_cast<uint8_t>(std::stoi(resolvedHex.substr(0,2),nullptr,16));
        uint8_t ins = static_cast<uint8_t>(std::stoi(resolvedHex.substr(2,2),nullptr,16));
        uint8_t p1  = static_cast<uint8_t>(std::stoi(resolvedHex.substr(4,2),nullptr,16));
        uint8_t p2  = static_cast<uint8_t>(std::stoi(resolvedHex.substr(6,2),nullptr,16));
        std::string dataHex = resolvedHex.size() > 8 ? resolvedHex.substr(8) : "";

        auto insEntry = InsCatalog::lookup(cla, ins);

        auto hex2 = [](uint8_t b) {
            std::ostringstream o;
            o << std::hex << std::uppercase << std::setfill('0') << std::setw(2) << (int)b;
            return o.str();
        };

        if (app.flags.jsonOutput) {
            app.fmt.result({
                {"input", input}, {"resolved", resolvedHex},
                {"macro", usedMacro ? usedMacro->name : ""},
                {"cla", hex2(cla)}, {"ins", hex2(ins)},
                {"ins_name", insEntry ? insEntry->name : "Unknown"},
                {"p1", hex2(p1)}, {"p2", hex2(p2)}, {"data", dataHex},
                {"standard", insEntry ? insEntry->standard : ""}
            });
        } else {
            std::cout << "\n";
            if (usedMacro)
                std::cout << "  Input:    " << input << "  →  " << resolvedHex << "\n"
                          << "  Macro:    " << usedMacro->name << "\n\n";
            else
                std::cout << "  Input:    " << resolvedHex << "\n\n";

            std::string claDesc;
            if (cla==0xFF) claDesc="PC/SC pseudo-APDU";
            else if (cla==0x90) claDesc="DESFire native";
            else claDesc="ISO 7816-4";

            std::cout << "  CLA = " << hex2(cla) << "   " << claDesc << "\n"
                      << "  INS = " << hex2(ins) << "   ";
            if (insEntry)
                std::cout << insEntry->name << "\n             " << insEntry->description
                          << "\n             Standard: " << insEntry->standard << "\n";
            else
                std::cout << "(unknown INS)\n";

            std::cout << "  P1  = " << hex2(p1) << "\n"
                      << "  P2  = " << hex2(p2) << "\n";
            if (!dataHex.empty())
                std::cout << "  Data: " << dataHex << "\n";

            if (usedMacro && !usedMacro->notes.empty())
                std::cout << "\n  Notes:\n    " << usedMacro->notes << "\n";
            if (usedMacro && !usedMacro->example.empty())
                std::cout << "\n  Example:\n    " << usedMacro->example << "\n";
            std::cout << "\n";
        }
        return ExitCode::Success;
    }
};

// ════════════════════════════════════════════════════════════════════════════════
// ListMacrosCmd — list-macros [--filter group]
// ════════════════════════════════════════════════════════════════════════════════
class ListMacrosCmd : public ICommand {
public:
    std::string name()        const override { return "list-macros"; }
    std::string description() const override {
        return "List all available APDU macros with their hex equivalents.";
    }
    std::string usage() const override {
        return "scardtool list-macros [--filter|-g general|pcsc|desfire|ultralight|user] [-j]";
    }
    CommandMode mode() const override { return CommandMode::All; }
    std::vector<ParamDef> params() const override {
        return { {"filter", 'f', "Filter by group", false, ""} };
    }
    std::vector<ArgDef> argDefs() const override {
        return { {"filter", 'g', true}, {"json", 'j', false} };
    }

    ExitCode execute(CommandContext& ctx) override {
        auto& app = ctx.app;
        ParamResolver pr(ctx);

        std::string filter = pr.resolve("filter", 'g').value_or("");
        for (char& c : filter) c = static_cast<char>(std::toupper(c));

        MacroRegistry::instance().loadDefaults();
        std::string grpArg = filter.empty() ? "" : capitalize(filter);
        auto macros = MacroRegistry::instance().byGroup(grpArg);

        if (app.flags.jsonOutput) {
            json arr = json::array();
            for (auto* m : macros)
                arr.push_back({ {"name",m->name}, {"usage",m->usage()},
                    {"description",m->description}, {"group",m->group},
                    {"template",m->templateFormatted()} });
            app.fmt.result({ {"macros",arr}, {"count",arr.size()} });
        } else {
            // Group'lara böl
            std::map<std::string,std::vector<const ApduMacro*>> groups;
            for (auto* m : macros) groups[m->group].push_back(m);

            std::cout << "\n┌─ APDU Macros " << std::string(57,'─') << "┐\n│\n";
            for (const auto& grp : std::vector<std::string>{"General","PCSC","DESFire","Ultralight","User"}) {
                auto it = groups.find(grp);
                if (it == groups.end() || it->second.empty()) continue;
                std::cout << "│  " << grp << "\n│  " << std::string(69,'─') << "\n";
                for (auto* m : it->second) {
                    std::string u = m->usage();
                    std::string d = m->description;
                    if (d.size()>35) d=d.substr(0,32)+"...";
                    std::string t = m->templateFormatted();
                    if (t.size()>22) t=t.substr(0,19)+"...";
                    std::cout << "│  " << std::left << std::setw(28) << u
                              << std::setw(36) << d << t << "\n";
                }
                std::cout << "│\n";
            }
            std::cout << "└" << std::string(71,'─') << "┘\n\n"
                      << "  Total: " << macros.size() << " macro(s)  |  "
                      << MacroRegistry::instance().userMacroCount() << " user macro(s)\n"
                      << "  Use 'explain-macro <NAME>' for detailed help\n"
                      << "  Custom: ./.scard_macros or ~/.scard_macros\n\n";
        }
        return ExitCode::Success;
    }

private:
    static std::string capitalize(const std::string& s) {
        if (s.empty()) return s;
        // Özel durumlar: büyük harf kuralı farklı
        std::string u; for (char c : s) u += static_cast<char>(std::toupper(c));
        if (u == "PCSC")     return "PCSC";
        if (u == "DESFIRE")  return "DESFire";
        if (u == "GENERAL")  return "General";
        if (u == "ULTRALIGHT") return "Ultralight";
        if (u == "USER")     return "User";
        // Varsayılan: ilk harf büyük
        std::string r;
        r += static_cast<char>(std::toupper(s[0]));
        for (size_t i=1;i<s.size();++i) r+=static_cast<char>(std::tolower(s[i]));
        return r;
    }
};

// ════════════════════════════════════════════════════════════════════════════════
// ExplainMacroCmd — explain-macro <n>
// ════════════════════════════════════════════════════════════════════════════════
class ExplainMacroCmd : public ICommand {
public:
    std::string name()        const override { return "explain-macro"; }
    std::string description() const override {
        return "Show detailed documentation for a specific APDU macro.";
    }
    std::string usage() const override {
        return "scardtool explain-macro <MACRO_NAME>  (e.g. GETUID, READ_BINARY)";
    }
    CommandMode mode() const override { return CommandMode::All; }
    std::vector<ParamDef> params() const override {
        return { {"name", 0, "Macro name", true, ""} };
    }
    std::vector<ArgDef> argDefs() const override {
        return { {"json", 'j', false} };
    }

    ExitCode execute(CommandContext& ctx) override {
        auto& app = ctx.app;
        if (ctx.args.positional.empty())
            return fail(ExitCode::InvalidArgs,
                "Usage: scardtool explain-macro <MACRO_NAME>\n"
                "       Run 'scardtool list-macros' to see all macros.");

        MacroRegistry::instance().loadDefaults();
        auto macro = MacroRegistry::instance().find(ctx.args.positional[0]);
        if (!macro)
            return fail(ExitCode::GeneralError,
                "Unknown macro: '" + ctx.args.positional[0] + "'\n"
                "       Run 'scardtool list-macros' to see available macros.");

        if (app.flags.jsonOutput) {
            app.fmt.result({
                {"name",macro->name},{"usage",macro->usage()},
                {"description",macro->description},{"group",macro->group},
                {"standard",macro->standard},{"template",macro->templateFormatted()},
                {"params",macro->params},{"notes",macro->notes},
                {"example",macro->example},{"seealso",macro->seealso}
            });
        } else {
            std::cout << "\n  Macro:      " << macro->name
                      << "\n  Group:      " << macro->group;
            if (!macro->standard.empty())
                std::cout << "\n  Standard:   " << macro->standard;
            std::cout << "\n";
            if (macro->params.empty())
                std::cout << "  Parameters: none\n";
            else {
                std::cout << "  Parameters:\n";
                for (const auto& p : macro->params)
                    std::cout << "    <" << p << ">  (hex bytes)\n";
            }
            std::cout << "\n  APDU:  " << macro->templateFormatted() << "\n";
            if (!macro->params.empty())
                std::cout << "  Usage: " << macro->usage() << "\n";
            else {
                try { std::cout << "  Hex:   " << macro->resolveFixed() << "\n"; }
                catch(...) {}
            }
            std::cout << "\n  Description:\n    " << macro->description << "\n";
            if (!macro->notes.empty())
                std::cout << "\n  Notes:\n    " << macro->notes << "\n";
            if (!macro->example.empty())
                std::cout << "\n  Example:\n    " << macro->example << "\n";
            if (!macro->seealso.empty()) {
                std::cout << "\n  See also:  ";
                for (size_t i=0;i<macro->seealso.size();++i) {
                    if (i) std::cout << ", ";
                    std::cout << macro->seealso[i];
                }
                std::cout << "\n";
            }
            std::cout << "\n";
        }
        return ExitCode::Success;
    }
};

// ════════════════════════════════════════════════════════════════════════════════
// DetectCipherCmd — detect-cipher
// Kartı sorgular, blok boyutunu belirler ve uygun şifreleme algoritmasını önerir.
// Sonuç --save-session ile session dosyasına kaydedilebilir.
//
// Çakışma kuralı:
//   --cipher belirtilmişse detect sonucu gösterilir ama ignored → uyarı verilir.
//   detect-cipher + --save-session → session'a cipher=<algo> yazar.
// ════════════════════════════════════════════════════════════════════════════════

class DetectCipherCmd : public ICommand {
public:
    std::string name()        const override { return "detect-cipher"; }
    std::string description() const override {
        return "Probe card type and recommend best encryption cipher.\n"
               "       Use --save-session to persist the recommendation.\n"
               "       If --cipher is also set, detect result is shown but ignored.";
    }
    std::string usage() const override {
        return "scardtool detect-cipher -r <reader> [--save-session] [-j]";
    }
    CommandMode mode() const override { return CommandMode::All; }

    std::vector<ParamDef> params() const override {
        return { {"reader", 'r', "Reader index or name", true, ""} };
    }
    std::vector<ArgDef> argDefs() const override {
        return {
            {"reader",       'r', true},
            {"save-session", 0,   false},
            {"json",         'j', false},
        };
    }

    ExitCode execute(CommandContext& ctx) override {
        auto& app = ctx.app;
        ParamResolver pr(ctx);

        auto readerVal = pr.require("reader", "Reader: ", 'r');
        if (!readerVal) return ExitCode::InvalidArgs;

        if (app.flags.dryRun) {
            app.fmt.dryRun("detect-cipher reader=" + *readerVal);
            return ExitCode::Success;
        }

        auto ec = UidCmd::ensureConnected(app, *readerVal);
        if (ec != ExitCode::Success) return ec;

        // ── UID ──────────────────────────────────────────────────────────────
        std::string uid, ats;
        {
            auto r = app.pcsc.trySendCommand({0xFF,0xCA,0x00,0x00,0x00}, false);
            if (r.is_ok() && checkSW(r.unwrap()) == 0x9000) {
                const BYTEV& resp = r.unwrap();
                uid = mcp::toHex(BYTEV(resp.begin(), resp.end()-2));
            }
        }

        // ── ATS ──────────────────────────────────────────────────────────────
        {
            auto r = app.pcsc.trySendCommand({0xFF,0xCA,0x01,0x00,0x00}, false);
            if (r.is_ok() && checkSW(r.unwrap()) == 0x9000) {
                const BYTEV& resp = r.unwrap();
                ats = mcp::toHex(BYTEV(resp.begin(), resp.end()-2));
            }
        }

        // ── Kart tipi tespiti ────────────────────────────────────────────────
        CardType cardType = CardType::Unknown;
        std::string detectedName = "Unknown";
        {
            // DESFire: GET VERSION
            auto r = app.pcsc.trySendCommand({0x90,0x60,0x00,0x00,0x00}, false);
            if (r.is_ok()) {
                uint16_t sw = checkSW(r.unwrap());
                if (sw == 0x91AF || sw == 0x9100) {
                    cardType    = CardType::MifareDesfire;
                    detectedName = "Mifare DESFire";
                }
            }
            // Ultralight: GET VERSION (60 00 00 00)
            if (cardType == CardType::Unknown) {
                auto rv = app.pcsc.trySendCommand({0x60,0x00,0x00,0x00}, false);
                if (rv.is_ok() && checkSW(rv.unwrap()) == 0x9000
                    && rv.unwrap().size() >= 9) {
                    cardType    = CardType::MifareUltralight;
                    detectedName = "Mifare Ultralight/NTAG";
                }
            }
            // Mifare Classic: UID 4 byte → Classic 1K
            if (cardType == CardType::Unknown) {
                if (uid.size() == 8) {
                    cardType    = CardType::MifareClassic1K;
                    detectedName = "Mifare Classic 1K (likely)";
                } else if (uid.size() == 14) {
                    cardType    = CardType::MifareClassic1K;
                    detectedName = "Mifare Classic / 7-byte UID";
                }
            }
        }

        int blockSize = CardMath::blockSize(
            cardType == CardType::Unknown ? CardType::MifareClassic1K : cardType);

        // ── Analiz ───────────────────────────────────────────────────────────
        auto result = CardCrypto::analyze(cardType, blockSize, uid, ats);

        // ── Çakışma uyarısı ──────────────────────────────────────────────────
        bool hasExplicitCipher = (app.flags.cipher != CipherAlgo::Detect
                                  && app.flags.cipher != CipherAlgo::None);
        if (hasExplicitCipher && !app.flags.noWarn) {
            app.fmt.warn("--cipher " + CardCrypto::toString(app.flags.cipher) +
                " is set — detect-cipher result (" +
                CardCrypto::toString(result.recommended) +
                ") is shown but NOT applied.");
        }

        // ── Çıktı ────────────────────────────────────────────────────────────
        if (app.flags.jsonOutput) {
            json opts = json::array();
            for (const auto& o : result.options) {
                opts.push_back({
                    {"name",        o.name},
                    {"display",     o.displayName},
                    {"suitable",    o.suitable},
                    {"recommended", o.recommended},
                    {"reason",      o.reason}
                });
            }
            app.fmt.result({
                {"uid",           uid},
                {"ats",           ats},
                {"detected_type", detectedName},
                {"block_size",    blockSize},
                {"recommended",   CardCrypto::toString(result.recommended)},
                {"options",       opts}
            });
        } else {
            printHuman(result, uid, ats, detectedName, blockSize,
                       hasExplicitCipher, app.flags.cipher);
        }

        // ── --save-session ────────────────────────────────────────────────────
        if (pr.flag("save-session") && !hasExplicitCipher) {
            app.session.set("cipher", CardCrypto::toString(result.recommended));
            app.session.save();
            std::cout << "Saved: cipher=" << CardCrypto::toString(result.recommended)
                      << " to session\n";
        }

        return ExitCode::Success;
    }

private:
    static void printHuman(const CipherDetectResult& r,
                            const std::string& uid, const std::string& ats,
                            const std::string& detectedName, int blockSize,
                            bool conflictsWithExplicit, CipherAlgo explicitAlgo) {
        std::cout << "\n"
                  << "  Card UID:      " << (uid.empty() ? "N/A" : uid) << "\n"
                  << "  ATS:           " << (ats.empty() ? "N/A" : ats) << "\n"
                  << "  Detected type: " << detectedName << "\n"
                  << "  Block size:    " << blockSize << " bytes\n\n"
                  << "  Cipher recommendation:\n";

        for (const auto& o : r.options) {
            std::string marker = o.recommended ? "  ✓ (recommended)" :
                                 o.suitable    ? "  ✓" : "  ✗";
            std::cout << "    " << (o.suitable ? "✓" : "✗") << "  "
                      << std::left << std::setw(14) << o.name
                      << std::setw(20) << o.displayName;
            if (!o.suitable)
                std::cout << "  NOT suitable — " << o.reason;
            else if (o.recommended)
                std::cout << "  ← recommended";
            std::cout << "\n";
        }

        std::cout << "\n  Auto-select:   "
                  << CardCrypto::toString(r.recommended)
                  << " (" << CardCrypto::displayName(r.recommended) << ")\n";

        if (conflictsWithExplicit) {
            std::cout << "\n  ⚠ Note: --cipher "
                      << CardCrypto::toString(explicitAlgo)
                      << " overrides this recommendation.\n";
        }

        std::cout << "\n  Usage:\n"
                  << "    scardtool write -r 0 -p 4 -d <hex> -E"
                  << " --cipher " << CardCrypto::toString(r.recommended)
                  << " -P <password>\n"
                  << "    scardtool read  -r 0 -p 4 -l " << blockSize << " -E"
                  << " --cipher " << CardCrypto::toString(r.recommended)
                  << " -P <password>\n"
                  << "\n  Or save to session:\n"
                  << "    scardtool detect-cipher -r 0 --save-session\n\n";
    }
};

} // namespace scardtool::cmds