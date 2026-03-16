#pragma once
#include "../ToolRegistry.h"
#include "../PcscSession.h"

namespace mcp::tools {

inline void registerConnect(ToolRegistry& reg) {
    reg.registerTool(
        {
            "connect",
            "Connect to a smart card on the selected reader. "
            "Call list_readers first to get available reader indices.",
            {
                {"type", "object"},
                {"properties", {
                    {"reader_index", {
                        {"type",        "integer"},
                        {"description", "Index from list_readers (default: 0)"},
                        {"default",     0}
                    }},
                    {"retry_ms", {
                        {"type",        "integer"},
                        {"description", "Retry interval in ms when no card present (default: 500)"},
                        {"default",     500}
                    }},
                    {"max_retries", {
                        {"type",        "integer"},
                        {"description", "Max connection attempts, 0 = infinite (default: 3)"},
                        {"default",     3}
                    }}
                }},
                {"required", json::array()}
            }
        },
        [](const json& params) -> json {
            auto& sess = PcscSession::instance();
            std::lock_guard<std::mutex> lock(sess.mutex());
            auto& pcsc = sess.pcsc();

            if (!pcsc.hasContext()) {
                if (!pcsc.establishContext())
                    throw RpcError{ RpcError::INTERNAL_ERROR,
                                    "Failed to establish PC/SC context" };
            }

            size_t readerIndex = params.value("reader_index", 0);
            int    retryMs     = params.value("retry_ms",     500);
            int    maxRetries  = params.value("max_retries",  3);

            // chooseReader: 1-based index (0 = prompt interactively — we skip that)
            if (!pcsc.chooseReader(readerIndex + 1)) {
                throw RpcError{ RpcError::INTERNAL_ERROR,
                                "Reader index out of range: " +
                                std::to_string(readerIndex) };
            }

            if (!pcsc.connectToCard(retryMs, maxRetries)) {
                throw RpcError{ RpcError::INTERNAL_ERROR,
                                "No card detected or connection failed" };
            }

            return {
                {"content", json::array({
                    { {"type", "text"},
                      {"text", json({
                          {"connected", true},
                          {"protocol",  pcsc.protocol()}
                      }).dump(2)} }
                })}
            };
        }
    );
}

} // namespace mcp::tools
