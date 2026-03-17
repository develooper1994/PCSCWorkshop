/** @file ScriptRunner.h
 * @brief .scard script file / stdin executor with semicolon and script-engine support. */
#pragma once
#include "Script/ScriptEngine.h"
#include "Commands/CommandRegistry.h"
#include "App/App.h"
#include "Crypto/CardCrypto.h"
#include "ExitCode.h"
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>
#include <functional>

// ════════════════════════════════════════════════════════════════════════════════
// ScriptRunner — .scard script dosyası / stdin çalıştırıcı
//
// Desteklenen özellikler:
//   - Shebang satırı: #!/usr/bin/env scardtool --script  (atlanır)
//   - Yorum satırı:   # bu bir yorum
//   - Boş satırlar:   atlanır
//   - Inline multi:   connect -r 0; uid -r 0; ats -r 0
//   - Normal satır:   send-apdu -r 0 -a 00A4040007...
//
// Hata davranışı:
//   - stopOnError=true  → ilk hatada dur (varsayılan)
//   - stopOnError=false → tüm satırları çalıştır, sonunda hata say
// ════════════════════════════════════════════════════════════════════════════════

class ScriptRunner {
public:
    explicit ScriptRunner(CommandRegistry& registry,
                          App&             app,
                          bool             stopOnError = true)
        : registry_(registry), app_(app), stopOnError_(stopOnError) {}

    /** @brief Interactive prompt fonksiyonu set et (interactive mode için). */
    void setPromptFn(std::function<std::string(const std::string&)> fn) {
        promptFn_ = std::move(fn);
    }

    // Dosyadan çalıştır
    ExitCode runFile(const std::string& path) {
        std::ifstream f(path);
        if (!f.is_open()) {
            std::cerr << "Error: Cannot open script file: " << path << "\n";
            return ExitCode::InvalidArgs;
        }
        return runStream(f, path);
    }

    // stdin'den çalıştır (pipe / redirect)
    /** @brief Çok satırlı script string'ini çalıştır (C API ve test için). */
    ExitCode runString(const std::string& script) {
        std::istringstream ss(script);
        return runStream(ss, "<string>");
    }

    ExitCode runStdin() {
        return runStream(std::cin, "<stdin>");
    }

    // Tek satır / inline multi-command ("cmd1; cmd2; cmd3")
    ExitCode runLine(const std::string& line) {
        auto cmds = splitSemicolon(line);
        for (auto& cmd : cmds) {
            auto trimmed = trim(cmd);
            if (trimmed.empty() || trimmed[0] == '#') continue;
            auto ec = runSingle(trimmed);
            if (ec != ExitCode::Success) {
                errors_++;
                if (stopOnError_) return ec;
                lastError_ = ec;
            }
        }
        return errors_ > 0 ? lastError_ : ExitCode::Success;
    }

    // -e "cmd1; cmd2" inline exec
    ExitCode runInline(const std::string& cmds) {
        return runLine(cmds);
    }

    int errorCount() const { return errors_; }

private:
    /** @brief ScriptEngine'i lazy init et — dispatcher closure ile. */
    ScriptEngine& getEngine() {
        if (!engine_) {
            engine_ = std::make_unique<ScriptEngine>(
                [this](const std::string& cmdLine) -> ExitCode {
                    // Noktalı virgül ile birden fazla komut
                    auto cmds = splitSemicolon(cmdLine);
                    ExitCode last = ExitCode::Success;
                    for (auto& cmd : cmds) {
                        auto t = trim(cmd);
                        if (t.empty() || t[0] == '#') continue;
                        last = runSingle(t);
                        if (last != ExitCode::Success) {
                            ++errors_; lastError_ = last;
                            if (stopOnError_) return last;
                        }
                    }
                    return last;
                }
            );
        }
        return *engine_;
    }

    CommandRegistry& registry_;
    App&             app_;
    bool             stopOnError_;
    int              errors_    = 0;
    ExitCode         lastError_ = ExitCode::Success;
    std::function<std::string(const std::string&)> promptFn_;
    std::unique_ptr<ScriptEngine> engine_; ///< Script dili motoru (lazy init)

    ExitCode runStream(std::istream& stream, const std::string& source) {
        // Satırları topla
        std::vector<std::string> allLines;
        std::string line;
        while (std::getline(stream, line)) allLines.push_back(line);

        // ScriptEngine ile çalıştır (değişken/if/loop desteği)
        auto& engine = getEngine();
        ExitCode ec = engine.execute(allLines);
        if (ec != ExitCode::Success) {
            ++errors_; lastError_ = ec;
        }
        (void)source;
        return errors_ > 0 ? lastError_ : ExitCode::Success;
    }

