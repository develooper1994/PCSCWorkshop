/** @file McpMode.h
 * @brief MCP server mode — wraps commands as MCP tools over App&. */
#pragma once
#include "App/App.h"
#include "Commands/Commands.h"
#include "Mcp/McpServer.h"
#include "Mcp/ToolRegistry.h"
#include "Mcp/HexUtils.h"
#include "ExitCode.h"
#include <string>
#include <optional>

// ════════════════════════════════════════════════════════════════════════════════
// McpMode — MCP server modu
//
// CLI command'ların aynı core logic'ini MCP tool wrapper'ları olarak expose eder.
// App& üzerinden çalışır — PcscSession singleton yok.
//
// Desteklenen transport:
//   stdio (varsayılan)
//   TCP socket: --socket/-K <port>   [TODO: socket transport]
// ════════════════════════════════════════════════════════════════════════════════

class McpMode {
public:
    explicit McpMode(App& app) : app_(app) {}

    ExitCode run(std::optional<int> /*socketPort*/ = std::nullopt) {
        // TODO: socket port verilmişse TCP transport kur
        // Şimdilik her zaman stdio

        mcp::ToolRegistry registry;
        registerTools(registry);

        mcp::McpServer server({"scardtool", "1.0.0"}, registry);
        server.run();
        return ExitCode::Success;
    }

private:
    App& app_;

    void registerTools(mcp::ToolRegistry& reg) {
        // Temel
        registerListReaders(reg);
        registerConnect(reg);
        registerDisconnect(reg);
        registerUid(reg);
        registerAts(reg);
        registerCardInfo(reg);
        registerSendApdu(reg);
        // Mifare Classic
        registerLoadKey(reg);
        registerAuth(reg);
        registerRead(reg);
        registerWrite(reg);
        registerReadSector(reg);
        registerWriteSector(reg);
        registerReadTrailer(reg);
        registerMakeAccess(reg);
        // Katalog
        registerExplainSw(reg);
        registerExplainApdu(reg);
        registerListMacros(reg);
        registerExplainMacro(reg);
        // Şifreleme
        registerDetectCipher(reg);
        // Transaction
        registerBeginTransaction(reg);
        registerEndTransaction(reg);
    }

    // ── Helpers ──────────────────────────────────────────────────────────────

    // CLI CommandContext oluştur (MCP: promptFn yok — persistent bağlantı var)
    CommandContext makeCtx(const mcp::json& params,
                           const std::string& subcommand = "") {
        ParsedArgs args;
        args.subcommand = subcommand;
        // JSON params → ParsedArgs opts
        if (params.is_object()) {
            for (auto& [k, v] : params.items()) {
                if (v.is_string())
                    args.opts[k] = v.get<std::string>();
                else if (v.is_number_integer())
                    args.opts[k] = std::to_string(v.get<int>());
                else if (v.is_number_float())
                    args.opts[k] = std::to_string(static_cast<int>(v.get<double>()));
                else if (v.is_boolean()) {
                    if (v.get<bool>()) args.flags.insert(k);
                }
                else if (v.is_null()) {
                    // skip
                }
            }
        }
        return CommandContext{ app_, args, nullptr };
    }

    // Hata mesajıyla throw
    static void throwError(ExitCode ec, const std::string& op) {
        std::string msg = op + " failed";
        switch (ec) {
            case ExitCode::InvalidArgs:   msg += " (invalid arguments)"; break;
            case ExitCode::PcscError:     msg += " (PC/SC error)"; break;
            case ExitCode::CardCommError: msg += " (card communication error)"; break;
            default: break;
        }
        throw mcp::RpcError{ mcp::RpcError::INTERNAL_ERROR, msg };
    }

    mcp::json cmdResult(ExitCode ec, const std::string& errMsg = "") {
        if (ec != ExitCode::Success) {
            throw mcp::RpcError{ mcp::RpcError::INTERNAL_ERROR, errMsg };
        }
        return mcp::json::object();
    }

    // MCP tool sonucunu content array formatında döndür
    static mcp::json wrap(const mcp::json& j) {
        return {
            {"content", mcp::json::array({
                { {"type", "text"}, {"text", j.dump(2)} }
            })}
        };
    }

