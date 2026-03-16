/**
 * @file test_hex_utils.cpp
 * @brief HexUtils birim testleri.
 */
#include <gtest/gtest.h>
#include "Mcp/HexUtils.h"

// ── toHex ─────────────────────────────────────────────────────────────────────

TEST(HexUtils, ToHex_Empty) {
    EXPECT_EQ(mcp::toHex({}), "");
}

TEST(HexUtils, ToHex_SingleByte) {
    EXPECT_EQ(mcp::toHex({0xFF}), "FF");
    EXPECT_EQ(mcp::toHex({0x00}), "00");
    EXPECT_EQ(mcp::toHex({0x0A}), "0A");
}

TEST(HexUtils, ToHex_MultipleBytes) {
    EXPECT_EQ(mcp::toHex({0xFF,0xCA,0x00,0x00,0x00}), "FFCA000000");
}

TEST(HexUtils, ToHex_Uppercase) {
    EXPECT_EQ(mcp::toHex({0xAB,0xCD,0xEF}), "ABCDEF");
}

TEST(HexUtils, ToHex_LeadingZero) {
    EXPECT_EQ(mcp::toHex({0x01,0x02}), "0102");
}

// ── fromHex ───────────────────────────────────────────────────────────────────

TEST(HexUtils, FromHex_Empty) {
    EXPECT_EQ(mcp::fromHex(""), BYTEV{});
}

TEST(HexUtils, FromHex_SingleByte) {
    EXPECT_EQ(mcp::fromHex("FF"), (BYTEV{0xFF}));
    EXPECT_EQ(mcp::fromHex("00"), (BYTEV{0x00}));
    EXPECT_EQ(mcp::fromHex("0A"), (BYTEV{0x0A}));
}

TEST(HexUtils, FromHex_MultipleBytes) {
    auto r = mcp::fromHex("FFCA000000");
    ASSERT_EQ(r.size(), 5u);
    EXPECT_EQ(r[0], 0xFF);
    EXPECT_EQ(r[1], 0xCA);
    EXPECT_EQ(r[4], 0x00);
}

TEST(HexUtils, FromHex_LowerCase) {
    auto r = mcp::fromHex("abcdef");
    ASSERT_EQ(r.size(), 3u);
    EXPECT_EQ(r[0], 0xAB);
    EXPECT_EQ(r[1], 0xCD);
    EXPECT_EQ(r[2], 0xEF);
}

TEST(HexUtils, FromHex_MixedCase) {
    auto r = mcp::fromHex("AbCd");
    ASSERT_EQ(r.size(), 2u);
    EXPECT_EQ(r[0], 0xAB);
    EXPECT_EQ(r[1], 0xCD);
}

TEST(HexUtils, FromHex_OddLength_Throws) {
    EXPECT_THROW(mcp::fromHex("F"), std::invalid_argument);
    EXPECT_THROW(mcp::fromHex("FFF"), std::invalid_argument);
}

TEST(HexUtils, FromHex_InvalidChar_Throws) {
    EXPECT_THROW(mcp::fromHex("GG"), std::invalid_argument);
    EXPECT_THROW(mcp::fromHex("ZZ"), std::invalid_argument);
    EXPECT_THROW(mcp::fromHex("FF GG"), std::invalid_argument);
}

// ── Roundtrip ─────────────────────────────────────────────────────────────────

TEST(HexUtils, Roundtrip_ToThenFrom) {
    BYTEV original = {0x00,0xFF,0xAB,0xCD,0xEF,0x12,0x34};
    auto hex  = mcp::toHex(original);
    auto back = mcp::fromHex(hex);
    EXPECT_EQ(original, back);
}

TEST(HexUtils, Roundtrip_FromThenTo) {
    std::string original = "DEADBEEF01234567";
    auto bytes = mcp::fromHex(original);
    auto back  = mcp::toHex(bytes);
    EXPECT_EQ(original, back);
}

// ── Key roundtrip ─────────────────────────────────────────────────────────────

TEST(HexUtils, MifareKey_FFFFFFFFFFFF) {
    auto bytes = mcp::fromHex("FFFFFFFFFFFF");
    ASSERT_EQ(bytes.size(), 6u);
    for (auto b : bytes) EXPECT_EQ(b, 0xFF);
    EXPECT_EQ(mcp::toHex(bytes), "FFFFFFFFFFFF");
}

TEST(HexUtils, APDUSelect) {
    auto apdu = mcp::fromHex("00A4040007D276000085010000");
    EXPECT_EQ(apdu.size(), 13u);
    EXPECT_EQ(apdu[0], 0x00);
    EXPECT_EQ(apdu[1], 0xA4);
}
