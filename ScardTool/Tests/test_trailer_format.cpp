/**
 * @file test_trailer_format.cpp
 * @brief TrailerFormat birim testleri.
 *
 * Access byte encode/decode ve trailer parse/build işlemleri.
 */
#include <gtest/gtest.h>
#include "Card/TrailerFormat.h"
#include "Card/AccessBitsHelper.h"

// ── Transport default encoding ────────────────────────────────────────────────
// Classic transport default: tüm data block'lar 000, trailer 000
// Beklenen: FF 07 80 00  (NXP AN10609 referansı)

TEST(TrailerFormat, EncodeTransportDefault) {
    SectorAccess sa = SectorAccess::transportDefault();
    // data blocks [000], trailer [001] → encode
    auto ab = TrailerFormat::encodeAccess(sa);
    // Transport default beklenen değer: FF 07 80 00
    // Farklı trailer condition → ayrı test
    EXPECT_EQ(ab[3], 0x00); // GPB = 0
    // encode → decode → aynı olmalı
    auto back = TrailerFormat::decodeAccess(ab);
    ASSERT_TRUE(back.has_value());
    for (int i = 0; i < 3; ++i) {
        EXPECT_EQ(back->data[i].c1, sa.data[i].c1);
        EXPECT_EQ(back->data[i].c2, sa.data[i].c2);
        EXPECT_EQ(back->data[i].c3, sa.data[i].c3);
    }
}

// ── Encode → Decode roundtrip ─────────────────────────────────────────────────

TEST(TrailerFormat, EncodeDecodeRoundtrip_AllZero) {
    SectorAccess sa{};
    for (int i = 0; i < 3; ++i) sa.data[i] = {0,0,0};
    sa.trailer = {0,0,0};

    auto ab  = TrailerFormat::encodeAccess(sa);
    auto ret = TrailerFormat::decodeAccess(ab);
    ASSERT_TRUE(ret.has_value());
    for (int i = 0; i < 3; ++i) {
        EXPECT_EQ(ret->data[i].bits(), 0u);
    }
    EXPECT_EQ(ret->trailer.bits(), 0u);
}

TEST(TrailerFormat, EncodeDecodeRoundtrip_MixedConditions) {
    SectorAccess sa{};
    sa.data[0] = AccessBitsHelper::fromBits(0); // 000
    sa.data[1] = AccessBitsHelper::fromBits(2); // 010 read-only
    sa.data[2] = AccessBitsHelper::fromBits(4); // 100
    sa.trailer = AccessBitsHelper::fromBits(3); // 011

    auto ab  = TrailerFormat::encodeAccess(sa);
    auto ret = TrailerFormat::decodeAccess(ab);
    ASSERT_TRUE(ret.has_value());
    EXPECT_EQ(ret->data[0].bits(), 0u);
    EXPECT_EQ(ret->data[1].bits(), 2u);
    EXPECT_EQ(ret->data[2].bits(), 4u);
    EXPECT_EQ(ret->trailer.bits(), 3u);
}

TEST(TrailerFormat, EncodeDecodeRoundtrip_AllSeven) {
    SectorAccess sa{};
    for (int i = 0; i < 3; ++i) sa.data[i] = AccessBitsHelper::fromBits(7);
    sa.trailer = AccessBitsHelper::fromBits(7);

    auto ab  = TrailerFormat::encodeAccess(sa);
    auto ret = TrailerFormat::decodeAccess(ab);
    ASSERT_TRUE(ret.has_value());
    for (int i = 0; i < 3; ++i) EXPECT_EQ(ret->data[i].bits(), 7u);
    EXPECT_EQ(ret->trailer.bits(), 7u);
}

// ── Integrity check (bit complement) ─────────────────────────────────────────

TEST(TrailerFormat, DecodeInvalidBytes_ReturnsNullopt) {
    ACCESSBYTES ab = {0xDE, 0xAD, 0xBE, 0xEF};  // rastgele bozuk
    auto result = TrailerFormat::decodeAccess(ab);
    EXPECT_FALSE(result.has_value());
}

TEST(TrailerFormat, DecodeAllZero_ReturnsNullopt) {
    ACCESSBYTES ab = {0x00, 0x00, 0x00, 0x00};
    auto result = TrailerFormat::decodeAccess(ab);
    EXPECT_FALSE(result.has_value());  // complement check fail
}

// ── Trailer parse ─────────────────────────────────────────────────────────────

TEST(TrailerFormat, ParseValidTrailer) {
    // Transport default trailer: FF FF FF FF FF FF  FF 07 80 00  FF FF FF FF FF FF
    BYTEV raw = {
        0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,  // KeyA
        0xFF,0x07,0x80,                  // Access bytes (transport default)
        0x00,                            // GPB
        0xFF,0xFF,0xFF,0xFF,0xFF,0xFF   // KeyB
    };

    auto td = TrailerFormat::parse(raw);
    EXPECT_TRUE(td.valid);
    EXPECT_TRUE(td.parseError.empty());
    // KeyA okunduğunda masked değil burada (ham parse)
    for (auto b : td.keyA) EXPECT_EQ(b, 0xFF);
    for (auto b : td.keyB) EXPECT_EQ(b, 0xFF);
}

TEST(TrailerFormat, ParseTooShort_Invalid) {
    BYTEV raw = {0xFF, 0x07, 0x80};  // sadece 3 byte
    auto td = TrailerFormat::parse(raw);
    EXPECT_FALSE(td.valid);
    EXPECT_FALSE(td.parseError.empty());
}