    // ── Tool registrations ────────────────────────────────────────────────────

    void registerListReaders(mcp::ToolRegistry& reg) {
        reg.registerTool(
            { "list_readers",
              "List all available PC/SC smart card readers.",
              { {"type","object"}, {"properties",mcp::json::object()},
                {"required", mcp::json::array()} }
            },
            [this](const mcp::json& params) -> mcp::json {
                // Capture stdout output by redirecting through json mode
                bool prevJson = app_.flags.jsonOutput;
                app_.flags.jsonOutput = true;
                app_.initFormatter();

                scardtool::cmds::ListReadersCmd cmd;
                auto ctx = makeCtx(params);
                auto ec = cmd.execute(ctx);

                app_.flags.jsonOutput = prevJson;
                app_.initFormatter();

                if (ec != ExitCode::Success) throwError(ec, "list_readers");

                mcp::json arr = mcp::json::array();
                for (size_t i = 0; i < app_.readers.size(); ++i)
                    arr.push_back({ {"index", i},
                                    {"name", App::wideToUtf8(app_.readers[i])} });

                return wrap({ {"readers", arr} });
            });
    }

    void registerConnect(mcp::ToolRegistry& reg) {
        reg.registerTool(
            { "connect",
              "Connect to a smart card on the specified reader.",
              { {"type","object"},
                {"properties", {
                    {"reader", { {"type","string"},
                                 {"description","Reader index or name"} }}
                }},
                {"required", mcp::json::array({"reader"})}
              }
            },
            [this](const mcp::json& params) -> mcp::json {
                scardtool::cmds::ConnectCmd cmd;
                auto ctx = makeCtx(params);
                auto ec  = cmd.execute(ctx);
                if (ec != ExitCode::Success) throwError(ec, "connect");
                return wrap({ {"connected", true},
                              {"protocol",  app_.pcsc.protocol() == SCARD_PROTOCOL_T0
                                            ? "T=0" : "T=1"} });
            });
    }

    void registerDisconnect(mcp::ToolRegistry& reg) {
        reg.registerTool(
            { "disconnect", "Disconnect from the current card.",
              { {"type","object"}, {"properties",mcp::json::object()},
                {"required", mcp::json::array()} }
            },
            [this](const mcp::json& params) -> mcp::json {
                scardtool::cmds::DisconnectCmd cmd;
                auto ctx = makeCtx(params);
                cmd.execute(ctx);
                return wrap({ {"disconnected", true} });
            });
    }

    void registerUid(mcp::ToolRegistry& reg) {
        reg.registerTool(
            { "uid", "Read the UID of the card (FF CA 00 00 00).",
              { {"type","object"},
                {"properties", { {"reader", { {"type","string"} }} }},
                {"required", mcp::json::array({"reader"})} }
            },
            [this](const mcp::json& params) -> mcp::json {
                scardtool::cmds::UidCmd cmd;
                auto ctx = makeCtx(params);
                // Basit doğrudan transmit
                auto readerVal = params.value("reader", "0");
                auto ec = scardtool::cmds::UidCmd::ensureConnected(app_, readerVal);
                if (ec != ExitCode::Success)
                    throw mcp::RpcError{ mcp::RpcError::INTERNAL_ERROR, "connect failed" };

                BYTEV apdu = {0xFF, 0xCA, 0x00, 0x00, 0x00};
                auto res = app_.pcsc.trySendCommand(apdu, true);
                if (!res.is_ok())
                    throw mcp::RpcError{ mcp::RpcError::INTERNAL_ERROR,
                                         res.error().message() };

                const BYTEV& resp = res.unwrap();
                if (resp.size() < 2)
                    throw mcp::RpcError{ mcp::RpcError::INTERNAL_ERROR, "Response too short" };

                BYTEV data(resp.begin(), resp.end() - 2);
                return wrap({ {"uid", mcp::toHex(data)} });
            });
    }

