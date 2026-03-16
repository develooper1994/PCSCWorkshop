#include "ExitCode.h"
#include "App/App.h"
#include "ArgParser/ArgParser.h"
#include "EnvVars.h"
#include "Commands/CommandRegistry.h"
#include "Script/ScriptRunner.h"
#include "Modes/InteractiveMode.h"
#include "Modes/McpMode.h"
#include "Crypto/CardCrypto.h"
#include <iostream>
#include <string>
#include <filesystem>
#include <optional>
#include <algorithm>
#ifdef _WIN32
#  include <windows.h>
#  include <io.h>
#else
#  include <termios.h>
#  include <unistd.h>
#endif

// ════════════════════════════════════════════════════════════════════════════════
// Version
// ════════════════════════════════════════════════════════════════════════════════

static constexpr const char* VERSION = "1.0.0";

static std::string buildInfo() {
    return std::string("scardtool ") + VERSION +
           " (C++17"
#ifdef SCARDTOOL_USE_LINENOISE
           ", linenoise-ng"
#else
           ", getline fallback"
#endif
           ")";
}

// ════════════════════════════════════════════════════════════════════════════════
// Global argüman tanımları
// ════════════════════════════════════════════════════════════════════════════════

static const std::vector<ArgDef> GLOBAL_DEFS = {
    {"json",         'j', false},
    {"verbose",      'v', false},
    {"dry-run",      0,   false},
    {"session",      'S', false},
    {"save-session", 0,   false},
    {"help",         'h', false},
    {"version",      'V', false},
    {"mcp",          'm', false},
    {"socket",       'K', true },
    {"script",       0,   false},
    {"exec",         'e', true },
    {"file",         'f', true },
    // ── Şifreleme ─────────────────────────────────────────────────────
    {"encrypt",      'E', false},  // şifrelemeyi aktif et
    {"cipher",       0,   true },  // algoritma: detect|aes-ctr|aes-cbc|aes-gcm|3des-cbc|xor|none
    {"password",     'P', true },  // parola (CLI — geçmiş dosyasına yazılır, dikkatli kullan)
    {"password-prompt", 'W', false}, // parola terminalde echo-off ile sor (güvenli)
    {"no-warn",      'q', false},  // uyarıları bastır
};

// ════════════════════════════════════════════════════════════════════════════════
// argv[0] busybox dispatch
//
// "scardtool"        → normal mod
// "list-readers"     → scardtool list-readers
// "scardtool-uid"    → scardtool uid
// "send-apdu"        → scardtool send-apdu
// ════════════════════════════════════════════════════════════════════════════════

static std::optional<std::string> extractSymlinkSubcmd(const std::string& argv0) {
    // Binary adını al (path'siz)
    std::string base = std::filesystem::path(argv0).stem().string();

    // "scardtool" → normal
    if (base == "scardtool") return std::nullopt;

    // "scardtool-list-readers" → "list-readers"
    static const std::string prefix = "scardtool-";
    if (base.size() > prefix.size() &&
        base.substr(0, prefix.size()) == prefix) {
        return base.substr(prefix.size());
    }

    // Doğrudan "list-readers", "send-apdu" vb.
    return base;
}

// ════════════════════════════════════════════════════════════════════════════════
// main
// ════════════════════════════════════════════════════════════════════════════════

