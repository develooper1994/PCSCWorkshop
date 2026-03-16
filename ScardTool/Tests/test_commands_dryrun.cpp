/**
 * @file test_commands_dryrun.cpp
 * @brief Komut birim testleri (dry-run modu — PCSC donanımı gerektirmez).
 *
 * Her komutun parametre doğrulama, hata döndürme ve dry-run davranışını test eder.
 */
#include <gtest/gtest.h>
#include <set>
#include "Commands/CommandRegistry.h"
#include "App/App.h"
#include "Crypto/CardCrypto.h"

// ── Test helper ───────────────────────────────────────────────────────────────

/**
 * @brief Dry-run modunda komut çalıştır.
 * @param subcmd Subcommand adı
 * @param extraArgs Ek argümanlar ({"--reader","0"} gibi)
 * @return ExitCode
 */
static ExitCode runDryRun(const std::string& subcmd,
                           std::vector<std::string> extraArgs = {}) {
    static CommandRegistry registry;
    static bool registered = false;

    App app;
    app.flags.dryRun     = true;
    app.flags.jsonOutput = false;
    app.flags.verbose    = false;
    app.initFormatter();

    auto* cmd = registry.find(subcmd);
    if (!cmd) return ExitCode::InvalidArgs;

    // Arg defs topla
    std::vector<ArgDef> allDefs = {
        {"json",    'j', false}, {"verbose",'v',false},
        {"dry-run", 0,   false}, {"session",'S',false},
    };
    for (auto& d : cmd->argDefs()) allDefs.push_back(d);

    // argv oluştur
    std::vector<std::string> argStrings = {"scardtool", subcmd};
    for (auto& a : extraArgs) argStrings.push_back(a);
    std::vector<char*> argv;
    for (auto& s : argStrings) argv.push_back(const_cast<char*>(s.c_str()));

    ParsedArgs parsed;
    try {
        parsed = ArgParser::parse(static_cast<int>(argv.size()),
                                  argv.data(), allDefs);
    } catch (...) {
        return ExitCode::InvalidArgs;
    }

    CommandContext ctx{app, parsed, nullptr};
    return cmd->execute(ctx);
}

// ── list-readers ──────────────────────────────────────────────────────────────

TEST(CmdDryRun, ListReaders_DryRun_Success) {
    EXPECT_EQ(runDryRun("list-readers"), ExitCode::Success);
}

TEST(CmdDryRun, ListReaders_WithJson_Success) {
    EXPECT_EQ(runDryRun("list-readers", {"-j"}), ExitCode::Success);
}

// ── connect ───────────────────────────────────────────────────────────────────

TEST(CmdDryRun, Connect_DryRun_WithReader) {
    EXPECT_EQ(runDryRun("connect", {"-r", "0"}), ExitCode::Success);
}

TEST(CmdDryRun, Connect_MissingReader_ReturnsInvalidArgs) {
    EXPECT_EQ(runDryRun("connect", {}), ExitCode::InvalidArgs);
}

// ── disconnect ────────────────────────────────────────────────────────────────

TEST(CmdDryRun, Disconnect_DryRun_Success) {
    EXPECT_EQ(runDryRun("disconnect"), ExitCode::Success);
}

// ── uid ───────────────────────────────────────────────────────────────────────

TEST(CmdDryRun, Uid_DryRun_WithReader) {
    EXPECT_EQ(runDryRun("uid", {"-r", "0"}), ExitCode::Success);
}

TEST(CmdDryRun, Uid_MissingReader_ReturnsInvalidArgs) {
    EXPECT_EQ(runDryRun("uid", {}), ExitCode::InvalidArgs);
}

// ── ats ───────────────────────────────────────────────────────────────────────

TEST(CmdDryRun, Ats_DryRun_WithReader) {
    EXPECT_EQ(runDryRun("ats", {"-r", "0"}), ExitCode::Success);
}