    void registerAts(mcp::ToolRegistry& reg) {
        reg.registerTool(
            { "ats", "Read ATS/historical bytes from the card (FF CA 01 00 00).",
              { {"type","object"},
                {"properties", { {"reader", { {"type","string"} }} }},
                {"required", mcp::json::array({"reader"})} }
            },
            [this](const mcp::json& params) -> mcp::json {
                auto readerVal = params.value("reader", "0");
                auto ec = scardtool::cmds::UidCmd::ensureConnected(app_, readerVal);
                if (ec != ExitCode::Success)
                    throw mcp::RpcError{ mcp::RpcError::INTERNAL_ERROR, "connect failed" };

                BYTEV apdu = {0xFF, 0xCA, 0x01, 0x00, 0x00};
                auto res = app_.pcsc.trySendCommand(apdu, true);
                if (!res.is_ok())
                    throw mcp::RpcError{ mcp::RpcError::INTERNAL_ERROR,
                                         res.error().message() };

                const BYTEV& resp = res.unwrap();
                BYTEV data(resp.begin(), resp.end() - 2);
                return wrap({ {"ats", mcp::toHex(data)} });
            });
    }

    void registerSendApdu(mcp::ToolRegistry& reg) {
        reg.registerTool(
            { "send_apdu",
              "Send a raw APDU to the card. Returns SW + response data.",
              { {"type","object"},
                {"properties", {
                    {"reader", { {"type","string"} }},
                    {"apdu",   { {"type","string"},
                                 {"description","Hex-encoded APDU"} }},
                    {"follow_chaining", { {"type","boolean"}, {"default",true} }}
                }},
                {"required", mcp::json::array({"reader","apdu"})} }
            },
            [this](const mcp::json& params) -> mcp::json {
                scardtool::cmds::SendApduCmd cmd;
                auto ctx = makeCtx(params);
                auto ec  = cmd.execute(ctx);
                if (ec != ExitCode::Success) throwError(ec, "send_apdu");
                return mcp::json::object();
            });
    }

    void registerBeginTransaction(mcp::ToolRegistry& reg) {
        reg.registerTool(
            { "begin_transaction",
              "Begin exclusive PC/SC transaction.",
              { {"type","object"},
                {"properties", { {"reader", { {"type","string"} }} }},
                {"required", mcp::json::array({"reader"})} }
            },
            [this](const mcp::json& params) -> mcp::json {
                scardtool::cmds::BeginTransactionCmd cmd;
                auto ctx = makeCtx(params);
                auto ec  = cmd.execute(ctx);
                if (ec != ExitCode::Success) throwError(ec, "begin_transaction");
                return wrap({ {"transaction", "begun"} });
            });
    }

    void registerEndTransaction(mcp::ToolRegistry& reg) {
        reg.registerTool(
            { "end_transaction",
              "End the current PC/SC transaction.",
              { {"type","object"},
                {"properties", {
                    {"disposition", { {"type","string"}, {"default","leave"} }}
                }},
                {"required", mcp::json::array()} }
            },
            [this](const mcp::json& params) -> mcp::json {
                scardtool::cmds::EndTransactionCmd cmd;
                auto ctx = makeCtx(params);
                auto ec  = cmd.execute(ctx);
                if (ec != ExitCode::Success)
                    throw mcp::RpcError{ mcp::RpcError::INTERNAL_ERROR, "end_transaction failed" };
                return wrap({ {"transaction", "ended"} });
            });
    }

    // ── Yeni tool implementasyonları ─────────────────────────────────────────

    // Ortak schema yardımcısı
    static mcp::json prop(const std::string& type, const std::string& desc = "") {
        mcp::json p = { {"type", type} };
        if (!desc.empty()) p["description"] = desc;
        return p;
    }

    void registerCardInfo(mcp::ToolRegistry& reg) {
        reg.registerTool({ "card_info",
            "Probe card type, UID, ATS, DESFire version, Ultralight version.",
            { {"type","object"}, {"properties", {
                {"reader",    prop("string","Reader index or name")},
                {"card_type", prop("string","Hint: classic1k|classic4k|ultralight|desfire")}
            }}, {"required", mcp::json::array({"reader"})} }},
            [this](const mcp::json& p) -> mcp::json {
                scardtool::cmds::CardInfoCmd cmd;
                app_.flags.jsonOutput = true; app_.initFormatter();
                auto ctx = makeCtx(p, "card-info"); auto ec = cmd.execute(ctx);
                if (ec != ExitCode::Success) throwError(ec, "card_info");
                return wrap(mcp::json::object());
            });
    }

