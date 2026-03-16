/**
 * @file test_crypto.cpp
 * @brief CardCrypto ve PayloadCipher birim testleri.
 *
 * PCSC donanımı gerektirmeyen, deterministik şifreleme testleri.
 */
#include <gtest/gtest.h>
#include "Crypto/CardCrypto.h"
#include "Crypto/PayloadCipher.h"
#include "Commands/CommandRegistry.h"
#include "App/App.h"

// ── CardCrypto — string dönüşümleri ──────────────────────────────────────────

TEST(CardCrypto, ToString_FromString_Roundtrip) {
    std::vector<CipherAlgo> algos = {
        CipherAlgo::None, CipherAlgo::Detect, CipherAlgo::AesCtr,
        CipherAlgo::AesCbc, CipherAlgo::AesGcm, CipherAlgo::TripleDesCbc,
        CipherAlgo::Xor
    };
    for (auto a : algos) {
        EXPECT_EQ(CardCrypto::fromString(CardCrypto::toString(a)), a)
            << "Failed roundtrip for: " << CardCrypto::toString(a);
    }
}

TEST(CardCrypto, FromString_Invalid_ReturnsNone) {
    EXPECT_EQ(CardCrypto::fromString("garbage"), CipherAlgo::None);
    EXPECT_EQ(CardCrypto::fromString(""),         CipherAlgo::None);
}

TEST(CardCrypto, IsValidString) {
    EXPECT_TRUE(CardCrypto::isValidString("none"));
    EXPECT_TRUE(CardCrypto::isValidString("detect"));
    EXPECT_TRUE(CardCrypto::isValidString("aes-ctr"));
    EXPECT_TRUE(CardCrypto::isValidString("aes-cbc"));
    EXPECT_TRUE(CardCrypto::isValidString("aes-gcm"));
    EXPECT_TRUE(CardCrypto::isValidString("3des-cbc"));
    EXPECT_TRUE(CardCrypto::isValidString("xor"));
    EXPECT_FALSE(CardCrypto::isValidString("aes"));
    EXPECT_FALSE(CardCrypto::isValidString("garbage"));
}

TEST(CardCrypto, DisplayName_NotEmpty) {
    EXPECT_FALSE(CardCrypto::displayName(CipherAlgo::AesCtr).empty());
    EXPECT_FALSE(CardCrypto::displayName(CipherAlgo::AesGcm).empty());
    EXPECT_FALSE(CardCrypto::displayName(CipherAlgo::Xor).empty());
}

// ── CardCrypto — kart uygunluk analizi ───────────────────────────────────────

TEST(CardCrypto, Analyze_Classic1K_RecommendsAesCtr) {
    auto r = CardCrypto::analyze(CardType::MifareClassic1K, 16);
    EXPECT_EQ(r.recommended, CipherAlgo::AesCtr);
    EXPECT_EQ(r.blockSize, 16);
}

TEST(CardCrypto, Analyze_Ultralight_RecommendsAesCtr) {
    auto r = CardCrypto::analyze(CardType::MifareUltralight, 4);
    EXPECT_EQ(r.recommended, CipherAlgo::AesCtr);
}

TEST(CardCrypto, Analyze_DESFire_RecommendsAesGcm) {
    auto r = CardCrypto::analyze(CardType::MifareDesfire, 64);
    EXPECT_EQ(r.recommended, CipherAlgo::AesGcm);
}

TEST(CardCrypto, Analyze_Classic1K_GCM_NotSuitable) {
    auto r = CardCrypto::analyze(CardType::MifareClassic1K, 16);
    for (const auto& o : r.options) {
        if (o.algo == CipherAlgo::AesGcm) {
            EXPECT_FALSE(o.suitable) << "GCM should not be suitable for Classic";
        }
    }
}

TEST(CardCrypto, Analyze_Classic1K_CTR_Suitable) {
    auto r = CardCrypto::analyze(CardType::MifareClassic1K, 16);
    for (const auto& o : r.options) {
        if (o.algo == CipherAlgo::AesCtr) {
            EXPECT_TRUE(o.suitable);
        }
    }
}