// ── card-info ─────────────────────────────────────────────────────────────────

TEST(CmdDryRun, CardInfo_DryRun_WithReader) {
    EXPECT_EQ(runDryRun("card-info", {"-r", "0"}), ExitCode::Success);
}

TEST(CmdDryRun, CardInfo_WithCardType) {
    EXPECT_EQ(runDryRun("card-info", {"-r","0","-c","classic1k"}), ExitCode::Success);
}

// ── send-apdu ─────────────────────────────────────────────────────────────────

TEST(CmdDryRun, SendApdu_DryRun_Success) {
    EXPECT_EQ(runDryRun("send-apdu", {"-r","0","-a","FFCA000000"}), ExitCode::Success);
}

TEST(CmdDryRun, SendApdu_MissingApdu_ReturnsInvalidArgs) {
    EXPECT_EQ(runDryRun("send-apdu", {"-r","0"}), ExitCode::InvalidArgs);
}

TEST(CmdDryRun, SendApdu_MissingReader_ReturnsInvalidArgs) {
    EXPECT_EQ(runDryRun("send-apdu", {"-a","FFCA000000"}), ExitCode::InvalidArgs);
}

TEST(CmdDryRun, SendApdu_InvalidHex_ReturnsInvalidArgs) {
    // dry-run olmasa da invalid hex → hata
    EXPECT_EQ(runDryRun("send-apdu", {"-r","0","-a","ZZZZ"}), ExitCode::InvalidArgs);
}

// ── load-key ──────────────────────────────────────────────────────────────────

TEST(CmdDryRun, LoadKey_DryRun_Success) {
    EXPECT_EQ(runDryRun("load-key", {"-r","0","-k","FFFFFFFFFFFF"}), ExitCode::Success);
}

TEST(CmdDryRun, LoadKey_InvalidKey_TooShort) {
    EXPECT_EQ(runDryRun("load-key", {"-r","0","-k","FFFF"}), ExitCode::InvalidArgs);
}

TEST(CmdDryRun, LoadKey_InvalidKey_BadChars) {
    EXPECT_EQ(runDryRun("load-key", {"-r","0","-k","GGGGGGGGGGGG"}), ExitCode::InvalidArgs);
}

TEST(CmdDryRun, LoadKey_WithVolatile) {
    EXPECT_EQ(runDryRun("load-key", {"-r","0","-k","FFFFFFFFFFFF","-L"}), ExitCode::Success);
}

TEST(CmdDryRun, LoadKey_MissingKey) {
    EXPECT_EQ(runDryRun("load-key", {"-r","0"}), ExitCode::InvalidArgs);
}

// ── auth ──────────────────────────────────────────────────────────────────────

TEST(CmdDryRun, Auth_DryRun_Success) {
    EXPECT_EQ(runDryRun("auth", {"-r","0","-b","4"}), ExitCode::Success);
}

TEST(CmdDryRun, Auth_WithKeyTypeB) {
    EXPECT_EQ(runDryRun("auth", {"-r","0","-b","4","-t","B"}), ExitCode::Success);
}

TEST(CmdDryRun, Auth_WithGeneral) {
    EXPECT_EQ(runDryRun("auth", {"-r","0","-b","4","-G"}), ExitCode::Success);
}

TEST(CmdDryRun, Auth_InvalidKeyType) {
    EXPECT_EQ(runDryRun("auth", {"-r","0","-b","4","-t","C"}), ExitCode::InvalidArgs);
}

TEST(CmdDryRun, Auth_MissingBlock) {
    EXPECT_EQ(runDryRun("auth", {"-r","0"}), ExitCode::InvalidArgs);
}

// ── read ──────────────────────────────────────────────────────────────────────

TEST(CmdDryRun, Read_DryRun_Success) {
    EXPECT_EQ(runDryRun("read", {"-r","0","-p","4"}), ExitCode::Success);
}