    void registerLoadKey(mcp::ToolRegistry& reg) {
        reg.registerTool({ "load_key",
            "Load a Mifare Classic key into the reader key slot.",
            { {"type","object"}, {"properties", {
                {"reader",   prop("string","Reader index or name")},
                {"key",      prop("string","6-byte key as 12 hex chars (e.g. FFFFFFFFFFFF)")},
                {"slot",     prop("string","Key slot number (default: 01)")},
                {"volatile", prop("boolean","Use volatile session slot")}
            }}, {"required", mcp::json::array({"reader","key"})} }},
            [this](const mcp::json& p) -> mcp::json {
                scardtool::cmds::LoadKeyCmd cmd;
                auto ctx = makeCtx(p, "load-key"); auto ec = cmd.execute(ctx);
                if (ec != ExitCode::Success) throwError(ec, "load_key");
                return wrap({ {"loaded", true} });
            });
    }

    void registerAuth(mcp::ToolRegistry& reg) {
        reg.registerTool({ "auth",
            "Authenticate to a Mifare Classic block.",
            { {"type","object"}, {"properties", {
                {"reader",   prop("string","Reader index or name")},
                {"block",    prop("integer","Block number to authenticate")},
                {"key_type", prop("string","A or B (default: A)")},
                {"key_num",  prop("string","Key slot (default: 01)")},
                {"general",  prop("boolean","Use general authenticate format")}
            }}, {"required", mcp::json::array({"reader","block"})} }},
            [this](const mcp::json& p) -> mcp::json {
                scardtool::cmds::AuthCmd cmd;
                // Rename key_type → key-type for CLI compat
                auto args = p;
                if (p.contains("key_type")) args["key-type"] = p["key_type"];
                if (p.contains("key_num"))  args["key-num"]  = p["key_num"];
                auto ctx = makeCtx(args, "auth"); auto ec = cmd.execute(ctx);
                if (ec != ExitCode::Success) throwError(ec, "auth");
                return wrap({ {"authenticated", true} });
            });
    }

    void registerRead(mcp::ToolRegistry& reg) {
        reg.registerTool({ "read",
            "Read bytes from a card page/block (FF B0). Authenticate first for Classic.",
            { {"type","object"}, {"properties", {
                {"reader", prop("string","Reader index or name")},
                {"page",   prop("integer","Page/block number")},
                {"length", prop("integer","Bytes to read (default: 16)")}
            }}, {"required", mcp::json::array({"reader","page"})} }},
            [this](const mcp::json& p) -> mcp::json {
                scardtool::cmds::ReadCmd cmd;
                app_.flags.jsonOutput = true; app_.initFormatter();
                auto ctx = makeCtx(p, "read"); auto ec = cmd.execute(ctx);
                if (ec != ExitCode::Success) throwError(ec, "read");
                return wrap(mcp::json::object());
            });
    }

    void registerWrite(mcp::ToolRegistry& reg) {
        reg.registerTool({ "write",
            "Write bytes to a card page/block (FF D6). Authenticate first for Classic.",
            { {"type","object"}, {"properties", {
                {"reader", prop("string","Reader index or name")},
                {"page",   prop("integer","Page/block number")},
                {"data",   prop("string","Hex data to write")}
            }}, {"required", mcp::json::array({"reader","page","data"})} }},
            [this](const mcp::json& p) -> mcp::json {
                scardtool::cmds::WriteCmd cmd;
                auto ctx = makeCtx(p, "write"); auto ec = cmd.execute(ctx);
                if (ec != ExitCode::Success) throwError(ec, "write");
                return wrap({ {"written", true} });
            });
    }

