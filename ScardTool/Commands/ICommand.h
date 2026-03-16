/** @file ICommand.h
 * @brief Command interface, ParamResolver and CommandContext. */
#pragma once
#include "App/App.h"
#include "ArgParser/ArgParser.h"
#include "EnvVars.h"
#include "ExitCode.h"
#include <string>
#include <vector>
#include <functional>
#include <optional>
#include <iostream>

// ════════════════════════════════════════════════════════════════════════════════
// CommandMode — komutun hangi modda çalışabileceği
// ════════════════════════════════════════════════════════════════════════════════

enum class CommandMode {
    All,          // Terminal + Interactive + MCP
    TerminalOnly, // Sadece terminal / interactive (stateless CLI)
    PersistentOnly, // Interactive + MCP (persistent process gerektirir)
};

// ════════════════════════════════════════════════════════════════════════════════
// ParamDef — komut parametresi tanımı
// ════════════════════════════════════════════════════════════════════════════════

struct ParamDef {
    std::string longName;   // "reader"
    char        shortName;  // 'r'
    std::string description;
    bool        required;
    std::string defaultVal; // required=false ise varsayılan
};

// ════════════════════════════════════════════════════════════════════════════════
// CommandContext — komuta aktarılan çalışma bağlamı
// ════════════════════════════════════════════════════════════════════════════════

struct CommandContext {
    App&        app;
    ParsedArgs  args;
    // Interactive modda kullanıcıdan girdi almak için
    // Terminal modunda nullptr — eksik required param → hata
    std::function<std::string(const std::string& prompt)> promptFn;

    bool isPersistent() const { return promptFn != nullptr; }
};

// ════════════════════════════════════════════════════════════════════════════════
// ════════════════════════════════════════════════════════════════════════════════
// ParamResolver — Öncelik zinciri (yüksekten düşüğe):
//
//   1. Interactive prompt  — kullanıcı o an yazıyor (en yüksek)
//   2. CLI args            — --reader 0, -r 0
//   3. Env variable        — SCARDTOOL_READER=0
//   4. Session file        — .scardtool_session  (--session aktifse)
//
// Mantık: o an aktif olan kullanıcı girişi her zaman kazanır.
// Otomatik kaynaklar (env, session) sadece kullanıcı bir şey vermemişse devreye girer.
// ════════════════════════════════════════════════════════════════════════════════

class ParamResolver {
public:
    explicit ParamResolver(CommandContext& ctx) : ctx_(ctx) {}

    /** @brief Parametre değerini CLI→Env→Session zinciriyle çöz (interactive hariç).
     *  Interactive prompt için require() kullanın.
     */
    std::optional<std::string> resolve(const std::string& longName,
                                       char shortName = 0) const {
        // 2. CLI args
        if (auto v = ctx_.args.get(longName)) return v;
        if (shortName && ctx_.args.has(std::string(1, shortName))) {
            if (auto v = ctx_.args.get(std::string(1, shortName))) return v;
        }
        // 3. Env variable
        if (auto v = EnvVars::forParam(longName)) return v;
        // 4. Session
        if (ctx_.app.flags.sessionActive)
            if (auto v = ctx_.app.session.get(longName)) return v;
        return std::nullopt;
    }

    /** @brief Required parametre:
     *    1. Interactive prompt (CLI'da yoksa, promptFn set edilmişse)
     *    2. CLI → Env → Session
     *    3. Hata
     */
    std::optional<std::string> require(const std::string& longName,
                                       const std::string& promptMsg,
                                       char shortName = 0) const {
        // 1. Interactive prompt — CLI'da verilmemişse sor
        if (ctx_.promptFn) {
            bool fromCLI = ctx_.args.get(longName).has_value()
                || (shortName && ctx_.args.has(std::string(1, shortName)));
            if (!fromCLI) {
                std::string val = ctx_.promptFn(promptMsg);
                if (!val.empty()) return val;
            }
        }
        // 2-4: CLI → Env → Session
        if (auto v = resolve(longName, shortName)) return v;
        // Hata
        std::cerr << "Error: Missing required parameter --" << longName;
        if (shortName) std::cerr << " / -" << shortName;
        if (auto e = envNameFor(longName))
            std::cerr << "\n       (or set " << *e << " env variable)";
        std::cerr << "\n";
        return std::nullopt;
    }

    /** @brief Flag kontrolü (değersiz boolean). */
    bool flag(const std::string& longName, char shortName = 0) const {
        if (ctx_.args.has(longName)) return true;
        if (shortName && ctx_.args.has(std::string(1, shortName))) return true;
        return false;
    }

private:
    CommandContext& ctx_;
    static std::optional<std::string> envNameFor(const std::string& n) {
        if (n == "reader")   return std::string(EnvVar::READER);
        if (n == "password") return std::string(EnvVar::PASSWORD);
        if (n == "cipher")   return std::string(EnvVar::CIPHER);
        if (n == "key")      return std::string(EnvVar::KEY);
        return std::nullopt;
    }
};

// ICommand — komut tabanı
// ════════════════════════════════════════════════════════════════════════════════

class ICommand {
public:
    virtual ~ICommand() = default;

    virtual std::string  name()        const = 0;
    virtual std::string  description() const = 0;
    virtual std::string  usage()       const = 0;
    virtual CommandMode  mode()        const = 0;
    virtual std::vector<ParamDef> params() const = 0;
    virtual std::vector<ArgDef>   argDefs() const = 0;

    virtual ExitCode execute(CommandContext& ctx) = 0;

    std::string helpLine() const {
        return "  " + name() + "\n      " + description();
    }
};