TEST(CardCrypto, Analyze_Classic1K_CBC_Suitable) {
    // Classic1K blok = 16B = AES blok boyutu → CBC uygun
    auto r = CardCrypto::analyze(CardType::MifareClassic1K, 16);
    for (const auto& o : r.options) {
        if (o.algo == CipherAlgo::AesCbc) {
            EXPECT_TRUE(o.suitable) << "AES-CBC should be suitable for 16-byte blocks";
        }
    }
}

TEST(CardCrypto, Analyze_Ultralight_CBC_NotSuitable) {
    // Ultralight page = 4B < 16B AES block → CBC uygun değil
    auto r = CardCrypto::analyze(CardType::MifareUltralight, 4);
    for (const auto& o : r.options) {
        if (o.algo == CipherAlgo::AesCbc) {
            EXPECT_FALSE(o.suitable) << "AES-CBC should not be suitable for 4-byte pages";
        }
    }
}

TEST(CardCrypto, Analyze_OptionsNotEmpty) {
    auto r = CardCrypto::analyze(CardType::MifareClassic1K, 16);
    EXPECT_GT(r.options.size(), 3u);
}

TEST(CardCrypto, IsSuitable_AesCtr_Always) {
    EXPECT_TRUE(CardCrypto::isSuitable(CipherAlgo::AesCtr, CardType::MifareClassic1K, 16));
    EXPECT_TRUE(CardCrypto::isSuitable(CipherAlgo::AesCtr, CardType::MifareUltralight, 4));
    EXPECT_TRUE(CardCrypto::isSuitable(CipherAlgo::AesCtr, CardType::MifareDesfire, 64));
}

TEST(CardCrypto, IsSuitable_AesGcm_OnlyDESFire) {
    EXPECT_TRUE (CardCrypto::isSuitable(CipherAlgo::AesGcm, CardType::MifareDesfire,   64));
    EXPECT_FALSE(CardCrypto::isSuitable(CipherAlgo::AesGcm, CardType::MifareClassic1K, 16));
    EXPECT_FALSE(CardCrypto::isSuitable(CipherAlgo::AesGcm, CardType::MifareUltralight, 4));
}

// ── CardCrypto — trailer blok tespiti ────────────────────────────────────────

TEST(CardCrypto, IsTrailerBlock_Classic1K) {
    // Her 4. blok (3,7,11,...) trailer
    EXPECT_TRUE (CardCrypto::isTrailerBlock(3,  CardType::MifareClassic1K));
    EXPECT_TRUE (CardCrypto::isTrailerBlock(7,  CardType::MifareClassic1K));
    EXPECT_TRUE (CardCrypto::isTrailerBlock(63, CardType::MifareClassic1K));
    EXPECT_FALSE(CardCrypto::isTrailerBlock(0,  CardType::MifareClassic1K));
    EXPECT_FALSE(CardCrypto::isTrailerBlock(4,  CardType::MifareClassic1K));
    EXPECT_FALSE(CardCrypto::isTrailerBlock(62, CardType::MifareClassic1K));
}

TEST(CardCrypto, IsTrailerBlock_DESFire_NeverTrailer) {
    EXPECT_FALSE(CardCrypto::isTrailerBlock(0, CardType::MifareDesfire));
    EXPECT_FALSE(CardCrypto::isTrailerBlock(3, CardType::MifareDesfire));
    EXPECT_FALSE(CardCrypto::isTrailerBlock(7, CardType::MifareDesfire));
}

// ── PayloadCipher — AES-128-CTR roundtrip ────────────────────────────────────

class PayloadCipherTest : public ::testing::Test {
protected:
    const std::string password = "test_password_123";
    const std::string uid      = "04A1B2C3";
};