    ExitCode runStream_legacy(std::istream& stream, const std::string& source) {
        std::string line;
        int lineNo = 0;

        while (std::getline(stream, line)) {
            ++lineNo;

            // Shebang (sadece ilk satır)
            if (lineNo == 1 && line.substr(0, 2) == "#!") continue;

            // Yorum ve boş satır
            auto t = trim(line);
            if (t.empty() || t[0] == '#') continue;

            app_.fmt.verbose("[" + source + ":" + std::to_string(lineNo) + "] " + t);

            auto ec = runLine(t);
            if (ec != ExitCode::Success && stopOnError_) {
                std::cerr << "[" << source << ":" << lineNo << "] "
                          << "Script stopped on error (exit code "
                          << toInt(ec) << ")\n";
                return ec;
            }
        }
        return errors_ > 0 ? lastError_ : ExitCode::Success;
    }

    ExitCode runSingle(const std::string& line) {
        // Global arg defs — main.cpp GLOBAL_DEFS ile senkron tutulmalı
        std::vector<ArgDef> globalDefs = {
            {"json",         'j', false},
            {"verbose",      'v', false},
            {"dry-run",      0,   false},
            {"session",      'S', false},
            {"save-session", 0,   false},
            {"help",         'h', false},
            // ── Şifreleme ───────────────────────────────────────────────────
            {"encrypt",      'E', false},
            {"cipher",       0,   true },
            {"password",     'P', true },
            {"no-warn",      'q', false},
        };

        ParsedArgs parsed;
        try {
            // Tüm komutların arg def'lerini topla
            std::vector<ArgDef> allDefs = globalDefs;
            if (auto* cmd = registry_.find(ArgParser::tokenize(line)[0])) {
                for (auto& d : cmd->argDefs()) allDefs.push_back(d);
            }
            parsed = ArgParser::parseLine(line, allDefs);
        } catch (const std::exception& e) {
            std::cerr << "Parse error: " << e.what() << "\n";
            return ExitCode::InvalidArgs;
        }

        if (parsed.subcommand.empty()) return ExitCode::Success;

        // Satır bazlı global flag override'ları app_'e uygula
        // (script içinde -j, -v, -q, -E gibi flagler satır başında kullanılabilir)
        bool flagsChanged = false;
        if (parsed.has("json")    && !app_.flags.jsonOutput) { app_.flags.jsonOutput = true; flagsChanged = true; }
        if (parsed.has("verbose") && !app_.flags.verbose)    { app_.flags.verbose    = true; flagsChanged = true; }
        if (parsed.has("no-warn") && !app_.flags.noWarn)     { app_.flags.noWarn     = true; flagsChanged = true; }
        if (parsed.has("encrypt") && !app_.flags.encrypt)    { app_.flags.encrypt    = true; flagsChanged = true; }
        if (auto cs = parsed.get("cipher")) { app_.flags.cipher = CardCrypto::fromString(*cs); flagsChanged = true; }
        if (auto pw = parsed.get("password")) { app_.flags.password = *pw; }
        if (flagsChanged) app_.initFormatter();

        auto* cmd = registry_.find(parsed.subcommand);
        if (!cmd) {
            std::cerr << "Error: Unknown command '" << parsed.subcommand
                      << "'. Run 'scardtool --help' for available commands.\n";
            return ExitCode::InvalidArgs;
        }

        // Script/interactive modunda CommandContext oluştur
        CommandContext ctx{ app_, parsed, promptFn_ ? promptFn_ : nullptr };
        return cmd->execute(ctx);
    }

    // "cmd1; cmd2; cmd3" → ["cmd1", "cmd2", "cmd3"]
    static std::vector<std::string> splitSemicolon(const std::string& line) {
        std::vector<std::string> parts;
        std::stringstream ss(line);
        std::string part;
        bool inQuote = false;
        char qc = 0;
        std::string cur;

        for (char c : line) {
            if (inQuote) {
                if (c == qc) inQuote = false;
                else cur += c;
            } else if (c == '"' || c == '\'') {
                inQuote = true; qc = c;
            } else if (c == ';') {
                parts.push_back(cur); cur.clear();
            } else {
                cur += c;
            }
        }
        parts.push_back(cur);
        return parts;
    }

    static std::string trim(const std::string& s) {
        auto st = s.find_first_not_of(" \t\r\n");
        if (st == std::string::npos) return "";
        auto en = s.find_last_not_of(" \t\r\n");
        return s.substr(st, en - st + 1);
    }
};