TEST(CmdDryRun, Read_WithLength) {
    EXPECT_EQ(runDryRun("read", {"-r","0","-p","4","-l","16"}), ExitCode::Success);
}

TEST(CmdDryRun, Read_MissingPage) {
    EXPECT_EQ(runDryRun("read", {"-r","0"}), ExitCode::InvalidArgs);
}

TEST(CmdDryRun, Read_InvalidLength_Zero) {
    EXPECT_EQ(runDryRun("read", {"-r","0","-p","4","-l","0"}), ExitCode::InvalidArgs);
}

// ── write ─────────────────────────────────────────────────────────────────────

TEST(CmdDryRun, Write_DryRun_Success) {
    EXPECT_EQ(runDryRun("write", {"-r","0","-p","4","-d","00112233445566778899AABBCCDDEEFF"}),
              ExitCode::Success);
}

TEST(CmdDryRun, Write_InvalidHex) {
    EXPECT_EQ(runDryRun("write", {"-r","0","-p","4","-d","ZZZZ"}), ExitCode::InvalidArgs);
}

TEST(CmdDryRun, Write_MissingData) {
    EXPECT_EQ(runDryRun("write", {"-r","0","-p","4"}), ExitCode::InvalidArgs);
}

// ── read-sector ───────────────────────────────────────────────────────────────

TEST(CmdDryRun, ReadSector_DryRun_Success) {
    EXPECT_EQ(runDryRun("read-sector", {"-r","0","-s","0","-k","FFFFFFFFFFFF"}),
              ExitCode::Success);
}

TEST(CmdDryRun, ReadSector_InvalidSector) {
    EXPECT_EQ(runDryRun("read-sector", {"-r","0","-s","99","-k","FFFFFFFFFFFF","-c","classic1k"}),
              ExitCode::InvalidArgs);
}

TEST(CmdDryRun, ReadSector_InvalidCardType) {
    EXPECT_EQ(runDryRun("read-sector", {"-r","0","-s","0","-k","FFFFFFFFFFFF","-c","invalid"}),
              ExitCode::InvalidArgs);
}

// ── write-sector ──────────────────────────────────────────────────────────────

TEST(CmdDryRun, WriteSector_DryRun_Success) {
    // Sector 0 = 3 data blocks × 16 byte = 48 byte
    std::string data48 = std::string(96, '0'); // 48 bytes = 96 hex chars
    EXPECT_EQ(runDryRun("write-sector", {"-r","0","-s","0","-k","FFFFFFFFFFFF","-d",data48}),
              ExitCode::Success);
}

TEST(CmdDryRun, WriteSector_WrongDataLength) {
    EXPECT_EQ(runDryRun("write-sector", {"-r","0","-s","0","-k","FFFFFFFFFFFF",
                                          "-d","00112233"}), // çok kısa
              ExitCode::InvalidArgs);
}

// ── read-trailer ──────────────────────────────────────────────────────────────

TEST(CmdDryRun, ReadTrailer_DryRun_Success) {
    EXPECT_EQ(runDryRun("read-trailer", {"-r","0","-s","0","-k","FFFFFFFFFFFF"}),
              ExitCode::Success);
}

// ── write-trailer ─────────────────────────────────────────────────────────────

TEST(CmdDryRun, WriteTrailer_DryRun_ValidAccess) {
    // FF0F0000 = transport default (valid access bytes)
    EXPECT_EQ(runDryRun("write-trailer", {
        "-r","0","-s","0",
        "--key-a","FFFFFFFFFFFF",
        "--key-b","FFFFFFFFFFFF",
        "--access","FF0F0000"
    }), ExitCode::Success);
}

TEST(CmdDryRun, WriteTrailer_InvalidAccessBytes_IntegrityFail) {
    EXPECT_EQ(runDryRun("write-trailer", {
        "-r","0","-s","0",
        "--key-a","FFFFFFFFFFFF",
        "--key-b","FFFFFFFFFFFF",
        "--access","DEADBEEF"
    }), ExitCode::InvalidArgs);
}

