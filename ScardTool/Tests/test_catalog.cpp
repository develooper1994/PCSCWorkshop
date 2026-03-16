/**
 * @file test_catalog.cpp
 * @brief StatusWordCatalog, InsCatalog ve MacroRegistry birim testleri.
 */
#include <gtest/gtest.h>
#include "Catalog/ApduCatalog.h"

// ── StatusWordCatalog ─────────────────────────────────────────────────────────

TEST(StatusWordCatalog, Lookup_9000_Success) {
    auto e = StatusWordCatalog::lookup(0x90, 0x00);
    ASSERT_TRUE(e.has_value());
    EXPECT_EQ(e->code, "9000");
    EXPECT_EQ(e->category, "Success");
}

TEST(StatusWordCatalog, Lookup_6982_Security) {
    auto e = StatusWordCatalog::lookup(0x69, 0x82);
    ASSERT_TRUE(e.has_value());
    EXPECT_EQ(e->code, "6982");
    EXPECT_EQ(e->category, "Security");
    EXPECT_FALSE(e->resolution.empty());  // çözüm önerisi olmalı
}

TEST(StatusWordCatalog, Lookup_6983_AuthBlocked) {
    auto e = StatusWordCatalog::lookup(0x69, 0x83);
    ASSERT_TRUE(e.has_value());
    EXPECT_NE(e->name.find("blocked"), std::string::npos);
}

TEST(StatusWordCatalog, Lookup_6300_AuthSentinel) {
    auto e = StatusWordCatalog::lookup(0x63, 0x00);
    ASSERT_TRUE(e.has_value());
    EXPECT_EQ(e->sw1, 0x63);
}

TEST(StatusWordCatalog, Lookup_Wildcard_61xx) {
    // 61 03 → wildcard 61 FF ile eşleşmeli
    auto e = StatusWordCatalog::lookup(0x61, 0x03);
    ASSERT_TRUE(e.has_value());
    EXPECT_EQ(e->sw1, 0x61);
    EXPECT_EQ(e->sw2, SW2_ANY);
}

TEST(StatusWordCatalog, Lookup_Wildcard_6Cxx) {
    auto e = StatusWordCatalog::lookup(0x6C, 0x10);
    ASSERT_TRUE(e.has_value());
    EXPECT_EQ(e->sw1, 0x6C);
}

TEST(StatusWordCatalog, Lookup_63Cx_VerifyFail) {
    auto e = StatusWordCatalog::lookup(0x63, 0xC2);
    ASSERT_TRUE(e.has_value());
    EXPECT_EQ(e->sw1, 0x63);
}

TEST(StatusWordCatalog, Lookup_6F00_NoPreciseDiagnosis) {
    auto e = StatusWordCatalog::lookup(0x6F, 0x00);
    ASSERT_TRUE(e.has_value());
    EXPECT_EQ(e->category, "Unknown");
}

TEST(StatusWordCatalog, Lookup_Unknown_ReturnsNullopt) {
    auto e = StatusWordCatalog::lookup(0x00, 0x00);
    EXPECT_FALSE(e.has_value());
}

TEST(StatusWordCatalog, Lookup_91AF_DESFire) {
    auto e = StatusWordCatalog::lookup(0x91, 0xAF);
    ASSERT_TRUE(e.has_value());
    EXPECT_EQ(e->category, "DESFire");
}

TEST(StatusWordCatalog, Lookup_9100_DESFireOK) {
    auto e = StatusWordCatalog::lookup(0x91, 0x00);
    ASSERT_TRUE(e.has_value());
    EXPECT_EQ(e->category, "DESFire");
}

TEST(StatusWordCatalog, LookupByString_6982) {
    auto e = StatusWordCatalog::lookup("6982");
    ASSERT_TRUE(e.has_value());
    EXPECT_EQ(e->code, "6982");
}

TEST(StatusWordCatalog, LookupByString_InvalidLength) {
    EXPECT_FALSE(StatusWordCatalog::lookup("69").has_value());
    EXPECT_FALSE(StatusWordCatalog::lookup("69820000").has_value());
}

