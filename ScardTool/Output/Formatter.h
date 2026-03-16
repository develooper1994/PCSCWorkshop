/** @file Formatter.h
 * @brief stdout/stderr separated output (human-readable + JSON). */
#pragma once

// ════════════════════════════════════════════════════════════════════════════════
// TODO: Colored output (--color flag)
//
// When implemented:
//   Errors    → Red    (ANSI \033[31m)
//   Warnings  → Yellow (ANSI \033[33m)
//   Info/OK   → Green  (ANSI \033[32m)
//   Debug     → Gray   (ANSI \033[90m)  logcat-style: "D/tag: message"
//
// Global flags to add:
//   --color        Enable colored output (default: off)
//   --no-color     Explicit disable
//
// Platform note:
//   Windows: use SetConsoleTextAttribute() or enable ANSI via
//            SetConsoleMode(ENABLE_VIRTUAL_TERMINAL_PROCESSING)
//   Linux/macOS: ANSI escape codes directly
//
// isatty() check: never color when piped (stdout is not a terminal)
// ════════════════════════════════════════════════════════════════════════════════

#include <nlohmann/json.hpp>
#include <iostream>
#include <string>
#include <sstream>
#include <iomanip>

using json = nlohmann::json;

// ════════════════════════════════════════════════════════════════════════════════
// Formatter — stdout/stderr ayrımlı çıktı yönetimi
//
// Kurallar:
//   - Başarılı sonuç  → stdout
//   - Hata / uyarı    → stderr
//   - --json flag     → stdout'a JSON, stderr'e yine plain text
//
// Kullanım:
//   Formatter fmt(jsonMode);
//   fmt.success("Connected to ACR1281U");
//   fmt.error("No readers found");
//   fmt.json({ {"readers", arr} });
// ════════════════════════════════════════════════════════════════════════════════

class Formatter {
public:
    explicit Formatter(bool jsonMode = false, bool verbose = false, bool noWarn = false)
        : jsonMode_(jsonMode), verbose_(verbose), noWarn_(noWarn) {}

    // ── Başarılı çıktı (stdout) ──────────────────────────────────────────────
    void success(const std::string& msg) const {
        if (!jsonMode_) std::cout << msg << "\n";
    }

    void json(const ::json& j) const {
        std::cout << j.dump(2) << "\n";
    }

    // Ham key-value tablo çıktısı (human) veya JSON
    void result(const ::json& j) const {
        if (jsonMode_) {
            std::cout << j.dump(2) << "\n";
        } else {
            printHuman(j, 0);
        }
    }

    // ── Hata / uyarı (stderr) ────────────────────────────────────────────────
    void error(const std::string& msg) const {
        std::cerr << "Error: " << msg << "\n";
    }

    void warn(const std::string& msg) const {
        if (!noWarn_) std::cerr << "Warning: " << msg << "\n";
    }

    void verbose(const std::string& msg) const {
        if (verbose_) std::cerr << "[verbose] " << msg << "\n";
    }

    // ── Dry-run bildirimi ────────────────────────────────────────────────────
    void dryRun(const std::string& action) const {
        std::cout << "[dry-run] Would execute: " << action << "\n";
    }

    bool isJson()    const { return jsonMode_; }
    bool isVerbose() const { return verbose_; }

private:
    bool jsonMode_;
    bool verbose_;
    bool noWarn_ = false;

    // Basit recursive human-readable JSON printer (tablo formatı)
    static void printHuman(const ::json& j, int indent) {
        std::string pad(indent * 2, ' ');
        if (j.is_object()) {
            for (auto& [k, v] : j.items()) {
                if (v.is_object() || v.is_array()) {
                    std::cout << pad << k << ":\n";
                    printHuman(v, indent + 1);
                } else {
                    std::cout << pad << std::left << std::setw(20 - indent*2)
                              << (k + ":") << " " << valueToStr(v) << "\n";
                }
            }
        } else if (j.is_array()) {
            int idx = 0;
            for (auto& item : j) {
                std::cout << pad << "[" << idx++ << "] ";
                if (item.is_object() || item.is_array()) {
                    std::cout << "\n";
                    printHuman(item, indent + 1);
                } else {
                    std::cout << valueToStr(item) << "\n";
                }
            }
        } else {
            std::cout << pad << valueToStr(j) << "\n";
        }
    }

    static std::string valueToStr(const ::json& v) {
        if (v.is_string()) return v.get<std::string>();
        return v.dump();
    }
};
