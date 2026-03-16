/**
 * @file test_script_engine.cpp
 * @brief ScriptEngine birim testleri — değişken, koşul, döngü.
 */
#include <gtest/gtest.h>
#include "Script/ScriptEngine.h"
#include <vector>
#include <string>

// Test için basit bir dispatcher — çalıştırılan komutları kaydet
struct TestDispatcher {
    std::vector<std::string> executed;
    std::map<std::string, ExitCode> responses; // komut prefix → exit code

    ExitCode dispatch(const std::string& cmd) {
        executed.push_back(cmd);
        for (auto& [prefix, ec] : responses)
            if (cmd.find(prefix) != std::string::npos) return ec;
        return ExitCode::Success;
    }
};

// ── Yardımcı ─────────────────────────────────────────────────────────────────

static ScriptEngine makeEngine(TestDispatcher& d) {
    return ScriptEngine([&d](const std::string& cmd) {
        return d.dispatch(cmd);
    });
}

static std::vector<std::string> lines(std::initializer_list<const char*> ls) {
    std::vector<std::string> v;
    for (auto l : ls) v.push_back(l);
    return v;
}

// ── Değişkenler ───────────────────────────────────────────────────────────────

TEST(ScriptEngine, Variable_AssignAndExpand) {
    TestDispatcher d;
    auto e = makeEngine(d);
    e.execute(lines({"$READER = 0", "uid -r $READER"}));
    ASSERT_EQ(d.executed.size(), 1u);
    EXPECT_EQ(d.executed[0], "uid -r 0");
}

TEST(ScriptEngine, Variable_WithoutDollarSign) {
    TestDispatcher d;
    auto e = makeEngine(d);
    e.execute(lines({"KEY = FFFFFFFFFFFF", "load-key -k $KEY"}));
    ASSERT_EQ(d.executed.size(), 1u);
    EXPECT_EQ(d.executed[0], "load-key -k FFFFFFFFFFFF");
}

TEST(ScriptEngine, Variable_MultipleInOneLine) {
    TestDispatcher d;
    auto e = makeEngine(d);
    e.execute(lines({"$R = 0", "$P = 4", "read -r $R -p $P"}));
    ASSERT_EQ(d.executed.size(), 1u);
    EXPECT_EQ(d.executed[0], "read -r 0 -p 4");
}

TEST(ScriptEngine, Variable_Undefined_Passthrough) {
    TestDispatcher d;
    auto e = makeEngine(d);
    e.execute(lines({"cmd $UNDEF"}));
    // Tanımsız değişken → olduğu gibi bırak
    ASSERT_EQ(d.executed.size(), 1u);
    EXPECT_EQ(d.executed[0], "cmd $UNDEF");
}

// ── Aritmetik ─────────────────────────────────────────────────────────────────

TEST(ScriptEngine, Arithmetic_Addition) {
    TestDispatcher d;
    auto e = makeEngine(d);
    e.execute(lines({"$N = 3", "$N = $N + 2", "cmd $N"}));
    ASSERT_EQ(d.executed.size(), 1u);
    EXPECT_EQ(d.executed[0], "cmd 5");
}

TEST(ScriptEngine, Arithmetic_Subtraction) {
    TestDispatcher d;
    auto e = makeEngine(d);
    e.execute(lines({"$N = 10", "$N = $N - 4", "cmd $N"}));
    EXPECT_EQ(d.executed[0], "cmd 6");
}

TEST(ScriptEngine, Arithmetic_Multiplication) {
    TestDispatcher d;
    auto e = makeEngine(d);
    e.execute(lines({"$N = 3", "$N = $N * 4", "cmd $N"}));
    EXPECT_EQ(d.executed[0], "cmd 12");
}

// ── Echo ─────────────────────────────────────────────────────────────────────

TEST(ScriptEngine, Echo_Literal) {
    TestDispatcher d;
    auto e = makeEngine(d);
    testing::internal::CaptureStdout();
    e.execute(lines({"echo Hello"}));
    std::string out = testing::internal::GetCapturedStdout();
    EXPECT_EQ(out, "Hello\n");
    EXPECT_TRUE(d.executed.empty()); // echo komut değil
}

TEST(ScriptEngine, Echo_WithVariable) {
    TestDispatcher d;
    auto e = makeEngine(d);
    e.setVar("NAME", "world");
    testing::internal::CaptureStdout();
    e.execute(lines({"echo Hello $NAME"}));
    std::string out = testing::internal::GetCapturedStdout();
    EXPECT_EQ(out, "Hello world\n");
}

