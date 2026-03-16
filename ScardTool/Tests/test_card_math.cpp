/**
 * @file test_card_math.cpp
 * @brief CardMath birim testleri.
 */
#include <gtest/gtest.h>
#include "Card/CardMath.h"

// ── Classic 1K ────────────────────────────────────────────────────────────────

TEST(CardMath, Classic1K_SectorCount) {
    EXPECT_EQ(CardMath::sectorCount(CardType::MifareClassic1K), 16);
}

TEST(CardMath, Classic1K_BlocksInSector) {
    for (int s = 0; s < 16; ++s)
        EXPECT_EQ(CardMath::blocksInSector(s, CardType::MifareClassic1K), 4);
}

TEST(CardMath, Classic1K_FirstBlock) {
    EXPECT_EQ(CardMath::sectorFirstBlock(0,  CardType::MifareClassic1K), 0);
    EXPECT_EQ(CardMath::sectorFirstBlock(1,  CardType::MifareClassic1K), 4);
    EXPECT_EQ(CardMath::sectorFirstBlock(15, CardType::MifareClassic1K), 60);
}

TEST(CardMath, Classic1K_TrailerBlock) {
    EXPECT_EQ(CardMath::trailerBlock(0,  CardType::MifareClassic1K), 3);
    EXPECT_EQ(CardMath::trailerBlock(1,  CardType::MifareClassic1K), 7);
    EXPECT_EQ(CardMath::trailerBlock(15, CardType::MifareClassic1K), 63);
}

TEST(CardMath, Classic1K_SectorBlocks) {
    auto blocks = CardMath::sectorBlocks(0, CardType::MifareClassic1K);
    ASSERT_EQ(blocks.size(), 4u);
    EXPECT_EQ(blocks[0], 0);
    EXPECT_EQ(blocks[3], 3);

    auto blocks1 = CardMath::sectorBlocks(1, CardType::MifareClassic1K);
    EXPECT_EQ(blocks1[0], 4);
    EXPECT_EQ(blocks1[3], 7);
}

TEST(CardMath, Classic1K_BlockToSector) {
    EXPECT_EQ(CardMath::blockToSector(0,  CardType::MifareClassic1K), 0);
    EXPECT_EQ(CardMath::blockToSector(3,  CardType::MifareClassic1K), 0);
    EXPECT_EQ(CardMath::blockToSector(4,  CardType::MifareClassic1K), 1);
    EXPECT_EQ(CardMath::blockToSector(63, CardType::MifareClassic1K), 15);
}

TEST(CardMath, Classic1K_BlockKind) {
    EXPECT_EQ(CardMath::blockKind(0,  CardType::MifareClassic1K), CardBlockKind::Manufacturer);
    EXPECT_EQ(CardMath::blockKind(1,  CardType::MifareClassic1K), CardBlockKind::Data);
    EXPECT_EQ(CardMath::blockKind(3,  CardType::MifareClassic1K), CardBlockKind::Trailer);
    EXPECT_EQ(CardMath::blockKind(63, CardType::MifareClassic1K), CardBlockKind::Trailer);
}

TEST(CardMath, Classic1K_IsTrailer) {
    EXPECT_TRUE (CardMath::isTrailer(3,  CardType::MifareClassic1K));
    EXPECT_FALSE(CardMath::isTrailer(2,  CardType::MifareClassic1K));
    EXPECT_TRUE (CardMath::isTrailer(63, CardType::MifareClassic1K));
}

TEST(CardMath, Classic1K_ValidBlock) {
    EXPECT_TRUE (CardMath::validBlock(0,  CardType::MifareClassic1K));
    EXPECT_TRUE (CardMath::validBlock(63, CardType::MifareClassic1K));
    EXPECT_FALSE(CardMath::validBlock(64, CardType::MifareClassic1K));
    EXPECT_FALSE(CardMath::validBlock(-1, CardType::MifareClassic1K));
}

TEST(CardMath, Classic1K_ValidSector) {
    EXPECT_TRUE (CardMath::validSector(0,  CardType::MifareClassic1K));
    EXPECT_TRUE (CardMath::validSector(15, CardType::MifareClassic1K));
    EXPECT_FALSE(CardMath::validSector(16, CardType::MifareClassic1K));
}