TEST(StatusWordCatalog, IsSuccess_9000) {
    EXPECT_TRUE(StatusWordCatalog::isSuccess(0x90, 0x00));
}

TEST(StatusWordCatalog, IsSuccess_9100) {
    EXPECT_TRUE(StatusWordCatalog::isSuccess(0x91, 0x00));
}

TEST(StatusWordCatalog, IsSuccess_6982_False) {
    EXPECT_FALSE(StatusWordCatalog::isSuccess(0x69, 0x82));
}

TEST(StatusWordCatalog, ShortDesc_9000) {
    EXPECT_FALSE(StatusWordCatalog::shortDesc(0x90, 0x00).empty());
}

TEST(StatusWordCatalog, ShortDesc_61xx_SubstitutesValue) {
    std::string d = StatusWordCatalog::shortDesc(0x61, 0x0F);
    // 0F değeri çıktıda görünmeli (append veya replace)
    EXPECT_NE(d.find("0F"), std::string::npos) << "Got: " << d;
    EXPECT_EQ(d.find("xx"), std::string::npos); // raw "xx" olmamalı
}

TEST(StatusWordCatalog, TableNotEmpty) {
    EXPECT_GT(StatusWordCatalog::table().size(), 40u);
}

TEST(StatusWordCatalog, AllEntriesHaveNonEmptyFields) {
    for (const auto& e : StatusWordCatalog::table()) {
        EXPECT_FALSE(e.code.empty())     << "empty code";
        EXPECT_FALSE(e.name.empty())     << "empty name: " << e.code;
        EXPECT_FALSE(e.category.empty()) << "empty category: " << e.code;
    }
}

// ── InsCatalog ────────────────────────────────────────────────────────────────

TEST(InsCatalog, Lookup_ISO_SELECT) {
    auto e = InsCatalog::lookup(0x00, 0xA4);
    ASSERT_TRUE(e.has_value());
    EXPECT_EQ(e->group, "ISO");
    EXPECT_NE(e->name.find("SELECT"), std::string::npos);
}

TEST(InsCatalog, Lookup_ISO_ReadBinary) {
    auto e = InsCatalog::lookup(0x00, 0xB0);
    ASSERT_TRUE(e.has_value());
    EXPECT_NE(e->name.find("READ"), std::string::npos);
}

TEST(InsCatalog, Lookup_PCSC_GetUID) {
    auto e = InsCatalog::lookup(0xFF, 0xCA);
    ASSERT_TRUE(e.has_value());
    EXPECT_EQ(e->group, "PCSC");
    EXPECT_NE(e->name.find("GET DATA"), std::string::npos);
}

TEST(InsCatalog, Lookup_PCSC_LoadKey) {
    auto e = InsCatalog::lookup(0xFF, 0x82);
    ASSERT_TRUE(e.has_value());
    EXPECT_EQ(e->group, "PCSC");
}

TEST(InsCatalog, Lookup_PCSC_ReadBinary) {
    auto e = InsCatalog::lookup(0xFF, 0xB0);
    ASSERT_TRUE(e.has_value());
    EXPECT_EQ(e->group, "PCSC");
}

TEST(InsCatalog, Lookup_DESFire_GetVersion) {
    auto e = InsCatalog::lookup(0x90, 0x60);
    ASSERT_TRUE(e.has_value());
    EXPECT_EQ(e->group, "DESFire");
    EXPECT_NE(e->name.find("VERSION"), std::string::npos);
}

TEST(InsCatalog, Lookup_DESFire_Select) {
    auto e = InsCatalog::lookup(0x90, 0x5A);
    ASSERT_TRUE(e.has_value());
    EXPECT_EQ(e->group, "DESFire");
}

TEST(InsCatalog, Lookup_DESFire_Format) {
    auto e = InsCatalog::lookup(0x90, 0xFC);
    ASSERT_TRUE(e.has_value());
    EXPECT_EQ(e->group, "DESFire");
}

TEST(InsCatalog, Lookup_Unknown_ReturnsNullopt) {
    EXPECT_FALSE(InsCatalog::lookup(0x00, 0xEE).has_value());
}