// ── if/else ──────────────────────────────────────────────────────────────────

TEST(ScriptEngine, If_Ok_Then) {
    TestDispatcher d;
    auto e = makeEngine(d);
    // Son komut başarılı → if ok dalı
    e.execute(lines({"cmd-a", "if ok", "echo-then", "fi"}));
    EXPECT_EQ(d.executed[0], "cmd-a");
    EXPECT_EQ(d.executed[1], "echo-then");
}

TEST(ScriptEngine, If_Fail_Else) {
    TestDispatcher d;
    d.responses["cmd-fail"] = ExitCode::GeneralError;
    auto e = makeEngine(d);
    // stop-on-error false: hata sonrası if bloğuna devam et
    e.execute(lines({"set stop-on-error false", "cmd-fail", "if ok", "echo-then", "else", "echo-else", "fi"}));
    EXPECT_TRUE(std::find(d.executed.begin(), d.executed.end(), "echo-else") != d.executed.end());
    EXPECT_TRUE(std::find(d.executed.begin(), d.executed.end(), "echo-then") == d.executed.end());
}

TEST(ScriptEngine, If_VarComparison) {
    TestDispatcher d;
    auto e = makeEngine(d);
    e.execute(lines({"$X = 5", "if $X == 5", "echo-match", "fi"}));
    EXPECT_TRUE(std::find(d.executed.begin(), d.executed.end(), "echo-match") != d.executed.end());
}

TEST(ScriptEngine, If_VarNotEqual) {
    TestDispatcher d;
    auto e = makeEngine(d);
    e.execute(lines({"$X = 3", "if $X == 5", "echo-match", "fi"}));
    EXPECT_TRUE(std::find(d.executed.begin(), d.executed.end(), "echo-match") == d.executed.end());
}

TEST(ScriptEngine, If_Elif) {
    TestDispatcher d;
    auto e = makeEngine(d);
    e.execute(lines({
        "$X = 7",
        "if $X == 5", "echo-five",
        "elif $X == 7", "echo-seven",
        "else", "echo-other",
        "fi"
    }));
    EXPECT_TRUE(std::find(d.executed.begin(), d.executed.end(), "echo-seven") != d.executed.end());
    EXPECT_TRUE(std::find(d.executed.begin(), d.executed.end(), "echo-five") == d.executed.end());
    EXPECT_TRUE(std::find(d.executed.begin(), d.executed.end(), "echo-other") == d.executed.end());
}

TEST(ScriptEngine, If_NumericLessThan) {
    TestDispatcher d;
    auto e = makeEngine(d);
    e.execute(lines({"$N = 3", "if $N < 10", "echo-lt", "fi"}));
    EXPECT_TRUE(std::find(d.executed.begin(), d.executed.end(), "echo-lt") != d.executed.end());
}

TEST(ScriptEngine, If_Not) {
    TestDispatcher d;
    auto e = makeEngine(d);
    e.execute(lines({"$X = 0", "if not $X == 1", "echo-not-one", "fi"}));
    EXPECT_TRUE(std::find(d.executed.begin(), d.executed.end(), "echo-not-one") != d.executed.end());
}

// ── while ─────────────────────────────────────────────────────────────────────

TEST(ScriptEngine, While_CountUp) {
    TestDispatcher d;
    auto e = makeEngine(d);
    e.execute(lines({"$I = 0", "while $I < 3", "cmd-iter $I", "$I = $I + 1", "done"}));
    ASSERT_EQ(d.executed.size(), 3u);
    EXPECT_EQ(d.executed[0], "cmd-iter 0");
    EXPECT_EQ(d.executed[1], "cmd-iter 1");
    EXPECT_EQ(d.executed[2], "cmd-iter 2");
}

TEST(ScriptEngine, While_ZeroIterations) {
    TestDispatcher d;
    auto e = makeEngine(d);
    e.execute(lines({"$I = 5", "while $I < 3", "cmd-iter", "done"}));
    EXPECT_TRUE(d.executed.empty());
}

TEST(ScriptEngine, While_ConditionWithArithmetic) {
    TestDispatcher d;
    auto e = makeEngine(d);
    e.execute(lines({"$S = 2", "$I = 0", "while $I < $S + 1", "cmd-iter", "$I = $I + 1", "done"}));
    EXPECT_EQ(d.executed.size(), 3u); // 0,1,2 → 3 iter
}

// ── for ───────────────────────────────────────────────────────────────────────

