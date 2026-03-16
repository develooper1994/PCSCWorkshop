/** @file JsonRpc.h
 * @brief JSON-RPC 2.0 request/response/error types. */
#pragma once
#include <nlohmann/json.hpp>
#include <string>
#include <optional>

namespace mcp {

using json = nlohmann::json;

// ── JSON-RPC 2.0 Error Codes ────────────────────────────────────────────────
struct RpcError {
    static constexpr int PARSE_ERROR      = -32700;
    static constexpr int INVALID_REQUEST  = -32600;
    static constexpr int METHOD_NOT_FOUND = -32601;
    static constexpr int INVALID_PARAMS   = -32602;
    static constexpr int INTERNAL_ERROR   = -32603;

    int         code;
    std::string message;
    json        data = nullptr;

    json toJson() const {
        json j = { {"code", code}, {"message", message} };
        if (!data.is_null()) j["data"] = data;
        return j;
    }
};

// ── Inbound request ─────────────────────────────────────────────────────────
struct RpcRequest {
    std::string          jsonrpc = "2.0";
    std::string          method;
    json                 params  = nullptr;
    std::optional<json>  id;     // nullopt = notification

    static std::optional<RpcRequest> parse(const json& j) {
        if (!j.contains("method") || !j["method"].is_string())
            return std::nullopt;

        RpcRequest req;
        req.method = j["method"].get<std::string>();

        if (j.contains("params"))
            req.params = j["params"];

        if (j.contains("id"))
            req.id = j["id"];

        return req;
    }
};

// ── Outbound response ────────────────────────────────────────────────────────
inline json makeResult(const json& id, const json& result) {
    return { {"jsonrpc", "2.0"}, {"id", id}, {"result", result} };
}

inline json makeError(const json& id, const RpcError& err) {
    return { {"jsonrpc", "2.0"}, {"id", id}, {"error", err.toJson()} };
}

inline json makeError(const json& id, int code, const std::string& msg) {
    return makeError(id, RpcError{ code, msg });
}

} // namespace mcp
