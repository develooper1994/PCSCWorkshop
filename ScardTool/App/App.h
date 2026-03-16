/** @file App.h
 * @brief Shared application state (PCSC, session, formatter, reader cache). */
#pragma once
#include "PCSC.h"
#include "Session/SessionFile.h"
#include "Output/Formatter.h"
#include "Crypto/CardCrypto.h"
#include <mutex>
#include <string>
#include <vector>
#include <optional>
#include <codecvt>
#include <locale>

// ════════════════════════════════════════════════════════════════════════════════
// App — tüm modlar tarafından paylaşılan merkezi durum
//
// Hem CLI komutları hem MCP tool handler'ları bu struct üzerinden çalışır.
// pcscMutex: MCP server async transport'a geçilirse korunmaya devam eder.
// ════════════════════════════════════════════════════════════════════════════════

struct AppFlags {
    bool jsonOutput    = false;
    bool verbose       = false;
    bool dryRun        = false;
    bool sessionActive = false; ///< --session/-S
    bool noWarn        = false; ///< --no-warn/-q: uyarıları bastır
    // ── Şifreleme ──────────────────────────────────────────────────────
    bool        encrypt  = false;
    CipherAlgo  cipher   = CipherAlgo::Detect;
    std::string password;
};

struct App {
    PCSC           pcsc;
    std::mutex     pcscMutex;
    AppFlags       flags;
    SessionFile    session;
    Formatter      fmt;  // flags sonrası set edilir

    // Cache: son list-readers çağrısından
    std::vector<std::wstring> readers;

    App() = default;

    App(const App&) = delete;
    App& operator=(const App&) = delete;

    // flags set edildikten sonra çağrılmalı
    void initFormatter() {
        fmt = Formatter(flags.jsonOutput, flags.verbose, flags.noWarn);
    }

    // wstring → UTF-8
    static std::string wideToUtf8(const std::wstring& ws) {
        if (ws.empty()) return {};
#ifdef _WIN32
        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> conv;
#else
        std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
#endif
        return conv.to_bytes(ws);
    }

    // reader index veya isim substring'inden PCSC reader seç
    // Döner: seçilen reader adı (wstring) — bulamazsa nullopt
    std::optional<std::wstring> resolveReader(const std::string& indexOrName) {
        // Cache doluysa kullan, yoksa liste al
        if (readers.empty()) {
            if (!pcsc.hasContext()) pcsc.establishContext();
            auto list = pcsc.listReaders();
            if (!list.ok || list.names.empty()) return std::nullopt;
            readers = list.names;
        }

        // Sayı ise index olarak kullan
        try {
            size_t idx = std::stoul(indexOrName);
            if (idx < readers.size()) return readers[idx];
            return std::nullopt;
        } catch (...) {}

        // String ise substring match
        for (auto& r : readers) {
            if (wideToUtf8(r).find(indexOrName) != std::string::npos)
                return r;
        }
        return std::nullopt;
    }
};