TEST_F(PayloadCipherTest, AesCtr_EncryptDecrypt_16Bytes) {
    BYTEV plaintext(16, 0xAB);
    PayloadCipher enc(password, uid, CipherAlgo::AesCtr);
    auto ciphertext = enc.encrypt(plaintext, 4);
    EXPECT_EQ(ciphertext.size(), 16u);  // CTR: çıktı = giriş
    EXPECT_NE(ciphertext, plaintext);

    PayloadCipher dec(password, uid, CipherAlgo::AesCtr);
    auto recovered = dec.decrypt(ciphertext, 4);
    EXPECT_EQ(recovered, plaintext);
}

TEST_F(PayloadCipherTest, AesCtr_EncryptDecrypt_4Bytes) {
    BYTEV plaintext = {0x01, 0x02, 0x03, 0x04};
    PayloadCipher pc(password, uid, CipherAlgo::AesCtr);
    auto ct = pc.encrypt(plaintext, 0);
    EXPECT_EQ(ct.size(), 4u);
    auto pt = pc.decrypt(ct, 0);
    EXPECT_EQ(pt, plaintext);
}

TEST_F(PayloadCipherTest, AesCtr_DifferentBlocks_DifferentCiphertext) {
    BYTEV plaintext(16, 0x55);
    PayloadCipher pc(password, uid, CipherAlgo::AesCtr);
    auto ct4 = pc.encrypt(plaintext, 4);
    auto ct8 = pc.encrypt(plaintext, 8);
    EXPECT_NE(ct4, ct8);  // Farklı blok → farklı IV → farklı ciphertext
}

TEST_F(PayloadCipherTest, AesCtr_DifferentPasswords_DifferentCiphertext) {
    BYTEV plaintext(16, 0x55);
    PayloadCipher pc1("password1", uid, CipherAlgo::AesCtr);
    PayloadCipher pc2("password2", uid, CipherAlgo::AesCtr);
    auto ct1 = pc1.encrypt(plaintext, 4);
    auto ct2 = pc2.encrypt(plaintext, 4);
    EXPECT_NE(ct1, ct2);
}

TEST_F(PayloadCipherTest, AesCtr_WrongPassword_DecryptsToGarbage) {
    BYTEV plaintext(16, 0xAB);
    PayloadCipher enc("correct_password", uid, CipherAlgo::AesCtr);
    auto ct = enc.encrypt(plaintext, 4);

    PayloadCipher dec("wrong_password", uid, CipherAlgo::AesCtr);
    auto garbage = dec.decrypt(ct, 4);
    EXPECT_NE(garbage, plaintext);  // Yanlış parola → farklı sonuç (CTR)
}

TEST_F(PayloadCipherTest, AesCbc_EncryptDecrypt_16Bytes) {
    BYTEV plaintext(16, 0xCC);
    PayloadCipher pc(password, uid, CipherAlgo::AesCbc);
    auto ct = pc.encrypt(plaintext, 4);
    auto pt = pc.decrypt(ct, 4);
    EXPECT_EQ(pt, plaintext);
}

TEST_F(PayloadCipherTest, AesCbc_NonAligned_Throws) {
    BYTEV plaintext(10, 0xCC);  // 10 byte, 16'ya bölünmez
    PayloadCipher pc(password, uid, CipherAlgo::AesCbc);
    EXPECT_THROW(pc.encrypt(plaintext, 4), std::runtime_error);
}

TEST_F(PayloadCipherTest, TripleDesCbc_EncryptDecrypt_16Bytes) {
    // 3DES-CBC OpenSSL adds PKCS#7 padding → ciphertext may be larger
    // 16-byte input + padding = 24 bytes output
    BYTEV plaintext(16, 0xDD);
    PayloadCipher enc(password, uid, CipherAlgo::TripleDesCbc);
    BYTEV ct;
    ASSERT_NO_THROW(ct = enc.encrypt(plaintext, 4));
    EXPECT_NE(ct, plaintext);
    // Decrypt and verify roundtrip
    PayloadCipher dec(password, uid, CipherAlgo::TripleDesCbc);
    BYTEV pt;
    ASSERT_NO_THROW(pt = dec.decrypt(ct, 4));
    EXPECT_EQ(pt, plaintext);
}