TEST(CardMath, Classic1K_BlockSize) {
    EXPECT_EQ(CardMath::blockSize(CardType::MifareClassic1K), 16);
}

// ── Classic 4K ────────────────────────────────────────────────────────────────

TEST(CardMath, Classic4K_SectorCount) {
    EXPECT_EQ(CardMath::sectorCount(CardType::MifareClassic4K), 40);
}

TEST(CardMath, Classic4K_SmallSectorBlocks) {
    EXPECT_EQ(CardMath::blocksInSector(0,  CardType::MifareClassic4K), 4);
    EXPECT_EQ(CardMath::blocksInSector(31, CardType::MifareClassic4K), 4);
}

TEST(CardMath, Classic4K_LargeSectorBlocks) {
    EXPECT_EQ(CardMath::blocksInSector(32, CardType::MifareClassic4K), 16);
    EXPECT_EQ(CardMath::blocksInSector(39, CardType::MifareClassic4K), 16);
}

TEST(CardMath, Classic4K_LargeSectorFirstBlock) {
    // Sector 32 = ilk büyük sektör = block 128
    EXPECT_EQ(CardMath::sectorFirstBlock(32, CardType::MifareClassic4K), 128);
    EXPECT_EQ(CardMath::sectorFirstBlock(33, CardType::MifareClassic4K), 144);
}

TEST(CardMath, Classic4K_LargeSectorTrailer) {
    EXPECT_EQ(CardMath::trailerBlock(32, CardType::MifareClassic4K), 143);
    EXPECT_EQ(CardMath::trailerBlock(39, CardType::MifareClassic4K), 255);
}

TEST(CardMath, Classic4K_BlockToSector_LargeSector) {
    EXPECT_EQ(CardMath::blockToSector(128, CardType::MifareClassic4K), 32);
    EXPECT_EQ(CardMath::blockToSector(143, CardType::MifareClassic4K), 32);
    EXPECT_EQ(CardMath::blockToSector(144, CardType::MifareClassic4K), 33);
}

// ── Ultralight ────────────────────────────────────────────────────────────────

TEST(CardMath, Ultralight_BlockSize) {
    EXPECT_EQ(CardMath::blockSize(CardType::MifareUltralight), 4);
}

TEST(CardMath, Ultralight_ValidPages) {
    EXPECT_TRUE (CardMath::validBlock(0,  CardType::MifareUltralight));
    EXPECT_TRUE (CardMath::validBlock(15, CardType::MifareUltralight));
    EXPECT_FALSE(CardMath::validBlock(16, CardType::MifareUltralight));
}

// ── CardType string conversion ────────────────────────────────────────────────

TEST(CardMath, FromString_Classic1K) {
    EXPECT_EQ(CardMath::fromString("classic1k"), CardType::MifareClassic1K);
    EXPECT_EQ(CardMath::fromString("1k"),        CardType::MifareClassic1K);
}

TEST(CardMath, FromString_Classic4K) {
    EXPECT_EQ(CardMath::fromString("classic4k"), CardType::MifareClassic4K);
    EXPECT_EQ(CardMath::fromString("4k"),        CardType::MifareClassic4K);
}

TEST(CardMath, FromString_Ultralight) {
    EXPECT_EQ(CardMath::fromString("ultralight"), CardType::MifareUltralight);
    EXPECT_EQ(CardMath::fromString("ul"),         CardType::MifareUltralight);
}

TEST(CardMath, FromString_Unknown) {
    EXPECT_EQ(CardMath::fromString("garbage"), CardType::Unknown);
}

TEST(CardMath, ToString) {
    EXPECT_EQ(CardMath::toString(CardType::MifareClassic1K),  "classic1k");
    EXPECT_EQ(CardMath::toString(CardType::MifareClassic4K),  "classic4k");
    EXPECT_EQ(CardMath::toString(CardType::MifareUltralight), "ultralight");
    EXPECT_EQ(CardMath::toString(CardType::MifareDesfire),    "desfire");
    EXPECT_EQ(CardMath::toString(CardType::Unknown),          "unknown");
}
