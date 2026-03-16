#pragma once
#include "PCSC.h"
#include "Readers.h"
#include <memory>
#include <mutex>
#include <optional>
#include <string>

namespace mcp {

// ════════════════════════════════════════════════════════════════════════════════
// PcscSession
//
// MCP server process boyunca tek bir PCSC context + bağlantı yönetir.
// Tool handler'ları bu singleton üzerinden okuma/yazma yapar.
// Mutex ile thread-safe — ileride async transport'a geçilirse korunur.
// ════════════════════════════════════════════════════════════════════════════════
class PcscSession {
public:
    static PcscSession& instance() {
        static PcscSession s;
        return s;
    }

    PcscSession(const PcscSession&) = delete;
    PcscSession& operator=(const PcscSession&) = delete;

    std::mutex& mutex() { return mutex_; }

    PCSC& pcsc() { return pcsc_; }
    const PCSC& pcsc() const { return pcsc_; }

    bool isConnected() const { return pcsc_.isConnected(); }
    bool hasContext()  const { return pcsc_.hasContext();  }

    // Seçili reader adı (listReaders sonrası set edilir)
    const std::wstring& selectedReader() const { return pcsc_.readerName(); }

private:
    PcscSession() = default;

    mutable std::mutex mutex_;
    PCSC               pcsc_;
};

} // namespace mcp