TEST_F(PayloadCipherTest, TripleDesCbc_4Bytes_Throws) {
    BYTEV plaintext(4, 0xDD);  // 4B < 8B DES blok
    PayloadCipher pc(password, uid, CipherAlgo::TripleDesCbc);
    EXPECT_THROW(pc.encrypt(plaintext, 0), std::runtime_error);
}

TEST_F(PayloadCipherTest, Xor_EncryptDecrypt_4Bytes) {
    BYTEV plaintext = {0x01, 0x02, 0x03, 0x04};
    PayloadCipher pc(password, uid, CipherAlgo::Xor);
    auto ct = pc.encrypt(plaintext, 0);
    auto pt = pc.decrypt(ct, 0);
    EXPECT_EQ(pt, plaintext);
}

TEST_F(PayloadCipherTest, Xor_OutputSameSize) {
    BYTEV plaintext(16, 0xAA);
    PayloadCipher pc(password, uid, CipherAlgo::Xor);
    auto ct = pc.encrypt(plaintext, 4);
    EXPECT_EQ(ct.size(), plaintext.size());
}

TEST_F(PayloadCipherTest, AlgoName) {
    PayloadCipher pc(password, uid, CipherAlgo::AesCtr);
    EXPECT_EQ(pc.algoName(), "aes-ctr");
}

TEST_F(PayloadCipherTest, ContextFor_DifferentBlocks_DifferentIV) {
    PayloadCipher pc(password, uid, CipherAlgo::AesCtr);
    auto ctx4 = pc.contextFor(4);
    auto ctx8 = pc.contextFor(8);
    EXPECT_NE(ctx4.iv, ctx8.iv);
    EXPECT_NE(ctx4.key, ctx8.key);
}

TEST_F(PayloadCipherTest, ContextFor_SameBlock_SameKey) {
    PayloadCipher pc(password, uid, CipherAlgo::AesCtr);
    auto ctx1 = pc.contextFor(4);
    auto ctx2 = pc.contextFor(4);
    EXPECT_EQ(ctx1.key, ctx2.key);
    EXPECT_EQ(ctx1.iv,  ctx2.iv);
}

TEST_F(PayloadCipherTest, EmptyUID_StillWorks) {
    BYTEV plaintext(16, 0x77);
    PayloadCipher enc(password, "", CipherAlgo::AesCtr);
    auto ct = enc.encrypt(plaintext, 4);
    PayloadCipher dec(password, "", CipherAlgo::AesCtr);
    auto pt = dec.decrypt(ct, 4);
    EXPECT_EQ(pt, plaintext);
}

// ── detect-cipher dry-run ─────────────────────────────────────────────────────

TEST(CmdDryRunCrypto, DetectCipher_DryRun_Success) {
    // Commands.h'de DetectCipherCmd dry-run testi
    // Registry'den doğrudan çağır
    CommandRegistry registry;
    App app;
    app.flags.dryRun = true;
    app.initFormatter();
    auto* cmd = registry.find("detect-cipher");
    ASSERT_NE(cmd, nullptr);
    std::vector<ArgDef> defs = {{"reader",'r',true},{"save-session",0,false}};
    std::vector<std::string> argStrs = {"scardtool","detect-cipher","-r","0"};
    std::vector<char*> argv;
    for (auto& s : argStrs) argv.push_back(const_cast<char*>(s.c_str()));
    auto parsed = ArgParser::parse(static_cast<int>(argv.size()), argv.data(), defs);
    CommandContext ctx{app, parsed, nullptr};
    EXPECT_EQ(cmd->execute(ctx), ExitCode::Success);
}

// ── AllNames ─────────────────────────────────────────────────────────────────
TEST(CardCrypto, AllNames_Count) {
    auto names = CardCrypto::allNames();
    EXPECT_EQ(names.size(), 7u);
    EXPECT_NE(std::find(names.begin(), names.end(), "aes-ctr"), names.end());
    EXPECT_NE(std::find(names.begin(), names.end(), "aes-gcm"), names.end());
    EXPECT_NE(std::find(names.begin(), names.end(), "3des-cbc"), names.end());
}
