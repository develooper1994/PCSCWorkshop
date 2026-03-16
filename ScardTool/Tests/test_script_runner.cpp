/**
 * @file test_script_runner.cpp
 * @brief ScriptRunner birim testleri.
 *
 * PCSC donanım gerektirmeyen komutları (dry-run, help, make-access vb.) test eder.
 */
#include <gtest/gtest.h>
#include <fstream>
#include <filesystem>
#include <sstream>
#include "Script/ScriptRunner.h"
#include "Commands/CommandRegistry.h"
#include "App/App.h"

namespace fs = std::filesystem;

// ── Tokenizer testleri ────────────────────────────────────────────────────────

TEST(Tokenizer, SemicolonSplit_Basic) {
    // ScriptRunner::splitSemicolon testi için inline run üzerinden
    // Direkt test edemiyoruz (private), tokenize ile dolaylı test
    auto tokens = ArgParser::tokenize("cmd1");
    ASSERT_EQ(tokens.size(), 1u);
    EXPECT_EQ(tokens[0], "cmd1");
}

// ── ScriptRunner ile dosya testleri ──────────────────────────────────────────

class ScriptRunnerTest : public ::testing::Test {
protected:
    CommandRegistry registry;
    App app;
    std::string tmpFile;

    void SetUp() override {
        app.flags.dryRun     = true;  // PCSC'ye dokunmaz
        app.flags.jsonOutput = false;
        app.flags.verbose    = false;
        app.initFormatter();
        tmpFile = (fs::temp_directory_path() / "scardtool_test.scard").string();
    }

    void TearDown() override {
        if (fs::exists(tmpFile)) fs::remove(tmpFile);
    }

    void writeScript(const std::string& content) {
        std::ofstream f(tmpFile);
        f << content;
    }
};

TEST_F(ScriptRunnerTest, RunFile_EmptyScript) {
    writeScript("");
    ScriptRunner runner(registry, app);
    EXPECT_EQ(runner.runFile(tmpFile), ExitCode::Success);
}

TEST_F(ScriptRunnerTest, RunFile_OnlyComments) {
    writeScript("# comment\n# another\n");
    ScriptRunner runner(registry, app);
    EXPECT_EQ(runner.runFile(tmpFile), ExitCode::Success);
}

TEST_F(ScriptRunnerTest, RunFile_Shebang) {
    writeScript("#!/usr/bin/env scardtool --script\n# comment\n");
    ScriptRunner runner(registry, app);
    EXPECT_EQ(runner.runFile(tmpFile), ExitCode::Success);
}

TEST_F(ScriptRunnerTest, RunFile_NonExistent) {
    ScriptRunner runner(registry, app);
    EXPECT_EQ(runner.runFile("/nonexistent/path.scard"), ExitCode::InvalidArgs);
}

TEST_F(ScriptRunnerTest, RunFile_DryRunListReaders) {
    writeScript("list-readers\n");
    ScriptRunner runner(registry, app);
    EXPECT_EQ(runner.runFile(tmpFile), ExitCode::Success);
}

TEST_F(ScriptRunnerTest, RunFile_MultipleCommands) {
    writeScript("list-readers\ndisconnect\n");
    ScriptRunner runner(registry, app);
    EXPECT_EQ(runner.runFile(tmpFile), ExitCode::Success);
}

TEST_F(ScriptRunnerTest, RunInline_SingleCmd) {
    ScriptRunner runner(registry, app);
    EXPECT_EQ(runner.runInline("list-readers"), ExitCode::Success);
}

TEST_F(ScriptRunnerTest, RunInline_MultiCmdSemicolon) {
    ScriptRunner runner(registry, app);
    EXPECT_EQ(runner.runInline("list-readers; disconnect"), ExitCode::Success);
}

TEST_F(ScriptRunnerTest, RunInline_UnknownCommand_ReturnsInvalidArgs) {
    ScriptRunner runner(registry, app, true); // stopOnError
    EXPECT_EQ(runner.runInline("unknownxyz"), ExitCode::InvalidArgs);
}

TEST_F(ScriptRunnerTest, RunInline_EmptyString) {
    ScriptRunner runner(registry, app);
    EXPECT_EQ(runner.runInline(""), ExitCode::Success);
}

TEST_F(ScriptRunnerTest, RunLine_EmptyLine) {
    ScriptRunner runner(registry, app);
    EXPECT_EQ(runner.runLine(""), ExitCode::Success);
}

TEST_F(ScriptRunnerTest, RunLine_OnlyComment) {
    ScriptRunner runner(registry, app);
    EXPECT_EQ(runner.runLine("# comment"), ExitCode::Success);
}

TEST_F(ScriptRunnerTest, StopOnError_StopsAtFirst) {
    // İlk komut hatalı, ikinci de hatalı olmalı ama çalışmamali
    ScriptRunner runner(registry, app, true); // stopOnError=true
    auto ec = runner.runInline("unknowncmd1; unknowncmd2");
    EXPECT_EQ(ec, ExitCode::InvalidArgs);
    EXPECT_EQ(runner.errorCount(), 1);  // sadece 1 hata sayılır
}

TEST_F(ScriptRunnerTest, DontStopOnError_CountsAll) {
    ScriptRunner runner(registry, app, false); // stopOnError=false
    runner.runInline("unknowncmd1; unknowncmd2");
    EXPECT_GE(runner.errorCount(), 1);
}
