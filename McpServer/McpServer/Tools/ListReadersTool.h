#pragma once
#include "../ToolRegistry.h"
#include "../PcscSession.h"
#include <string>
#include <codecvt>
#include <locale>

namespace mcp::tools {

// ── wstring → utf8 helper ────────────────────────────────────────────────────
inline std::string wideToUtf8(const std::wstring& ws) {
    if (ws.empty()) return {};
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> conv;
    return conv.to_bytes(ws);
}

// ── Tool ─────────────────────────────────────────────────────────────────────
inline void registerListReaders(ToolRegistry& reg) {
    reg.registerTool(
        {
            "list_readers",
            "List all available PC/SC smart card readers on the system.",
            {
                {"type",       "object"},
                {"properties", json::object()},
                {"required",   json::array()}
            }
        },
        [](const json& /*params*/) -> json {
            auto& sess = PcscSession::instance();
            std::lock_guard<std::mutex> lock(sess.mutex());

            auto& pcsc = sess.pcsc();

            if (!pcsc.hasContext()) {
                if (!pcsc.establishContext())
                    throw RpcError{ RpcError::INTERNAL_ERROR,
                                    "Failed to establish PC/SC context" };
            }

            auto readerList = pcsc.listReaders();
            if (!readerList.ok) {
                throw RpcError{ RpcError::INTERNAL_ERROR,
                                "SCardListReaders failed" };
            }

            json readers = json::array();
            for (size_t i = 0; i < readerList.names.size(); ++i) {
                readers.push_back({
                    {"index", i},
                    {"name",  wideToUtf8(readerList.names[i])}
                });
            }

            return {
                {"content", json::array({
                    { {"type", "text"},
                      {"text", json({ {"readers", readers} }).dump(2)} }
                })}
            };
        }
    );
}

} // namespace mcp::tools