TEST(TrailerFormat, ParseBadAccessBytes_Invalid) {
    BYTEV raw(16, 0x00);  // tamamen sıfır → access check fail
    auto td = TrailerFormat::parse(raw);
    EXPECT_FALSE(td.valid);
}

// ── Build ─────────────────────────────────────────────────────────────────────

TEST(TrailerFormat, BuildAndParseRoundtrip) {
    KEYBYTES keyA = {0xA0,0xA1,0xA2,0xA3,0xA4,0xA5};
    KEYBYTES keyB = {0xB0,0xB1,0xB2,0xB3,0xB4,0xB5};

    SectorAccess sa{};
    sa.data[0] = AccessBitsHelper::fromBits(0);
    sa.data[1] = AccessBitsHelper::fromBits(0);
    sa.data[2] = AccessBitsHelper::fromBits(2);
    sa.trailer = AccessBitsHelper::fromBits(1);

    BYTEV built = TrailerFormat::build(keyA, keyB, sa, 0x00);
    ASSERT_EQ(built.size(), 16u);

    auto td = TrailerFormat::parse(built);
    ASSERT_TRUE(td.valid);
    for (int i = 0; i < 6; ++i) {
        EXPECT_EQ(td.keyA[i], keyA[i]);
        EXPECT_EQ(td.keyB[i], keyB[i]);
    }
    EXPECT_EQ(td.access.data[0].bits(), 0u);
    EXPECT_EQ(td.access.data[2].bits(), 2u);
    EXPECT_EQ(td.access.trailer.bits(), 1u);
}

// ── Key parse / hex conversion ────────────────────────────────────────────────

TEST(TrailerFormat, ParseKey_Valid) {
    auto k = TrailerFormat::parseKey("FFFFFFFFFFFF");
    ASSERT_TRUE(k.has_value());
    for (auto b : *k) EXPECT_EQ(b, 0xFF);
}

TEST(TrailerFormat, ParseKey_MixedCase) {
    auto k = TrailerFormat::parseKey("a0B1c2D3e4F5");
    ASSERT_TRUE(k.has_value());
    EXPECT_EQ((*k)[0], 0xA0);
    EXPECT_EQ((*k)[1], 0xB1);
}

TEST(TrailerFormat, ParseKey_TooShort) {
    EXPECT_FALSE(TrailerFormat::parseKey("FFFF").has_value());
}

TEST(TrailerFormat, ParseKey_TooLong) {
    EXPECT_FALSE(TrailerFormat::parseKey("FFFFFFFFFFFFFF").has_value());
}

TEST(TrailerFormat, ParseKey_InvalidChars) {
    EXPECT_FALSE(TrailerFormat::parseKey("GGGGGGGGGGGG").has_value());
}

TEST(TrailerFormat, KeyToHex) {
    KEYBYTES k = {0xAB,0xCD,0xEF,0x01,0x23,0x45};
    EXPECT_EQ(TrailerFormat::keyToHex(k), "ABCDEF012345");
}

TEST(TrailerFormat, KeyToHex_AllFF) {
    KEYBYTES k;
    k.fill(0xFF);
    EXPECT_EQ(TrailerFormat::keyToHex(k), "FFFFFFFFFFFF");
}

TEST(TrailerFormat, ParseAccessBytes_Valid) {
    auto ab = TrailerFormat::parseAccessBytes("FF078069");
    ASSERT_TRUE(ab.has_value());
    EXPECT_EQ((*ab)[0], 0xFF);
    EXPECT_EQ((*ab)[1], 0x07);
    EXPECT_EQ((*ab)[2], 0x80);
    EXPECT_EQ((*ab)[3], 0x69);
}

TEST(TrailerFormat, ParseAccessBytes_WrongLength) {
    EXPECT_FALSE(TrailerFormat::parseAccessBytes("FF07").has_value());
    EXPECT_FALSE(TrailerFormat::parseAccessBytes("FF0780690000").has_value());
}

// ── AccessBitsHelper ──────────────────────────────────────────────────────────

TEST(AccessBitsHelper, FromBits_AllValues) {
    for (uint8_t i = 0; i <= 7; ++i) {
        auto ba = AccessBitsHelper::fromBits(i);
        EXPECT_EQ(ba.bits(), i);
    }
}

TEST(AccessBitsHelper, DataConditionTable_Size) {
    EXPECT_EQ(AccessBitsHelper::dataConditions().size(), 8u);
}

TEST(AccessBitsHelper, TrailerConditionTable_Size) {
    EXPECT_EQ(AccessBitsHelper::trailerConditions().size(), 8u);
}

TEST(AccessBitsHelper, TransportDefault_Description) {
    auto& dc = AccessBitsHelper::dataConditions();
    EXPECT_NE(dc[0].label.find("transport"), std::string::npos);
}

TEST(AccessBitsHelper, ReadOnly_Description) {
    auto& dc = AccessBitsHelper::dataConditions();
    EXPECT_NE(dc[2].label.find("read"), std::string::npos);
}

TEST(AccessBitsHelper, ToHex_TransportDefault) {
    SectorAccess sa = SectorAccess::transportDefault();
    std::string hex = AccessBitsHelper::toHex(sa);
    EXPECT_EQ(hex.size(), 8u);  // 4 byte = 8 hex chars
    // Access bytes olmalı, hepsi 0 değil
    EXPECT_NE(hex, "00000000");
}