TEST(InsCatalog, ByGroup_ISO_NotEmpty) {
    EXPECT_GT(InsCatalog::byGroup("ISO").size(), 10u);
}

TEST(InsCatalog, ByGroup_PCSC_NotEmpty) {
    EXPECT_GT(InsCatalog::byGroup("PCSC").size(), 5u);
}

TEST(InsCatalog, ByGroup_DESFire_NotEmpty) {
    EXPECT_GT(InsCatalog::byGroup("DESFire").size(), 10u);
}

TEST(InsCatalog, TableNotEmpty) {
    EXPECT_GT(InsCatalog::table().size(), 50u);
}

// ── ApduMacro ─────────────────────────────────────────────────────────────────

TEST(ApduMacro, ResolveFixed_GETUID) {
    ApduMacro m;
    m.name         = "GETUID";
    m.apduTemplate = "FF CA 00 00 00";
    m.params       = {};
    EXPECT_EQ(m.resolveFixed(), "FFCA000000");
}

TEST(ApduMacro, ResolveFixed_WithParams_Throws) {
    ApduMacro m;
    m.name         = "READ_BINARY";
    m.apduTemplate = "FF B0 00 {page} {len}";
    m.params       = {"page", "len"};
    EXPECT_THROW(m.resolveFixed(), std::invalid_argument);
}

TEST(ApduMacro, Resolve_SingleDecimalParam) {
    ApduMacro m;
    m.name         = "AUTH_A";
    m.apduTemplate = "FF 88 00 {block} 60 01";
    m.params       = {"block"};
    auto r4  = m.resolve({"4"});
    auto r04 = m.resolve({"04"});
    EXPECT_EQ(r4, r04);
    EXPECT_NE(r4.find("04"), std::string::npos);
}

TEST(ApduMacro, Resolve_MultibyteKey) {
    ApduMacro m;
    m.name         = "LOAD_KEY";
    m.apduTemplate = "FF 82 20 01 06 {key}";
    m.params       = {"key"};
    auto upper = m.resolve({"FFFFFFFFFFFF"});
    auto lower = m.resolve({"ffffffffffff"});
    EXPECT_EQ(upper, lower);
    EXPECT_NE(upper.find("FFFFFFFFFFFF"), std::string::npos);
}

TEST(ApduMacro, Resolve_WrongParamCount_Throws) {
    ApduMacro m;
    m.name         = "READ_BINARY";
    m.apduTemplate = "FF B0 00 {page} {len}";
    m.params       = {"page", "len"};
    EXPECT_THROW(m.resolve({"4"}), std::invalid_argument);          // too few
    EXPECT_THROW(m.resolve({"4","16","99"}), std::invalid_argument); // too many
}

TEST(ApduMacro, Usage_NoParams) {
    ApduMacro m; m.name = "GETUID"; m.params = {};
    EXPECT_EQ(m.usage(), "GETUID");
}

TEST(ApduMacro, Usage_WithParams) {
    ApduMacro m; m.name = "READ_BINARY"; m.params = {"page","len"};
    EXPECT_EQ(m.usage(), "READ_BINARY:<page>:<len>");
}

// ── parseMacroCall ────────────────────────────────────────────────────────────

TEST(ParseMacroCall, NoParams) {
    std::string name; std::vector<std::string> args;
    parseMacroCall("GETUID", name, args);
    EXPECT_EQ(name, "GETUID");
    EXPECT_TRUE(args.empty());
}

TEST(ParseMacroCall, OneParam) {
    std::string name; std::vector<std::string> args;
    parseMacroCall("AUTH_A:4", name, args);
    EXPECT_EQ(name, "AUTH_A");
    ASSERT_EQ(args.size(), 1u);
    EXPECT_EQ(args[0], "4");
}

TEST(ParseMacroCall, TwoParams) {
    std::string name; std::vector<std::string> args;
    parseMacroCall("READ_BINARY:4:16", name, args);
    EXPECT_EQ(name, "READ_BINARY");
    ASSERT_EQ(args.size(), 2u);
    EXPECT_EQ(args[0], "4");
    EXPECT_EQ(args[1], "16");
}