TEST(CmdDryRun, WriteTrailer_InvalidAccessBytes_WrongLength) {
    EXPECT_EQ(runDryRun("write-trailer", {
        "-r","0","-s","0",
        "--key-a","FFFFFFFFFFFF",
        "--key-b","FFFFFFFFFFFF",
        "--access","FF07"
    }), ExitCode::InvalidArgs);
}

// ── make-access ───────────────────────────────────────────────────────────────

TEST(CmdDryRun, MakeAccess_Default) {
    EXPECT_EQ(runDryRun("make-access", {}), ExitCode::Success);
}

TEST(CmdDryRun, MakeAccess_AllZero) {
    EXPECT_EQ(runDryRun("make-access", {"--b0","0","--b1","0","--b2","0","--bt","0"}),
              ExitCode::Success);
}

TEST(CmdDryRun, MakeAccess_AllSeven) {
    EXPECT_EQ(runDryRun("make-access", {"--b0","7","--b1","7","--b2","7","--bt","7"}),
              ExitCode::Success);
}

TEST(CmdDryRun, MakeAccess_Table) {
    EXPECT_EQ(runDryRun("make-access", {"--table"}), ExitCode::Success);
}

// ── begin-transaction / end-transaction ──────────────────────────────────────

TEST(CmdDryRun, BeginTransaction_DryRun) {
    EXPECT_EQ(runDryRun("begin-transaction", {"-r","0"}), ExitCode::Success);
}

TEST(CmdDryRun, EndTransaction_DryRun) {
    EXPECT_EQ(runDryRun("end-transaction", {}), ExitCode::Success);
}

TEST(CmdDryRun, EndTransaction_WithDisposition) {
    EXPECT_EQ(runDryRun("end-transaction", {"-d","reset"}), ExitCode::Success);
}

TEST(CmdDryRun, EndTransaction_InvalidDisposition) {
    // invalid → parse hatasız ama execute'da hata
    EXPECT_EQ(runDryRun("end-transaction", {"-d","badval"}), ExitCode::InvalidArgs);
}

// ── CommandRegistry ───────────────────────────────────────────────────────────

TEST(CommandRegistry, FindKnownCommands) {
    CommandRegistry reg;
    EXPECT_NE(reg.find("list-readers"),     nullptr);
    EXPECT_NE(reg.find("connect"),          nullptr);
    EXPECT_NE(reg.find("disconnect"),       nullptr);
    EXPECT_NE(reg.find("uid"),              nullptr);
    EXPECT_NE(reg.find("ats"),              nullptr);
    EXPECT_NE(reg.find("card-info"),        nullptr);
    EXPECT_NE(reg.find("send-apdu"),        nullptr);
    EXPECT_NE(reg.find("load-key"),         nullptr);
    EXPECT_NE(reg.find("auth"),             nullptr);
    EXPECT_NE(reg.find("read"),             nullptr);
    EXPECT_NE(reg.find("write"),            nullptr);
    EXPECT_NE(reg.find("read-sector"),      nullptr);
    EXPECT_NE(reg.find("write-sector"),     nullptr);
    EXPECT_NE(reg.find("read-trailer"),     nullptr);
    EXPECT_NE(reg.find("write-trailer"),    nullptr);
    EXPECT_NE(reg.find("make-access"),      nullptr);
    EXPECT_NE(reg.find("begin-transaction"),nullptr);
    EXPECT_NE(reg.find("end-transaction"),  nullptr);
}

TEST(CommandRegistry, FindUnknown_ReturnsNull) {
    CommandRegistry reg;
    EXPECT_EQ(reg.find("nonexistent"), nullptr);
    EXPECT_EQ(reg.find(""),            nullptr);
}

