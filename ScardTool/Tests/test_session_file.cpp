/**
 * @file test_session_file.cpp
 * @brief SessionFile birim testleri.
 */
#include <gtest/gtest.h>
#include <fstream>
#include <filesystem>
#include "Session/SessionFile.h"

namespace fs = std::filesystem;

// ── Test fixture ──────────────────────────────────────────────────────────────

class SessionFileTest : public ::testing::Test {
protected:
    std::string tmpFile;

    void SetUp() override {
        tmpFile = (fs::temp_directory_path() / "scardtool_test_session.tmp").string();
    }

    void TearDown() override {
        if (fs::exists(tmpFile)) fs::remove(tmpFile);
    }

    void writeFile(const std::string& content) {
        std::ofstream f(tmpFile);
        f << content;
    }
};

// ── Load ──────────────────────────────────────────────────────────────────────

TEST_F(SessionFileTest, LoadNonExistent) {
    SessionFile sess;
    EXPECT_FALSE(sess.loadFrom("/nonexistent/path/.scardtool_session"));
    EXPECT_FALSE(sess.isLoaded());
}

TEST_F(SessionFileTest, LoadBasic) {
    writeFile("reader=0\nreader-name=ACR1281U\n");
    SessionFile sess;
    ASSERT_TRUE(sess.loadFrom(tmpFile));
    EXPECT_TRUE(sess.isLoaded());
    EXPECT_EQ(*sess.get("reader"), "0");
    EXPECT_EQ(*sess.get("reader-name"), "ACR1281U");
}

TEST_F(SessionFileTest, LoadComment) {
    writeFile("# this is a comment\nreader=1\n");
    SessionFile sess;
    ASSERT_TRUE(sess.loadFrom(tmpFile));
    EXPECT_FALSE(sess.has("# this is a comment"));
    EXPECT_EQ(*sess.get("reader"), "1");
}

TEST_F(SessionFileTest, LoadEmptyLines) {
    writeFile("\n\nreader=2\n\n");
    SessionFile sess;
    ASSERT_TRUE(sess.loadFrom(tmpFile));
    EXPECT_EQ(*sess.get("reader"), "2");
}

TEST_F(SessionFileTest, LoadValueWithSpaces) {
    writeFile("reader-name = ACR 1281U \n");
    SessionFile sess;
    ASSERT_TRUE(sess.loadFrom(tmpFile));
    EXPECT_EQ(*sess.get("reader-name"), "ACR 1281U");
}

TEST_F(SessionFileTest, LoadMissingKey_ReturnsNullopt) {
    writeFile("reader=0\n");
    SessionFile sess;
    ASSERT_TRUE(sess.loadFrom(tmpFile));
    EXPECT_FALSE(sess.get("nonexistent").has_value());
}

// ── Set / Has / Remove ────────────────────────────────────────────────────────

TEST_F(SessionFileTest, SetAndGet) {
    SessionFile sess;
    sess.set("reader", "0");
    EXPECT_TRUE(sess.has("reader"));
    EXPECT_EQ(*sess.get("reader"), "0");
}

TEST_F(SessionFileTest, SetOverwrite) {
    SessionFile sess;
    sess.set("reader", "0");
    sess.set("reader", "1");
    EXPECT_EQ(*sess.get("reader"), "1");
}

TEST_F(SessionFileTest, Remove) {
    SessionFile sess;
    sess.set("reader", "0");
    sess.remove("reader");
    EXPECT_FALSE(sess.has("reader"));
}

// ── Save / Load roundtrip ─────────────────────────────────────────────────────

TEST_F(SessionFileTest, SaveAndLoadRoundtrip) {
    SessionFile sess;
    sess.set("reader", "0");
    sess.set("reader-name", "ACR1281U");
    sess.set("key", "FFFFFFFFFFFF");
    ASSERT_TRUE(sess.save(tmpFile));

    SessionFile sess2;
    ASSERT_TRUE(sess2.loadFrom(tmpFile));
    EXPECT_EQ(*sess2.get("reader"), "0");
    EXPECT_EQ(*sess2.get("reader-name"), "ACR1281U");
    EXPECT_EQ(*sess2.get("key"), "FFFFFFFFFFFF");
}

TEST_F(SessionFileTest, SaveOverwrite) {
    writeFile("old-key=oldval\n");
    SessionFile sess;
    ASSERT_TRUE(sess.loadFrom(tmpFile));
    sess.set("new-key", "newval");
    sess.save(tmpFile);

    SessionFile sess2;
    sess2.loadFrom(tmpFile);
    EXPECT_TRUE(sess2.has("old-key"));
    EXPECT_TRUE(sess2.has("new-key"));
}

// ── all() ─────────────────────────────────────────────────────────────────────

TEST_F(SessionFileTest, AllReturnsAllKeys) {
    writeFile("a=1\nb=2\nc=3\n");
    SessionFile sess;
    ASSERT_TRUE(sess.loadFrom(tmpFile));
    auto& all = sess.all();
    EXPECT_EQ(all.size(), 3u);
    EXPECT_NE(all.find("a"), all.end());
    EXPECT_NE(all.find("b"), all.end());
    EXPECT_NE(all.find("c"), all.end());
}
