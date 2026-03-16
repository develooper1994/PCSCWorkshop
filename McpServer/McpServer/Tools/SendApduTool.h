#pragma once
#include "../ToolRegistry.h"
#include "../PcscSession.h"
#include "../HexUtils.h"

namespace mcp::tools {

inline void registerSendApdu(ToolRegistry& reg) {
    reg.registerTool(
        {
            "send_apdu",
            "Send a raw APDU command to the connected smart card. "
            "Returns the response bytes and SW1/SW2 status word.",
            {
                {"type", "object"},
                {"properties", {
                    {"apdu", {
                        {"type",        "string"},
                        {"description", "Hex-encoded APDU bytes, e.g. \"00A4040007D2760000850100\""}
                    }},
                    {"follow_chaining", {
                        {"type",        "boolean"},
                        {"description", "Automatically send GET RESPONSE for 61xx (default: true)"},
                        {"default",     true}
                    }}
                }},
                {"required", json::array({"apdu"})}
            }
        },
        [](const json& params) -> json {
            if (!params.contains("apdu") || !params["apdu"].is_string())
                throw RpcError{ RpcError::INVALID_PARAMS, "'apdu' is required" };

            std::string apduHex      = params["apdu"];
            bool        followChain  = params.value("follow_chaining", true);

            BYTEV apduBytes;
            try {
                apduBytes = fromHex(apduHex);
            } catch (const std::exception& e) {
                throw RpcError{ RpcError::INVALID_PARAMS,
                                std::string("Invalid hex in 'apdu': ") + e.what() };
            }

            auto& sess = PcscSession::instance();
            std::lock_guard<std::mutex> lock(sess.mutex());
            auto& pcsc = sess.pcsc();

            if (!pcsc.isConnected())
                throw RpcError{ RpcError::INTERNAL_ERROR,
                                "Not connected — call connect first" };

            auto result = pcsc.trySendCommand(apduBytes, followChain);
            if (!result.is_ok()) {
                throw RpcError{ RpcError::INTERNAL_ERROR,
                                result.error().message() };
            }

            const BYTEV& resp = result.unwrap();

            // Son 2 byte = SW1 SW2
            std::string sw = "????";
            std::string data;
            if (resp.size() >= 2) {
                BYTEV swBytes = { resp[resp.size() - 2], resp[resp.size() - 1] };
                BYTEV dataBytes(resp.begin(), resp.end() - 2);
                sw   = toHex(swBytes);
                data = toHex(dataBytes);
            } else {
                data = toHex(resp);
            }

            return {
                {"content", json::array({
                    { {"type", "text"},
                      {"text", json({
                          {"sw",       sw},
                          {"data",     data},
                          {"raw",      toHex(resp)},
                          {"success",  sw == "9000"}
                      }).dump(2)} }
                })}
            };
        }
    );
}

} // namespace mcp::tools