TEST(ParseMacroCall, ThreeParams) {
    std::string name; std::vector<std::string> args;
    parseMacroCall("DESFIRE_READDATA:01:000000:000010", name, args);
    EXPECT_EQ(name, "DESFIRE_READDATA");
    ASSERT_EQ(args.size(), 3u);
}

// ── MacroRegistry ─────────────────────────────────────────────────────────────

TEST(MacroRegistry, FindGETUID) {
    auto m = MacroRegistry::instance().find("GETUID");
    ASSERT_TRUE(m.has_value());
    EXPECT_EQ(m->name, "GETUID");
    EXPECT_TRUE(m->params.empty());
}

TEST(MacroRegistry, FindCaseInsensitive) {
    EXPECT_TRUE(MacroRegistry::instance().find("getuid").has_value());
    EXPECT_TRUE(MacroRegistry::instance().find("Getuid").has_value());
    EXPECT_TRUE(MacroRegistry::instance().find("GETUID").has_value());
}

TEST(MacroRegistry, FindREAD_BINARY) {
    auto m = MacroRegistry::instance().find("READ_BINARY");
    ASSERT_TRUE(m.has_value());
    EXPECT_EQ(m->params.size(), 2u);
}

TEST(MacroRegistry, FindDESFIRE_SELECT) {
    auto m = MacroRegistry::instance().find("DESFIRE_SELECT");
    ASSERT_TRUE(m.has_value());
    EXPECT_EQ(m->params.size(), 1u);
}

TEST(MacroRegistry, FindUnknown_ReturnsNullopt) {
    EXPECT_FALSE(MacroRegistry::instance().find("NONEXISTENT_XYZ").has_value());
}

TEST(MacroRegistry, Resolve_GETUID) {
    std::string hex;
    EXPECT_TRUE(MacroRegistry::instance().resolve("GETUID", hex));
    EXPECT_EQ(hex, "FFCA000000");
}

TEST(MacroRegistry, Resolve_GETATS) {
    std::string hex;
    EXPECT_TRUE(MacroRegistry::instance().resolve("GETATS", hex));
    EXPECT_EQ(hex, "FFCA010000");
}

TEST(MacroRegistry, Resolve_READ_BINARY_WithParams) {
    std::string hex;
    EXPECT_TRUE(MacroRegistry::instance().resolve("READ_BINARY:4:16", hex));
    EXPECT_EQ(hex, "FFB0000410");
}

TEST(MacroRegistry, Resolve_AUTH_A_WithBlock) {
    std::string hex;
    EXPECT_TRUE(MacroRegistry::instance().resolve("AUTH_A:4", hex));
    // 6 bytes: FF 88 00 04 60 01
    EXPECT_EQ(hex.size(), 12u);
    EXPECT_EQ(hex.substr(0,6), "FF8800");
    EXPECT_EQ(hex.substr(6,2), "04");  // block=4
    EXPECT_EQ(hex.substr(8),   "6001");
}

TEST(MacroRegistry, Resolve_LOAD_KEY_Passthrough) {
    std::string hex;
    EXPECT_TRUE(MacroRegistry::instance().resolve("LOAD_KEY:FFFFFFFFFFFF", hex));
    EXPECT_NE(hex.find("FFFFFFFFFFFF"), std::string::npos);
    EXPECT_NE(hex.find("FF82"), std::string::npos);
}

TEST(MacroRegistry, Resolve_HexPassthrough) {
    std::string hex;
    EXPECT_TRUE(MacroRegistry::instance().resolve("FFCA000000", hex));
    EXPECT_EQ(hex, "FFCA000000");
}

TEST(MacroRegistry, Resolve_HexWithSpaces_Passthrough) {
    std::string hex;
    EXPECT_TRUE(MacroRegistry::instance().resolve("FF CA 00 00 00", hex));
    EXPECT_EQ(hex, "FFCA000000");
}