    void registerReadSector(mcp::ToolRegistry& reg) {
        reg.registerTool({ "read_sector",
            "Authenticate and read all data blocks of a Mifare Classic sector.",
            { {"type","object"}, {"properties", {
                {"reader",    prop("string","Reader index or name")},
                {"sector",    prop("integer","Sector number (0-15 for 1K)")},
                {"key",       prop("string","6-byte key as 12 hex chars")},
                {"key_type",  prop("string","A or B (default: A)")},
                {"card_type", prop("string","classic1k|classic4k (default: classic1k)")}
            }}, {"required", mcp::json::array({"reader","sector","key"})} }},
            [this](const mcp::json& p) -> mcp::json {
                scardtool::cmds::ReadSectorCmd cmd;
                app_.flags.jsonOutput = true; app_.initFormatter();
                auto args = p;
                if (p.contains("key_type"))  args["key-type"]  = p["key_type"];
                if (p.contains("card_type")) args["card-type"] = p["card_type"];
                auto ctx = makeCtx(args, "read-sector"); auto ec = cmd.execute(ctx);
                if (ec != ExitCode::Success) throwError(ec, "read_sector");
                return wrap(mcp::json::object());
            });
    }

    void registerWriteSector(mcp::ToolRegistry& reg) {
        reg.registerTool({ "write_sector",
            "Authenticate and write data blocks of a Mifare Classic sector.",
            { {"type","object"}, {"properties", {
                {"reader",   prop("string","Reader index or name")},
                {"sector",   prop("integer","Sector number")},
                {"key",      prop("string","6-byte key as 12 hex chars")},
                {"data",     prop("string","Hex data (3 blocks × 16 bytes = 48 bytes for 1K)")},
                {"key_type", prop("string","A or B (default: A)")}
            }}, {"required", mcp::json::array({"reader","sector","key","data"})} }},
            [this](const mcp::json& p) -> mcp::json {
                scardtool::cmds::WriteSectorCmd cmd;
                auto args = p;
                if (p.contains("key_type")) args["key-type"] = p["key_type"];
                auto ctx = makeCtx(args, "write-sector"); auto ec = cmd.execute(ctx);
                if (ec != ExitCode::Success) throwError(ec, "write_sector");
                return wrap({ {"written", true} });
            });
    }

    void registerReadTrailer(mcp::ToolRegistry& reg) {
        reg.registerTool({ "read_trailer",
            "Read and parse the trailer block of a Mifare Classic sector.",
            { {"type","object"}, {"properties", {
                {"reader",   prop("string","Reader index or name")},
                {"sector",   prop("integer","Sector number")},
                {"key",      prop("string","6-byte key as 12 hex chars")},
                {"key_type", prop("string","A or B (default: A)")}
            }}, {"required", mcp::json::array({"reader","sector","key"})} }},
            [this](const mcp::json& p) -> mcp::json {
                scardtool::cmds::ReadTrailerCmd cmd;
                app_.flags.jsonOutput = true; app_.initFormatter();
                auto args = p;
                if (p.contains("key_type")) args["key-type"] = p["key_type"];
                auto ctx = makeCtx(args, "read-trailer"); auto ec = cmd.execute(ctx);
                if (ec != ExitCode::Success) throwError(ec, "read_trailer");
                return wrap(mcp::json::object());
            });
    }

    void registerMakeAccess(mcp::ToolRegistry& reg) {
        reg.registerTool({ "make_access",
            "Calculate Mifare Classic access bytes from condition indices (0-7).",
            { {"type","object"}, {"properties", {
                {"b0", prop("integer","Data block 0 condition (0-7, default: 0)")},
                {"b1", prop("integer","Data block 1 condition (0-7, default: 0)")},
                {"b2", prop("integer","Data block 2 condition (0-7, default: 0)")},
                {"bt", prop("integer","Trailer block condition (0-7, default: 1)")}
            }}, {"required", mcp::json::array()} }},
            [this](const mcp::json& p) -> mcp::json {
                scardtool::cmds::MakeAccessCmd cmd;
                app_.flags.jsonOutput = true; app_.initFormatter();
                auto ctx = makeCtx(p, "make-access"); auto ec = cmd.execute(ctx);
                if (ec != ExitCode::Success) throwError(ec, "make_access");
                return wrap(mcp::json::object());
            });
    }

