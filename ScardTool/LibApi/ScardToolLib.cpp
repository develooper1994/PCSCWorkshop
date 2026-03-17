/** @file ScardToolLib.cpp
 * @brief scardtool C API implementasyonu.
 *
 * scardtool_ctx → App + CommandRegistry bağlar.
 * Lua/Tcl extension'lar bu implementasyonu kullanır.
 */
#include "ScardToolLib.h"
#include "App/App.h"
#include "Commands/CommandRegistry.h"
#include "Script/ScriptRunner.h"
#include "ExitCode.h"

#include <sstream>
#include <cstring>
#include <cstdio>

// ─────────────────────────────────────────────────────────────────────────────
// Opaque context tanımı
// ─────────────────────────────────────────────────────────────────────────────

struct scardtool_ctx {
    App             app;
    CommandRegistry registry;
    std::string     lastError;
    std::string     readerStr;   // index veya isim
    int             readerIndex = 0;

    scardtool_ctx() {
        // CommandRegistry constructor'ı registerAll() çağırır
        // tüm komutlar otomatik register edilir
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Lifecycle
// ─────────────────────────────────────────────────────────────────────────────

scardtool_ctx* scardtool_create() {
    try {
        return new scardtool_ctx();
    } catch (...) {
        return nullptr;
    }
}

void scardtool_destroy(scardtool_ctx* ctx) {
    delete ctx;
}

// ─────────────────────────────────────────────────────────────────────────────
// Konfigürasyon
// ─────────────────────────────────────────────────────────────────────────────

void scardtool_set_reader(scardtool_ctx* ctx, int index) {
    if (!ctx) return;
    ctx->readerIndex = index;
    ctx->readerStr   = std::to_string(index);
}

void scardtool_set_reader_name(scardtool_ctx* ctx, const char* name) {
    if (!ctx || !name) return;
    ctx->readerStr = name;
}

void scardtool_set_json(scardtool_ctx* ctx, int enabled) {
    if (!ctx) return;
    ctx->app.flags.jsonOutput = (enabled != 0);
    ctx->app.initFormatter();
}

void scardtool_set_verbose(scardtool_ctx* ctx, int enabled) {
    if (!ctx) return;
    ctx->app.flags.verbose = (enabled != 0);
    ctx->app.initFormatter();
}

void scardtool_set_dry_run(scardtool_ctx* ctx, int enabled) {
    if (!ctx) return;
    ctx->app.flags.dryRun = (enabled != 0);
}

void scardtool_set_encrypt(scardtool_ctx* ctx, int enabled,
                            const char* cipher, const char* password) {
    if (!ctx) return;
    ctx->app.flags.encrypt = (enabled != 0);
    if (cipher){   /* parse cipher algo string */}
    if (password) ctx->app.flags.password = password;
}

// ─────────────────────────────────────────────────────────────────────────────
// Komut çalıştırma
// ─────────────────────────────────────────────────────────────────────────────

/** @brief Tek komut çalıştır, stdout'u yakala.
 *
 * ScriptRunner üzerinden çalıştırır; mevcut ArgParser API'si ile uyumlu.
 * "-r <reader>" otomatik enjekte edilir.
 */
static int run_command(scardtool_ctx* ctx,
                       const std::string& cmdline,
                       char* out_buf, size_t out_len) {
    // "-r <reader>" otomatik enjeksiyonu (yoksa)
    std::string full = cmdline;
    if (!ctx->readerStr.empty() &&
        full.find("-r ") == std::string::npos &&
        full.find("--reader ") == std::string::npos) {
        auto sp = full.find(' ');
        if (sp != std::string::npos)
            full = full.substr(0, sp) + " -r " + ctx->readerStr + full.substr(sp);
        else
            full += " -r " + ctx->readerStr;
    }

    // Çıktıyı yakala
    std::ostringstream capture;
    std::streambuf* saved = nullptr;
    if (out_buf && out_len > 0)
        saved = std::cout.rdbuf(capture.rdbuf());

    ExitCode ec = ExitCode::Success;
    try {
        ScriptRunner runner(ctx->registry, ctx->app);
        ec = runner.runLine(full);
    } catch (const std::exception& e) {
        ctx->lastError = e.what();
        ec = ExitCode::GeneralError;
    }

    if (saved) {
        std::cout.rdbuf(saved);
        auto s = capture.str();
        if (out_len > 1) {
            std::strncpy(out_buf, s.c_str(), out_len - 1);
            out_buf[out_len - 1] = '\0';
        }
    }
    return static_cast<int>(ec);
}

int scardtool_exec(scardtool_ctx* ctx,
                   const char* cmdline,
                   char* out_buf, size_t out_len) {
    if (!ctx || !cmdline) return static_cast<int>(ExitCode::InvalidArgs);
    return run_command(ctx, cmdline, out_buf, out_len);
}

int scardtool_run_file(scardtool_ctx* ctx, const char* path) {
    if (!ctx || !path) return static_cast<int>(ExitCode::InvalidArgs);
    ScriptRunner runner(ctx->registry, ctx->app);
    auto ec = runner.runFile(path);
    return static_cast<int>(ec);
}

int scardtool_run_string(scardtool_ctx* ctx, const char* script) {
    if (!ctx || !script) return static_cast<int>(ExitCode::InvalidArgs);
    ScriptRunner runner(ctx->registry, ctx->app);
    auto ec = runner.runString(script);
    return static_cast<int>(ec);
}

int scardtool_send_apdu(scardtool_ctx* ctx,
                         const char* apdu_hex,
                         char* resp_buf, size_t resp_len) {
    if (!ctx || !apdu_hex) return -1;
    std::string cmd = std::string("send-apdu -a ") + apdu_hex;
    return run_command(ctx, cmd, resp_buf, resp_len);
}

const char* scardtool_last_error(scardtool_ctx* ctx) {
    if (!ctx) return "null context";
    return ctx->lastError.c_str();
}

const char* scardtool_version(void) {
    return "scardtool v7 (" __DATE__ ")";
}

int scardtool_list_readers(scardtool_ctx* ctx,
                             char names[][256], int max) {
    if (!ctx || !names || max <= 0) return -1;
    try {
        if (!ctx->app.pcsc.hasContext())
            ctx->app.pcsc.establishContext();
        auto result = ctx->app.pcsc.listReaders();
        if (!result.ok) {
            ctx->lastError = "listReaders failed";
            return -1;
        }
        int n = 0;
        for (auto& r : result.names) {
            if (n >= max) break;
            std::string utf8 = App::wideToUtf8(r);
            std::strncpy(names[n], utf8.c_str(), 255);
            names[n][255] = '\0';
            ++n;
        }
        return n;
    } catch (const std::exception& e) {
        ctx->lastError = e.what();
        return -1;
    }
}