TEST(CommandRegistry, NamesContainsAllCommands) {
    CommandRegistry reg;
    auto names = reg.names();
    EXPECT_GE(names.size(), 15u);
    EXPECT_NE(std::find(names.begin(), names.end(), "list-readers"), names.end());
    EXPECT_NE(std::find(names.begin(), names.end(), "send-apdu"),    names.end());
    EXPECT_NE(std::find(names.begin(), names.end(), "make-access"),  names.end());
}

// ── detect-cipher ─────────────────────────────────────────────────────────────

TEST(CmdDryRun, DetectCipher_DryRun_Success) {
    EXPECT_EQ(runDryRun("detect-cipher", {"-r","0"}), ExitCode::Success);
}

TEST(CmdDryRun, DetectCipher_MissingReader) {
    EXPECT_EQ(runDryRun("detect-cipher", {}), ExitCode::InvalidArgs);
}

// ── CommandRegistry — yeni komutlar da kayitli ───────────────────────────────

TEST(CommandRegistry, FindCatalogAndCryptoCommands) {
    CommandRegistry reg;
    EXPECT_NE(reg.find("explain-sw"),    nullptr);
    EXPECT_NE(reg.find("explain-apdu"),  nullptr);
    EXPECT_NE(reg.find("list-macros"),   nullptr);
    EXPECT_NE(reg.find("explain-macro"), nullptr);
    EXPECT_NE(reg.find("detect-cipher"), nullptr);
}

TEST(CommandRegistry, NoNameConflicts) {
    CommandRegistry reg;
    auto names = reg.names();
    std::set<std::string> seen;
    for (const auto& n : names) {
        EXPECT_EQ(seen.count(n), 0u) << "Duplicate command name: " << n;
        seen.insert(n);
    }
}

// ── Şifreleme flag'leri — dry-run doğrulaması ─────────────────────────────────

TEST(CmdDryRun, Write_Encrypted_DryRun) {
    App app;
    app.flags.dryRun  = true;
    app.flags.encrypt = true;
    app.flags.cipher  = CipherAlgo::AesCtr;
    app.flags.password = "testpass";
    app.initFormatter();

    CommandRegistry reg;
    auto* cmd = reg.find("write");
    ASSERT_NE(cmd, nullptr);

    std::vector<ArgDef> defs = {
        {"reader",'r',true}, {"page",'p',true}, {"data",'d',true}
    };
    std::vector<std::string> argStrs = {
        "scardtool","write","-r","0","-p","4",
        "-d","00112233445566778899AABBCCDDEEFF"
    };
    std::vector<char*> argv;
    for (auto& s : argStrs) argv.push_back(const_cast<char*>(s.c_str()));
    auto parsed = ArgParser::parse(static_cast<int>(argv.size()), argv.data(), defs);
    CommandContext ctx{app, parsed, nullptr};
    EXPECT_EQ(cmd->execute(ctx), ExitCode::Success);
}

TEST(CmdDryRun, Write_Trailer_Rejected) {
    App app;
    app.flags.dryRun   = true;
    app.flags.encrypt  = true;
    app.flags.cipher   = CipherAlgo::AesCtr;
    app.flags.password = "testpass";
    app.initFormatter();

    CommandRegistry reg;
    auto* cmd = reg.find("write");
    ASSERT_NE(cmd, nullptr);

    // Block 3 = trailer → reddedilmeli
    std::vector<ArgDef> defs = {
        {"reader",'r',true}, {"page",'p',true}, {"data",'d',true}
    };
    std::vector<std::string> argStrs = {
        "scardtool","write","-r","0","-p","3",
        "-d","00112233445566778899AABBCCDDEEFF"
    };
    std::vector<char*> argv;
    for (auto& s : argStrs) argv.push_back(const_cast<char*>(s.c_str()));
    auto parsed = ArgParser::parse(static_cast<int>(argv.size()), argv.data(), defs);
    CommandContext ctx{app, parsed, nullptr};
    EXPECT_EQ(cmd->execute(ctx), ExitCode::InvalidArgs);
}
