/** @file InteractiveMode.h
 * @brief Persistent interactive shell with readline/linenoise and script recording. */
#pragma once
#include "Commands/CommandRegistry.h"
#include "Script/ScriptRunner.h"
#include "LineEditor/ILineEditor.h"
#include "App/App.h"
#include "ExitCode.h"
#include <memory>
#include <string>
#include <vector>
#include <fstream>
#include <optional>
#include <iostream>
#include <sstream>

// ════════════════════════════════════════════════════════════════════════════════
// InteractiveMode — persistent process shell
//
// - readline / linenoise ile history (up/down arrow) desteği — TODO: dosyaya kayıt
// - Tab completion: tüm subcommand isimleri
// - Record/stop-record: komutları .scard dosyasına kaydeder
// - Prompt: "scardtool> "
// - Çıkış: quit / exit / Ctrl+D
//
// Builtin komutlar (sadece interactive):
//   help [<cmd>]                  — yardım
//   record [file.scard]           — komutları kaydetmeye başla
//   stop-record                   — kaydı durdur ve yaz
//   run-script <file.scard>       — script dosyası çalıştır
//   quit / exit                   — çıkış
// ════════════════════════════════════════════════════════════════════════════════

class InteractiveMode {
public:
    InteractiveMode(CommandRegistry& registry, App& app)
        : registry_(registry), app_(app)
    {
        editor_ = makeLineEditor(registry.names());

        // History limit — SCARDTOOL_HISTSIZE env veya varsayılan 1000
        // Bash'te HISTSIZE (bellekte) ve HISTFILESIZE (dosyada) ayırımı vardır.
        // Biz basitleştirip SCARDTOOL_HISTSIZE her ikisi için kullanıyoruz.
        int histLimit = 1000; // varsayılan
        if (const char* hs = std::getenv("SCARDTOOL_HISTSIZE")) {
            try { histLimit = std::stoi(hs); } catch (...) {}
        }
        editor_->setHistoryLimit(histLimit);

        // History dosyasından yükle
        histPath_ = historyPath();
        editor_->loadHistory(histPath_);
    }

    ExitCode run() {
        printWelcome();

        while (true) {
            auto line = editor_->readline(prompt());
            if (!line) break; // EOF / Ctrl+D

            auto trimmed = trim(*line);
            if (trimmed.empty()) continue;

            if (trimmed != lastLine_) {
                editor_->addHistory(trimmed);
                lastLine_ = trimmed;
            }

            if (recording_ && trimmed != "stop-record") {
                recordBuffer_.push_back(trimmed);
            }

            if (trimmed == "quit" || trimmed == "exit") break;

            if (handleBuiltin(trimmed)) continue;

            ScriptRunner runner(registry_, app_, false);
            runner.setPromptFn([this](const std::string& msg) -> std::string {
                std::cout << msg;
                std::cout.flush();
                auto line = editor_->readline("");
                return line ? *line : "";
            });
            runner.runLine(trimmed);
        }

        // History kaydet
        if (!histPath_.empty()) {
            editor_->saveHistory(histPath_);
        }
        if (recording_) stopRecord();

        return ExitCode::Success;
    }

private:
    CommandRegistry&             registry_;
    App&                         app_;
    std::unique_ptr<ILineEditor> editor_;
    std::string                  lastLine_;
    std::string                  histPath_;

    // Recording
    bool                       recording_ = false;
    std::string                recordFile_;
    std::vector<std::string>   recordBuffer_;

    std::string prompt() const {
        if (app_.pcsc.isConnected())
            return "scardtool(connected)> ";
        return "scardtool> ";
    }

    void printWelcome() const {
        const char* hs = std::getenv("SCARDTOOL_HISTSIZE");
        int limit = 1000;
        if (hs) { try { limit = std::stoi(hs); } catch (...) {} }
        std::cout <<
"scardtool interactive mode\n"
"Type 'help' for commands, 'quit' to exit.\n"
"Use up/down arrows for history";
        std::cout << " (limit: " << (limit < 0 ? "unlimited" : std::to_string(limit)) << ")\n";
        std::cout << "History: " << histPath_ << "\n\n";
    }

    // Builtin komutları işle. True → handled, False → pass to ScriptRunner
    bool handleBuiltin(const std::string& line) {
        auto tokens = ArgParser::tokenize(line);
        if (tokens.empty()) return true;
        const auto& cmd = tokens[0];

        if (cmd == "help") {
            if (tokens.size() > 1) registry_.printCommandHelp(tokens[1]);
            else                   registry_.printHelp("interactive");
            return true;
        }

        if (cmd == "record") {
            std::string file = tokens.size() > 1 ? tokens[1] : "session.scard";
            startRecord(file);
            return true;
        }

        if (cmd == "stop-record") {
            stopRecord();
            return true;
        }

        if (cmd == "run-script") {
            if (tokens.size() < 2) {
                std::cerr << "Usage: run-script <file.scard>\n";
                return true;
            }
            ScriptRunner runner(registry_, app_);
            runner.runFile(tokens[1]);
            return true;
        }

        return false;
    }

    void startRecord(const std::string& file) {
        if (recording_) {
            std::cout << "Already recording to: " << recordFile_ << "\n";
            return;
        }
        recording_   = true;
        recordFile_  = file;
        recordBuffer_.clear();
        std::cout << "Recording started → " << file << "\n"
                  << "Type 'stop-record' to save.\n";
    }

    void stopRecord() {
        if (!recording_) {
            std::cout << "Not recording.\n";
            return;
        }
        recording_ = false;

        std::ofstream f(recordFile_, std::ios::trunc);
        if (!f.is_open()) {
            std::cerr << "Error: Cannot write: " << recordFile_ << "\n";
            return;
        }
        f << "#!/usr/bin/env scardtool --script\n";
        f << "# Recorded by scardtool interactive\n";
        for (auto& line : recordBuffer_) f << line << "\n";
        std::cout << "Script saved: " << recordFile_
                  << " (" << recordBuffer_.size() << " commands)\n";
        recordBuffer_.clear();
    }

    // Platform-specific history dosya yolu
    static std::string historyPath() {
        const char* home =
#ifdef _WIN32
            std::getenv("USERPROFILE");
#else
            std::getenv("HOME");
#endif
        if (!home) home = ".";
        return std::string(home) + "/.scardtool_history";
    }

    static std::string trim(const std::string& s) {
        auto st = s.find_first_not_of(" \t\r\n");
        if (st == std::string::npos) return "";
        auto en = s.find_last_not_of(" \t\r\n");
        return s.substr(st, en - st + 1);
    }
};
