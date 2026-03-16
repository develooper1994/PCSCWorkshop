/** @file ILineEditor.h
 * @brief Line editor abstraction (ILineEditor, GetlineEditor, LinenoisEditor). */
#pragma once
#include <string>
#include <optional>
#include <vector>
#include <iostream>
#include <fstream>

// ════════════════════════════════════════════════════════════════════════════════
// ILineEditor — line editing soyutlama arayüzü
//
// Implementasyonlar:
//   GetlineEditor    → stdlib fallback, pipe/script modunda, her platformda çalışır
//   LinenoisEditor   → linenoise-ng, history + completion (SCARDTOOL_USE_LINENOISE=ON)
//
// isatty kontrolü otomatik yapılır:
//   - stdin terminal   → readline prompt gösterir
//   - stdin pipe/dosya → prompt bastırmaz, düz okur (script-friendly)
// ════════════════════════════════════════════════════════════════════════════════

class ILineEditor {
public:
    virtual ~ILineEditor() = default;

    // Bir satır oku. EOF / Ctrl+D gelirse nullopt döner.
    virtual std::optional<std::string> readline(const std::string& prompt) = 0;

    // History
    virtual void addHistory(const std::string& line) = 0;
    virtual void saveHistory(const std::string& path) = 0;
    virtual void loadHistory(const std::string& path) = 0;
    /** @brief History satır limitini ayarla (bash HISTSIZE benzeri).
     *  Varsayılan: 1000. 0 = sınırsız. */
    virtual void setHistoryLimit(int limit) = 0;

    // Tab completion için aday ekle
    virtual void addCompletion(const std::string& word) = 0;

    // stdin terminal mi? (false → script/pipe modu)
    virtual bool isInteractive() const = 0;
};

// ── GetlineEditor — stdlib fallback ──────────────────────────────────────────
// Hiçbir dış bağımlılık gerektirmez.
// History: in-memory vector (TODO: kalıcı kayıt).
// Tab completion: yok.

#ifdef _WIN32
#  include <io.h>
#  define ISATTY(fd) _isatty(fd)
#  define STDIN_FD   0
#else
#  include <unistd.h>
#  define ISATTY(fd) isatty(fd)
#  define STDIN_FD   STDIN_FILENO
#endif

class GetlineEditor final : public ILineEditor {
public:
    GetlineEditor() : interactive_(ISATTY(STDIN_FD) != 0) {}

    std::optional<std::string> readline(const std::string& prompt) override {
        if (interactive_) {
            std::cout << prompt << std::flush;
        }
        std::string line;
        if (!std::getline(std::cin, line)) return std::nullopt;
        return line;
    }

    void addHistory(const std::string& line) override {
        if (!line.empty()) history_.push_back(line);
    }

    // TODO: history kalıcı kayıt (linenoise ile otomatik gelir)
    void saveHistory(const std::string& /*path*/) override {}
    void loadHistory(const std::string& /*path*/) override {}
    void setHistoryLimit(int /*limit*/) override {} // getline: no-op
    void addCompletion(const std::string& word) override {
        completions_.push_back(word);
    }

    bool isInteractive() const override { return interactive_; }

private:
    bool interactive_;
    std::vector<std::string> history_;
    std::vector<std::string> completions_;
};

// ── LinenoisEditor — linenoise-ng wrapper ────────────────────────────────────
// Derleme zamanında SCARDTOOL_USE_LINENOISE tanımlıysa aktif olur.
// Aktif değilse GetlineEditor kullanılır.
// Değişiklik gerekirse sadece bu blok değişir, ILineEditor kullanan
// hiçbir kod dokunulmaz.

#ifdef SCARDTOOL_USE_LINENOISE
#include <linenoise.h>
#include <cstdlib>  // free()

class LinenoisEditor final : public ILineEditor {
public:
    explicit LinenoisEditor(const std::vector<std::string>& completions = {}) {
        linenoiseSetCompletionCallback([](const char* buf, linenoiseCompletions* lc) {
            // statik completion list'e referans için trick
            for (auto& c : s_completions_) {
                if (c.rfind(buf, 0) == 0) // prefix match
                    linenoiseAddCompletion(lc, c.c_str());
            }
        });
        for (auto& c : completions) s_completions_.push_back(c);
    }

    std::optional<std::string> readline(const std::string& prompt) override {
        char* line = ::linenoise(prompt.c_str());
        if (!line) return std::nullopt;
        std::string result(line);
        ::free(line);
        return result;
    }

    void addHistory(const std::string& line) override {
        if (!line.empty()) ::linenoiseHistoryAdd(line.c_str());
    }

    void saveHistory(const std::string& path) override {
        ::linenoiseHistorySave(path.c_str());
    }

    void loadHistory(const std::string& path) override {
        ::linenoiseHistoryLoad(path.c_str());
    }

    /** @brief History satır limitini ayarla.
     *  Bash HISTSIZE/HISTFILESIZE eşdeğeri.
     *  limit=0 → linenoise default (100) kullanılır.
     *  limit=-1 → sınırsız (büyük değer).
     */
    void setHistoryLimit(int limit) override {
        if (limit > 0)
            ::linenoiseHistorySetMaxLen(limit);
        else if (limit < 0)
            ::linenoiseHistorySetMaxLen(100000); // effectively unlimited
        // 0 → default bırak
    }

    void addCompletion(const std::string& word) override {
        s_completions_.push_back(word);
    }

    bool isInteractive() const override {
        return ISATTY(STDIN_FD) != 0;
    }

private:
    // linenoise callback'i statik — completion list'i process-global tutuyoruz
    inline static std::vector<std::string> s_completions_;
};

// Factory: linenoise varsa onu kullan, yoksa getline
inline std::unique_ptr<ILineEditor> makeLineEditor(
    const std::vector<std::string>& completions = {}) {
    return std::make_unique<LinenoisEditor>(completions);
}

#else

inline std::unique_ptr<ILineEditor> makeLineEditor(
    const std::vector<std::string>& /*completions*/ = {}) {
    return std::make_unique<GetlineEditor>();
}

#endif // SCARDTOOL_USE_LINENOISE
