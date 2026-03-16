/**
 * @file test_access_bits.cpp
 * @brief AccessBitsHelper encode/decode doğruluk testleri.
 *
 * Mifare Classic AN10609 referans değerleriyle karşılaştırma.
 */
#include <gtest/gtest.h>
#include "Card/TrailerFormat.h"
#include "Card/AccessBitsHelper.h"

// ── Bilinen değerlerle doğrulama ──────────────────────────────────────────────
// Değerler encode çalıştırılarak doğrulanmıştır.
//
// Mifare Classic access byte encoding:
//   byte6 = (~C2 & 0xF)<<4 | (~C1 & 0xF)
//   byte7 = ( C1 & 0xF)<<4 | (~C3 & 0xF)
//   byte8 = ( C3 & 0xF)<<4 | ( C2 & 0xF)
//
// C1/C2/C3 nibble = data[0].cx | data[1].cx<<1 | data[2].cx<<2 | trailer.cx<<3
//
// BlockAccess::bits() = (c3<<2)|(c2<<1)|c1
// Yani bits=4 → c1=0,c2=0,c3=1 (trailer default)
//      bits=1 → c1=1,c2=0,c3=0

struct KnownCase {
    uint8_t b0, b1, b2, bt;   // condition bits (via bits() = c3<<2|c2<<1|c1)
    uint8_t ab0, ab1, ab2;    // beklenen access bytes (GPB=0 hariç)
    const char* label;
};

static const KnownCase KNOWN_CASES[] = {
    // Transport default: data=000 (bits=0), trailer={0,0,1}=bits=4
    // C1=0, C2=0, C3=8 → byte6=FF, byte7=07, byte8=80
    {0,0,0, 4,  0xFF,0x07,0x80, "transport default (bt=4)"},

    // All data read-only (bits=2={0,1,0}), trailer bits=3={1,1,0}
    // C1=8, C2=0xF, C3=0 → byte6=07, byte7=8F, byte8=0F
    {2,2,2, 3,  0x07,0x8F,0x0F, "all read-only, trailer 3"},

    // All no-access (bits=7={1,1,1}), trailer bits=7
    // C1=C2=C3=0xF → byte6=00, byte7=F0, byte8=FF
    {7,7,7, 7,  0x00,0xF0,0xFF, "all no-access"},
};

TEST(AccessBitsEncode, KnownCases) {
    for (auto& tc : KNOWN_CASES) {
        SectorAccess sa;
        sa.data[0] = AccessBitsHelper::fromBits(tc.b0);
        sa.data[1] = AccessBitsHelper::fromBits(tc.b1);
        sa.data[2] = AccessBitsHelper::fromBits(tc.b2);
        sa.trailer = AccessBitsHelper::fromBits(tc.bt);

        auto ab = TrailerFormat::encodeAccess(sa, 0x00);
        EXPECT_EQ(ab[0], tc.ab0) << tc.label << " ab[0]";
        EXPECT_EQ(ab[1], tc.ab1) << tc.label << " ab[1]";
        EXPECT_EQ(ab[2], tc.ab2) << tc.label << " ab[2]";
    }
}

TEST(AccessBitsDecode, KnownCases) {
    for (auto& tc : KNOWN_CASES) {
        ACCESSBYTES ab = {tc.ab0, tc.ab1, tc.ab2, 0x00};
        auto sa = TrailerFormat::decodeAccess(ab);
        ASSERT_TRUE(sa.has_value()) << tc.label;
        EXPECT_EQ(sa->data[0].bits(), tc.b0) << tc.label << " data[0]";
        EXPECT_EQ(sa->data[1].bits(), tc.b1) << tc.label << " data[1]";
        EXPECT_EQ(sa->data[2].bits(), tc.b2) << tc.label << " data[2]";
        EXPECT_EQ(sa->trailer.bits(), tc.bt) << tc.label << " trailer";
    }
}

// ── Exhaustive roundtrip (tüm 8^4 = 4096 kombinasyon) ──────────────────────

TEST(AccessBitsRoundtrip, ExhaustiveAllCombinations) {
    int failures = 0;
    for (int b0 = 0; b0 < 8; ++b0)
    for (int b1 = 0; b1 < 8; ++b1)
    for (int b2 = 0; b2 < 8; ++b2)
    for (int bt = 0; bt < 8; ++bt) {
        SectorAccess sa;
        sa.data[0] = AccessBitsHelper::fromBits(static_cast<uint8_t>(b0));
        sa.data[1] = AccessBitsHelper::fromBits(static_cast<uint8_t>(b1));
        sa.data[2] = AccessBitsHelper::fromBits(static_cast<uint8_t>(b2));
        sa.trailer = AccessBitsHelper::fromBits(static_cast<uint8_t>(bt));

        auto ab  = TrailerFormat::encodeAccess(sa, 0x00);
        auto ret = TrailerFormat::decodeAccess(ab);

        if (!ret.has_value()) { ++failures; continue; }
        if (ret->data[0].bits() != sa.data[0].bits()) ++failures;
        if (ret->data[1].bits() != sa.data[1].bits()) ++failures;
        if (ret->data[2].bits() != sa.data[2].bits()) ++failures;
        if (ret->trailer.bits() != sa.trailer.bits()) ++failures;
    }
    EXPECT_EQ(failures, 0) << failures << " encode/decode failures in 4096 combinations";
}

// ── GPB korunuyor mu? ─────────────────────────────────────────────────────────

TEST(AccessBitsEncode, GPBPreserved) {
    SectorAccess sa = SectorAccess::transportDefault();
    auto ab69 = TrailerFormat::encodeAccess(sa, 0x69);
    auto ab00 = TrailerFormat::encodeAccess(sa, 0x00);

    // Access bytes aynı, sadece GPB farklı
    EXPECT_EQ(ab69[0], ab00[0]);
    EXPECT_EQ(ab69[1], ab00[1]);
    EXPECT_EQ(ab69[2], ab00[2]);
    EXPECT_EQ(ab69[3], 0x69);
    EXPECT_EQ(ab00[3], 0x00);
}

// ── toHex consistent ─────────────────────────────────────────────────────────

TEST(AccessBitsHelper, ToHexLength) {
    for (int b0 = 0; b0 < 8; ++b0) {
        SectorAccess sa{};
        sa.data[0] = AccessBitsHelper::fromBits(b0);
        sa.data[1] = AccessBitsHelper::fromBits(0);
        sa.data[2] = AccessBitsHelper::fromBits(0);
        sa.trailer = AccessBitsHelper::fromBits(1);
        std::string hex = AccessBitsHelper::toHex(sa);
        EXPECT_EQ(hex.size(), 8u) << "b0=" << b0;
    }
}
