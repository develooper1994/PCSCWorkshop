#pragma once
#include "../ToolRegistry.h"
#include "../PcscSession.h"

#ifdef _WIN32
#  include <winscard.h>
#else
#  include <PCSC/winscard.h>
#  include <PCSC/wintypes.h>
#endif

namespace mcp::tools {

inline void registerTransaction(ToolRegistry& reg) {
    // ── begin_transaction ─────────────────────────────────────────────────────
    reg.registerTool(
        {
            "begin_transaction",
            "Begin an exclusive PC/SC transaction on the connected card. "
            "No other application can access the card until end_transaction is called.",
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

            if (!pcsc.isConnected())
                throw RpcError{ RpcError::INTERNAL_ERROR,
                                "Not connected — call connect first" };

            LONG rv = SCardBeginTransaction(pcsc.handle());
            if (rv != SCARD_S_SUCCESS) {
                throw RpcError{ RpcError::INTERNAL_ERROR,
                                "SCardBeginTransaction failed: 0x" +
                                [rv]{
                                    std::ostringstream s;
                                    s << std::hex << std::uppercase << rv;
                                    return s.str();
                                }() };
            }

            return {
                {"content", json::array({
                    { {"type", "text"},
                      {"text", R"({"transaction": "begun"})"} }
                })}
            };
        }
    );

    // ── end_transaction ───────────────────────────────────────────────────────
    reg.registerTool(
        {
            "end_transaction",
            "End the current PC/SC transaction.",
            {
                {"type", "object"},
                {"properties", {
                    {"disposition", {
                        {"type",        "string"},
                        {"enum",        json::array({"leave", "reset", "unpower", "eject"})},
                        {"description", "Card disposition after transaction (default: leave)"},
                        {"default",     "leave"}
                    }}
                }},
                {"required", json::array()}
            }
        },
        [](const json& params) -> json {
            auto& sess = PcscSession::instance();
            std::lock_guard<std::mutex> lock(sess.mutex());
            auto& pcsc = sess.pcsc();

            if (!pcsc.isConnected())
                throw RpcError{ RpcError::INTERNAL_ERROR,
                                "Not connected — call connect first" };

            static const std::unordered_map<std::string, DWORD> dispMap = {
                {"leave",   SCARD_LEAVE_CARD},
                {"reset",   SCARD_RESET_CARD},
                {"unpower", SCARD_UNPOWER_CARD},
                {"eject",   SCARD_EJECT_CARD}
            };

            std::string dispStr = params.value("disposition", "leave");
            auto it = dispMap.find(dispStr);
            DWORD disp = (it != dispMap.end()) ? it->second : SCARD_LEAVE_CARD;

            LONG rv = SCardEndTransaction(pcsc.handle(), disp);
            if (rv != SCARD_S_SUCCESS) {
                throw RpcError{ RpcError::INTERNAL_ERROR,
                                "SCardEndTransaction failed: 0x" +
                                [rv]{
                                    std::ostringstream s;
                                    s << std::hex << std::uppercase << rv;
                                    return s.str();
                                }() };
            }

            return {
                {"content", json::array({
                    { {"type", "text"},
                      {"text", json({ {"transaction", "ended"}, {"disposition", dispStr} }).dump(2)} }
                })}
            };
        }
    );
}

} // namespace mcp::tools