    void registerExplainSw(mcp::ToolRegistry& reg) {
        reg.registerTool({ "explain_sw",
            "Explain a Status Word (SW1SW2) code from the ISO 7816-4 catalog.",
            { {"type","object"}, {"properties", {
                {"sw", prop("string","4-char hex SW code e.g. 6982 or 9000")}
            }}, {"required", mcp::json::array({"sw"})} }},
            [this](const mcp::json& p) -> mcp::json {
                scardtool::cmds::ExplainSwCmd cmd;
                app_.flags.jsonOutput = true; app_.initFormatter();
                ParsedArgs args; args.positional.push_back(p.value("sw",""));
                CommandContext ctx{app_, args, nullptr};
                auto ec = cmd.execute(ctx);
                if (ec != ExitCode::Success) throwError(ec, "explain_sw");
                return wrap(mcp::json::object());
            });
    }

    void registerExplainApdu(mcp::ToolRegistry& reg) {
        reg.registerTool({ "explain_apdu",
            "Parse and explain an APDU (hex or macro name like GETUID, READ_BINARY:4:16).",
            { {"type","object"}, {"properties", {
                {"apdu", prop("string","Hex APDU or macro name e.g. GETUID or FFCA000000")}
            }}, {"required", mcp::json::array({"apdu"})} }},
            [this](const mcp::json& p) -> mcp::json {
                scardtool::cmds::ExplainApduCmd cmd;
                app_.flags.jsonOutput = true; app_.initFormatter();
                ParsedArgs args; args.positional.push_back(p.value("apdu",""));
                CommandContext ctx{app_, args, nullptr};
                auto ec = cmd.execute(ctx);
                if (ec != ExitCode::Success) throwError(ec, "explain_apdu");
                return wrap(mcp::json::object());
            });
    }

    void registerListMacros(mcp::ToolRegistry& reg) {
        reg.registerTool({ "list_macros",
            "List available APDU macros. Filter by group: general|pcsc|desfire|ultralight|user.",
            { {"type","object"}, {"properties", {
                {"filter", prop("string","Group filter: general|pcsc|desfire|ultralight|user")}
            }}, {"required", mcp::json::array()} }},
            [this](const mcp::json& p) -> mcp::json {
                scardtool::cmds::ListMacrosCmd cmd;
                app_.flags.jsonOutput = true; app_.initFormatter();
                auto ctx = makeCtx(p, "list-macros");
                if (p.contains("filter")) ctx.args.opts["filter"] = p["filter"];
                auto ec = cmd.execute(ctx);
                if (ec != ExitCode::Success) throwError(ec, "list_macros");
                return wrap(mcp::json::object());
            });
    }

    void registerExplainMacro(mcp::ToolRegistry& reg) {
        reg.registerTool({ "explain_macro",
            "Show detailed documentation for a specific APDU macro.",
            { {"type","object"}, {"properties", {
                {"name", prop("string","Macro name e.g. GETUID, READ_BINARY, DESFIRE_SELECT")}
            }}, {"required", mcp::json::array({"name"})} }},
            [this](const mcp::json& p) -> mcp::json {
                scardtool::cmds::ExplainMacroCmd cmd;
                app_.flags.jsonOutput = true; app_.initFormatter();
                ParsedArgs args; args.positional.push_back(p.value("name",""));
                CommandContext ctx{app_, args, nullptr};
                auto ec = cmd.execute(ctx);
                if (ec != ExitCode::Success) throwError(ec, "explain_macro");
                return wrap(mcp::json::object());
            });
    }

    void registerDetectCipher(mcp::ToolRegistry& reg) {
        reg.registerTool({ "detect_cipher",
            "Probe card and recommend best encryption cipher.",
            { {"type","object"}, {"properties", {
                {"reader", prop("string","Reader index or name")}
            }}, {"required", mcp::json::array({"reader"})} }},
            [this](const mcp::json& p) -> mcp::json {
                scardtool::cmds::DetectCipherCmd cmd;
                app_.flags.jsonOutput = true; app_.initFormatter();
                auto ctx = makeCtx(p, "detect-cipher"); auto ec = cmd.execute(ctx);
                if (ec != ExitCode::Success) throwError(ec, "detect_cipher");
                return wrap(mcp::json::object());
            });
    }
};