TEST(ScriptEngine, For_ListOfValues) {
    TestDispatcher d;
    auto e = makeEngine(d);
    e.execute(lines({"for $BLK in 0 1 2 3", "cmd-blk $BLK", "done"}));
    ASSERT_EQ(d.executed.size(), 4u);
    EXPECT_EQ(d.executed[0], "cmd-blk 0");
    EXPECT_EQ(d.executed[3], "cmd-blk 3");
}

TEST(ScriptEngine, For_WithVariable) {
    TestDispatcher d;
    auto e = makeEngine(d);
    e.setVar("KEY", "AABBCC");
    e.execute(lines({"for $S in 0 1", "load-key -k $KEY -s $S", "done"}));
    ASSERT_EQ(d.executed.size(), 2u);
    EXPECT_EQ(d.executed[0], "load-key -k AABBCC -s 0");
}

// ── Nested ────────────────────────────────────────────────────────────────────

TEST(ScriptEngine, Nested_ForInWhile) {
    TestDispatcher d;
    auto e = makeEngine(d);
    e.execute(lines({
        "$S = 0",
        "while $S < 2",
        "for $B in 0 1",
        "cmd $S $B",
        "done",
        "$S = $S + 1",
        "done"
    }));
    ASSERT_EQ(d.executed.size(), 4u);
    EXPECT_EQ(d.executed[0], "cmd 0 0");
    EXPECT_EQ(d.executed[1], "cmd 0 1");
    EXPECT_EQ(d.executed[2], "cmd 1 0");
    EXPECT_EQ(d.executed[3], "cmd 1 1");
}

// ── Comment / Shebang / Empty ─────────────────────────────────────────────────

TEST(ScriptEngine, Comment_Ignored) {
    TestDispatcher d;
    auto e = makeEngine(d);
    e.execute(lines({"# This is a comment", "cmd-real"}));
    ASSERT_EQ(d.executed.size(), 1u);
    EXPECT_EQ(d.executed[0], "cmd-real");
}

TEST(ScriptEngine, Shebang_Ignored) {
    TestDispatcher d;
    auto e = makeEngine(d);
    e.execute(lines({"#!/usr/bin/env scardtool", "cmd-real"}));
    ASSERT_EQ(d.executed.size(), 1u);
}

TEST(ScriptEngine, EmptyLines_Ignored) {
    TestDispatcher d;
    auto e = makeEngine(d);
    e.execute(lines({"", "  ", "cmd-real", ""}));
    ASSERT_EQ(d.executed.size(), 1u);
}

// ── stop-on-error ─────────────────────────────────────────────────────────────

TEST(ScriptEngine, StopOnError_Default) {
    TestDispatcher d;
    d.responses["cmd-fail"] = ExitCode::GeneralError;
    auto e = makeEngine(d);
    auto ec = e.execute(lines({"cmd-fail", "cmd-after"}));
    EXPECT_NE(ec, ExitCode::Success);
    // cmd-after çalıştırılmamalı
    EXPECT_TRUE(std::find(d.executed.begin(), d.executed.end(), "cmd-after") == d.executed.end());
}

TEST(ScriptEngine, StopOnError_False_Continues) {
    TestDispatcher d;
    d.responses["cmd-fail"] = ExitCode::GeneralError;
    auto e = makeEngine(d);
    e.execute(lines({"set stop-on-error false", "cmd-fail", "cmd-after"}));
    EXPECT_TRUE(std::find(d.executed.begin(), d.executed.end(), "cmd-after") != d.executed.end());
}

// ── Gerçek komut entegrasyonu (dry-run) ──────────────────────────────────────
// Bu testler real ScriptRunner+ScriptEngine zincirini test eder.
// Executor olarak gerçek binary yerine dispatch'i simüle eder.

TEST(ScriptEngine, RealCommand_Integration) {
    TestDispatcher d;
    auto e = makeEngine(d);
    e.execute(lines({
        "$R = 0",
        "$K = FFFFFFFFFFFF",
        "load-key --dry-run -r $R -k $K",
        "auth --dry-run -r $R -b 4 -t A",
        "read --dry-run -r $R -p 4 -l 16"
    }));
    ASSERT_EQ(d.executed.size(), 3u);
    EXPECT_EQ(d.executed[0], "load-key --dry-run -r 0 -k FFFFFFFFFFFF");
    EXPECT_EQ(d.executed[1], "auth --dry-run -r 0 -b 4 -t A");
    EXPECT_EQ(d.executed[2], "read --dry-run -r 0 -p 4 -l 16");
}
