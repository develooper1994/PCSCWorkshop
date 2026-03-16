/**
 * @file test_arg_parser.cpp
 * @brief ArgParser birim testleri.
 */
#include <gtest/gtest.h>
#include "ArgParser/ArgParser.h"

// ── Test fixtures ─────────────────────────────────────────────────────────────

static const std::vector<ArgDef> DEFS = {
    {"reader",   'r', true },
    {"json",     'j', false},
    {"verbose",  'v', false},
    {"length",   'l', true },
    {"dry-run",  0,   false},
    {"output",   'o', true },
};

static ParsedArgs parse(std::vector<std::string> args) {
    std::vector<char*> argv;
    argv.push_back(const_cast<char*>("scardtool"));
    for (auto& s : args) argv.push_back(const_cast<char*>(s.c_str()));
    return ArgParser::parse(static_cast<int>(argv.size()), argv.data(), DEFS);
}

// ── Short flag ────────────────────────────────────────────────────────────────

TEST(ArgParser, ShortFlag) {
    auto p = parse({"-j"});
    EXPECT_TRUE(p.has("json"));
    EXPECT_FALSE(p.has("verbose"));
}

TEST(ArgParser, MultipleCombinedShortFlags) {
    auto p = parse({"-jv"});
    EXPECT_TRUE(p.has("json"));
    EXPECT_TRUE(p.has("verbose"));
}

// ── Short option with value ───────────────────────────────────────────────────

TEST(ArgParser, ShortOptionSpace) {
    auto p = parse({"-r", "0"});
    ASSERT_TRUE(p.get("reader").has_value());
    EXPECT_EQ(*p.get("reader"), "0");
}

TEST(ArgParser, ShortOptionAdjacent) {
    auto p = parse({"-r0"});
    ASSERT_TRUE(p.get("reader").has_value());
    EXPECT_EQ(*p.get("reader"), "0");
}

TEST(ArgParser, ShortOptionThenFlag) {
    auto p = parse({"-r", "0", "-j"});
    EXPECT_EQ(*p.get("reader"), "0");
    EXPECT_TRUE(p.has("json"));
}

// ── Long flag ─────────────────────────────────────────────────────────────────

TEST(ArgParser, LongFlag) {
    auto p = parse({"--json"});
    EXPECT_TRUE(p.has("json"));
}

TEST(ArgParser, LongFlagNoShort) {
    auto p = parse({"--dry-run"});
    EXPECT_TRUE(p.has("dry-run"));
}

// ── Long option with value ────────────────────────────────────────────────────

TEST(ArgParser, LongOptionSpace) {
    auto p = parse({"--reader", "ACR1281U"});
    EXPECT_EQ(*p.get("reader"), "ACR1281U");
}

TEST(ArgParser, LongOptionEquals) {
    auto p = parse({"--reader=ACR1281U"});
    EXPECT_EQ(*p.get("reader"), "ACR1281U");
}

TEST(ArgParser, LongOptionEqualsWithSpaceInValue) {
    auto p = parse({"--output=my output.txt"});
    EXPECT_EQ(*p.get("output"), "my output.txt");
}

// ── Subcommand and positional ─────────────────────────────────────────────────

TEST(ArgParser, SubcommandOnly) {
    auto p = parse({"list-readers"});
    EXPECT_EQ(p.subcommand, "list-readers");
    EXPECT_TRUE(p.positional.empty());
}

TEST(ArgParser, SubcommandWithArgs) {
    auto p = parse({"send-apdu", "-r", "0", "-j"});
    EXPECT_EQ(p.subcommand, "send-apdu");
    EXPECT_EQ(*p.get("reader"), "0");
    EXPECT_TRUE(p.has("json"));
}

TEST(ArgParser, EndOfOptionsMarker) {
    auto p = parse({"--", "not-a-flag"});
    EXPECT_EQ(p.subcommand, "not-a-flag");
}

// ── getOr ─────────────────────────────────────────────────────────────────────

TEST(ArgParser, GetOrReturnsDefault) {
    auto p = parse({"-j"});
    EXPECT_EQ(p.getOr("reader", "default_val"), "default_val");
}

TEST(ArgParser, GetOrReturnsValue) {
    auto p = parse({"-r", "1"});
    EXPECT_EQ(p.getOr("reader", "default_val"), "1");
}

// ── Error cases ───────────────────────────────────────────────────────────────

TEST(ArgParser, UnknownLongOption) {
    EXPECT_THROW(parse({"--unknown-opt"}), std::invalid_argument);
}

TEST(ArgParser, UnknownShortOption) {
    EXPECT_THROW(parse({"-Z"}), std::invalid_argument);
}

TEST(ArgParser, MissingValueForLong) {
    EXPECT_THROW(parse({"--reader"}), std::invalid_argument);
}

TEST(ArgParser, MissingValueForShort) {
    EXPECT_THROW(parse({"-r"}), std::invalid_argument);
}

// ── Tokenizer ─────────────────────────────────────────────────────────────────

TEST(ArgParser, TokenizeSimple) {
    auto t = ArgParser::tokenize("list-readers -j");
    ASSERT_EQ(t.size(), 2u);
    EXPECT_EQ(t[0], "list-readers");
    EXPECT_EQ(t[1], "-j");
}

TEST(ArgParser, TokenizeDoubleQuotes) {
    auto t = ArgParser::tokenize("send-apdu -a \"FF CA 00\"");
    ASSERT_EQ(t.size(), 3u);
    EXPECT_EQ(t[2], "FF CA 00");
}

TEST(ArgParser, TokenizeSingleQuotes) {
    auto t = ArgParser::tokenize("cmd -k 'FFFFFFFFFFFF'");
    ASSERT_EQ(t.size(), 3u);
    EXPECT_EQ(t[2], "FFFFFFFFFFFF");
}

TEST(ArgParser, TokenizeEmpty) {
    auto t = ArgParser::tokenize("   ");
    EXPECT_TRUE(t.empty());
}

TEST(ArgParser, TokenizeMultipleSpaces) {
    auto t = ArgParser::tokenize("a   b   c");
    ASSERT_EQ(t.size(), 3u);
}

// ── parseLine ─────────────────────────────────────────────────────────────────

TEST(ArgParser, ParseLineBasic) {
    auto p = ArgParser::parseLine("uid -r 0 -j", DEFS);
    EXPECT_EQ(p.subcommand, "uid");
    EXPECT_EQ(*p.get("reader"), "0");
    EXPECT_TRUE(p.has("json"));
}

TEST(ArgParser, ParseLineEmpty) {
    auto p = ArgParser::parseLine("", DEFS);
    EXPECT_TRUE(p.subcommand.empty());
}