TEST(MacroRegistry, Resolve_UnknownMacro_ReturnsFalse) {
    std::string hex;
    EXPECT_FALSE(MacroRegistry::instance().resolve("NONEXISTENT_XYZ", hex));
}

TEST(MacroRegistry, Resolve_DESFIRE_FORMAT_Warning) {
    // Tehlikeli makro var mı?
    auto m = MacroRegistry::instance().find("DESFIRE_FORMAT");
    ASSERT_TRUE(m.has_value());
    // Açıklama uyarı içermeli
    EXPECT_NE(m->description.find("wipe"), std::string::npos);
}

TEST(MacroRegistry, AllBuiltinsHaveNonEmptyFields) {
    for (const auto& m : MacroRegistry::instance().all()) {
        if (m.group == "User") continue;
        EXPECT_FALSE(m.name.empty())        << "empty name";
        EXPECT_FALSE(m.description.empty()) << "empty desc: " << m.name;
        EXPECT_FALSE(m.group.empty())       << "empty group: " << m.name;
    }
}

TEST(MacroRegistry, BuiltinCount) {
    int builtIn = 0;
    for (const auto& m : MacroRegistry::instance().all())
        if (m.group != "User") ++builtIn;
    EXPECT_GE(builtIn, 40);
}

TEST(MacroRegistry, ByGroup_PCSC) {
    auto pcsc = MacroRegistry::instance().byGroup("PCSC");
    EXPECT_GT(pcsc.size(), 5u);
}

TEST(MacroRegistry, ByGroup_DESFire) {
    auto df = MacroRegistry::instance().byGroup("DESFire");
    EXPECT_GT(df.size(), 10u);
}

TEST(MacroRegistry, ByGroup_Empty_ReturnsAll) {
    auto all = MacroRegistry::instance().byGroup("");
    EXPECT_GT(all.size(), 40u);
}

// ── isHexApdu ─────────────────────────────────────────────────────────────────

TEST(IsHexApdu, ValidHex) {
    EXPECT_TRUE(isHexApdu("FFCA000000"));
    EXPECT_TRUE(isHexApdu("9000"));
    EXPECT_TRUE(isHexApdu("00A4040007D276000085010000"));
}

TEST(IsHexApdu, WithSpaces_Valid) {
    // spaces are allowed
    EXPECT_TRUE(isHexApdu("FF CA 00 00 00"));
}

TEST(IsHexApdu, OddLength_Invalid) {
    EXPECT_FALSE(isHexApdu("FCA"));
}

TEST(IsHexApdu, InvalidChars) {
    EXPECT_FALSE(isHexApdu("GETUID"));
    EXPECT_FALSE(isHexApdu("GGGG"));
}

TEST(IsHexApdu, Empty) {
    EXPECT_FALSE(isHexApdu(""));
}

// ── describeApdu + describeSw ─────────────────────────────────────────────────

TEST(ApduCatalog, DescribeApdu_GETUID) {
    std::string d = describeApdu("FFCA000000");
    EXPECT_NE(d.find("FF"), std::string::npos);
    EXPECT_NE(d.find("CA"), std::string::npos);
    EXPECT_NE(d.find("GET DATA"), std::string::npos);
}

TEST(ApduCatalog, DescribeApdu_TooShort) {
    std::string d = describeApdu("FF");
    EXPECT_NE(d.find("Too short"), std::string::npos);
}

TEST(ApduCatalog, DescribeSw_9000) {
    std::string d = describeSw(0x90, 0x00);
    EXPECT_NE(d.find("9000"), std::string::npos);
    EXPECT_NE(d.find("Normal"), std::string::npos);
}

TEST(ApduCatalog, DescribeSw_6982) {
    std::string d = describeSw(0x69, 0x82);
    EXPECT_NE(d.find("6982"), std::string::npos);
    EXPECT_NE(d.find("Security"), std::string::npos);
}

TEST(ApduCatalog, DescribeSw_Unknown) {
    std::string d = describeSw(0x00, 0x00);
    EXPECT_NE(d.find("unknown"), std::string::npos);
}