int main(int argc, char* argv[]) {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    CommandRegistry registry;

    // ── argv[0] busybox dispatch ─────────────────────────────────────────────
    auto symlinkCmd = extractSymlinkSubcmd(argv[0]);
    if (symlinkCmd) {
        // Geçerli bir komut mu? Yoksa normal moda geç.
        if (!registry.find(*symlinkCmd)) {
            symlinkCmd = std::nullopt; // bilinmeyen symlink adı → normal mod
        }
    }
    if (symlinkCmd) {
        // argv = ["uid", "-r", "0"]
        // → ["scardtool", "uid", "-r", "0"]
        std::vector<std::string> newArgs;
        newArgs.push_back("scardtool");
        newArgs.push_back(*symlinkCmd);
        for (int i = 1; i < argc; ++i) newArgs.push_back(argv[i]);

        std::vector<char*> newArgv;
        for (auto& s : newArgs) newArgv.push_back(const_cast<char*>(s.c_str()));

        return main(static_cast<int>(newArgv.size()), newArgv.data());
    }

    // ── Global + subcommand argümanlarını toplu parse et ─────────────────────
    // Önce subcommand'ı bul (GLOBAL_DEFS'e göre option value'ları atlayarak)
    std::vector<ArgDef> allDefs = GLOBAL_DEFS;
    {
        int i = 1;
        while (i < argc) {
            std::string tok = argv[i];
            if (tok == "--") { ++i; break; }

            if (!tok.empty() && tok[0] == '-') {
                // Long option: --foo veya --foo=val
                if (tok.size() > 1 && tok[1] == '-') {
                    auto eq = tok.find('=', 2);
                    std::string name = (eq != std::string::npos)
                                       ? tok.substr(2, eq-2) : tok.substr(2);
                    bool hasValue = (eq != std::string::npos);
                    if (!hasValue) {
                        // takesValue ise sonraki token value'dur, atla
                        for (auto& d : GLOBAL_DEFS) {
                            if (d.longName == name && d.takesValue) { ++i; break; }
                        }
                    }
                } else {
                    // Short option(s): -jvr veya -P pass
                    for (size_t pos = 1; pos < tok.size(); ++pos) {
                        char sc = tok[pos];
                        for (auto& d : GLOBAL_DEFS) {
                            if (d.shortName == sc && d.takesValue) {
                                if (pos + 1 >= tok.size()) ++i; // -P pass → skip "pass"
                                goto next_token; // multi-char sonraki yok
                            }
                        }
                    }
                    next_token:;
                }
                ++i;
                continue;
            }

            // Positional → subcommand
            if (auto* cmd = registry.find(tok)) {
                for (auto& d : cmd->argDefs()) {
                    bool dup = false;
                    for (auto& g : allDefs)
                        if (g.longName == d.longName) { dup = true; break; }
                    if (!dup) allDefs.push_back(d);
                }
            }
            break;
        }
    }

    ParsedArgs args;
    try {
        args = ArgParser::parse(argc, argv, allDefs);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n"
                  << "Run 'scardtool --help' for usage.\n";
        return toInt(ExitCode::InvalidArgs);
    }

    // ── --version ────────────────────────────────────────────────────────────
    if (args.has("version")) {
        std::cout << buildInfo() << "\n";
        return 0;
    }

    // ── App + Formatter init ──────────────────────────────────────────────────
    // Öncelik: CLI arg > Env variable > Session > Interactive
    App app;

    // ── Bool flagler: CLI || Env ──────────────────────────────────────────────
    app.flags.jsonOutput    = args.has("json")    || EnvVars::getBool(EnvVar::JSON);
    app.flags.verbose       = args.has("verbose") || EnvVars::getBool(EnvVar::VERBOSE);
    app.flags.dryRun        = args.has("dry-run") || EnvVars::getBool(EnvVar::DRY_RUN);
    app.flags.sessionActive = args.has("session");
    app.flags.noWarn        = args.has("no-warn") || EnvVars::getBool(EnvVar::NO_WARN);
    app.flags.encrypt       = args.has("encrypt") || EnvVars::getBool(EnvVar::ENCRYPT);

    // ── cipher: CLI > Env > (session'da sonra) ────────────────────────────────
    {
        std::optional<std::string> cipherStr = args.get("cipher");
        if (!cipherStr) cipherStr = EnvVars::get(EnvVar::CIPHER);
        if (cipherStr) {
            if (!CardCrypto::isValidString(*cipherStr)) {
                std::cerr << "Error: Unknown cipher '" << *cipherStr
                          << "'. Valid options: "
                          << "none, detect, aes-ctr, aes-cbc, aes-gcm, 3des-cbc, xor\n";
                return toInt(ExitCode::InvalidArgs);
            }
            app.flags.cipher = CardCrypto::fromString(*cipherStr);
        }
    }

    // ── password: CLI > --prompt > Env > (session'da sonra) ─────────────────
    {
        bool fromCLI = args.get("password").has_value();
        if (fromCLI) {
            app.flags.password = *args.get("password");
            // Uyarı: CLI'da parola shell history'e yazılabilir
            if (!app.flags.noWarn) {
                std::cerr << "Warning: --password on command line may appear in "
                             "shell history.\n"
                          << "         Safer alternatives:\n"
                          << "           -W / --password-prompt  (echo-off terminal prompt)\n"
                          << "           export SCARDTOOL_PASSWORD='...'  (env variable)\n";
            }
        } else if (args.has("password-prompt")) {
            // Echo-off terminal prompt — en güvenli interaktif yöntem
#ifdef _WIN32
            HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
            DWORD mode = 0;
            GetConsoleMode(hStdin, &mode);
            SetConsoleMode(hStdin, mode & ~ENABLE_ECHO_INPUT);
            std::cerr << "Password: ";
            std::getline(std::cin, app.flags.password);
            SetConsoleMode(hStdin, mode);
            std::cerr << "\n";
#else
            // POSIX: termios ile echo kapat
            struct termios oldt, newt;
            tcgetattr(STDIN_FILENO, &oldt);
            newt = oldt;
            newt.c_lflag &= static_cast<tcflag_t>(~ECHO);
            tcsetattr(STDIN_FILENO, TCSANOW, &newt);
            std::cerr << "Password: ";
            std::getline(std::cin, app.flags.password);
            tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
            std::cerr << "\n";
#endif
        } else if (auto pw = EnvVars::get(EnvVar::PASSWORD)) {
            app.flags.password = *pw;
            // SCARDTOOL_PASSWORD env → güvenli (history'de görünmez), uyarı yok
        }
        // Session fallback sonraki adımda
    }

    app.initFormatter();

    // ── Verbose: aktif env değişkenlerini logla ───────────────────────────────
    if (app.flags.verbose) {
        app.fmt.verbose("scardtool " + std::string(VERSION));
        EnvVars::printActive();
    }

    // ── Session yükle ─────────────────────────────────────────────────────────
    if (app.flags.sessionActive) {
        if (app.session.load()) {
            app.fmt.verbose("Session loaded from: " + app.session.loadedPath());

            // Password: interactive > CLI > Env > Session (session en düşük öncelik)
            if (app.flags.password.empty()) {
                if (auto pw = app.session.get("password")) {
                    app.flags.password = *pw;
                    // Session'da parola düz metin olarak saklıdır
                    if (!app.flags.noWarn) {
                        std::cerr << "Warning: Password loaded from session file (stored as plain text).\n"
                                  << "         Safer: use -W (prompt) or SCARDTOOL_PASSWORD env variable.\n"
                                  << "         Use -q to suppress this warning.\n";
                    }
                }
            }

            // Cipher: CLI > Env > Session
            bool cipherFromCLIorEnv = args.has("cipher") || EnvVars::get(EnvVar::CIPHER).has_value();
            if (!cipherFromCLIorEnv) {
                if (auto cs = app.session.get("cipher")) {
                    if (CardCrypto::isValidString(*cs))
                        app.flags.cipher = CardCrypto::fromString(*cs);
                }
            } else if (!app.flags.noWarn) {
                if (auto sc = app.session.get("cipher")) {
                    if (*sc != CardCrypto::toString(app.flags.cipher))
                        std::cerr << "Warning: session cipher (" << *sc
                                  << ") overridden by "
                                  << (args.has("cipher") ? "--cipher" : "SCARDTOOL_CIPHER")
                                  << " (" << CardCrypto::toString(app.flags.cipher) << ")\n";
                }
            }
        } else {
            app.fmt.verbose("No session file found (.scardtool_session).");
        }
    }

    // ── stdin durumunu erken belirle (help kontrolü buna bağlı) ─────────────
#ifdef _WIN32
    bool stdinIsTerminal = _isatty(0);
#else
    bool stdinIsTerminal = isatty(STDIN_FILENO);
#endif

    // ── --help ───────────────────────────────────────────────────────────────
    if (args.has("help") || (args.subcommand.empty() &&
        !args.has("mcp") && !args.has("exec") && !args.has("file") &&
        !args.has("script") && stdinIsTerminal)) {
        registry.printHelp(buildInfo());
        return 0;
    }

    // ── --mcp: MCP server modu ────────────────────────────────────────────────
    if (args.has("mcp") || args.has("script")) {
        std::optional<int> port;
        if (auto p = args.get("socket")) {
            try { port = std::stoi(*p); }
            catch (...) {
                std::cerr << "Error: Invalid port: " << *p << "\n";
                return toInt(ExitCode::InvalidArgs);
            }
        }
        McpMode mcp(app);
        return toInt(mcp.run(port));
    }

    // ── stdin pipe/redirect: script modu ─────────────────────────────────────
    if (!stdinIsTerminal && args.subcommand.empty() &&
        !args.has("exec") && !args.has("file")) {
        ScriptRunner runner(registry, app);
        return toInt(runner.runStdin());
    }

    // ── -e / --exec: inline komutlar ─────────────────────────────────────────
    if (auto exec = args.get("exec")) {
        ScriptRunner runner(registry, app);
        auto ec = runner.runInline(*exec);
        if (args.has("save-session")) app.session.save();
        return toInt(ec);
    }

    // ── -f / --file: script dosyası ──────────────────────────────────────────
    if (auto file = args.get("file")) {
        ScriptRunner runner(registry, app);
        auto ec = runner.runFile(*file);
        if (args.has("save-session")) app.session.save();
        return toInt(ec);
    }

    // ── interactive: interaktif shell ────────────────────────────────────────
    if (args.subcommand == "interactive") {
        InteractiveMode im(registry, app);
        return toInt(im.run());
    }

    // ── Terminal modu: tek subcommand ─────────────────────────────────────────
    if (args.subcommand.empty()) {
        registry.printHelp(buildInfo());
        return 0;
    }

    // help <cmd>
    if (args.subcommand == "help") {
        if (!args.positional.empty()) registry.printCommandHelp(args.positional[0]);
        else                          registry.printHelp(buildInfo());
        return 0;
    }

    auto* cmd = registry.find(args.subcommand);
    if (!cmd) {
        std::cerr << "Error: Unknown command '" << args.subcommand
                  << "'. Run 'scardtool --help' for available commands.\n";
        return toInt(ExitCode::InvalidArgs);
    }

    CommandContext ctx{ app, args, nullptr }; // terminal → no interactive prompt
    auto ec = cmd->execute(ctx);

    // --save-session
    if (ec == ExitCode::Success && args.has("save-session")) {
        // Kullanılan parametreleri session'a yaz
        for (auto& [k, v] : args.opts) app.session.set(k, v);
        app.session.save();
    }

    return toInt(ec);
}
