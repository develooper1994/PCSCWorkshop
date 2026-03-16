#pragma once
#include "../ToolRegistry.h"
#include "../PcscSession.h"

namespace mcp::tools {

inline void registerDisconnect(ToolRegistry& reg) {
    reg.registerTool(
        {
            "disconnect",
            "Disconnect from the current smart card and release the PC/SC context.",
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

            pcsc.disconnect();
            pcsc.releaseContext();

            return {
                {"content", json::array({
                    { {"type", "text"},
                      {"text", R"({"disconnected": true})"} }
                })}
            };
        }
    );
}

} // namespace mcp::tools
