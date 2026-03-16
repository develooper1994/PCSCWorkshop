/** @file McpServer.h
 * @brief MCP server stdio loop (MCP 2025-03-26 spec). */
#pragma once
#include "Mcp/JsonRpc.h"
#include "ToolRegistry.h"
#include <string>
#include <iostream>
#include <stdexcept>

namespace mcp {

// ── Server metadata ───────────────────────────────────────────────────────────
struct ServerInfo {
    std::string name;
    std::string version;
};

// ════════════════════════════════════════════════════════════════════════════════
// McpServer
//
// MCP 2025-03-26 spec — stdio transport.
// Lifecycle:
//   initialize  → initialized  → tools/list | tools/call  → (loop)
//
// Usage:
//   McpServer srv({ "pcsc-mcp", "1.0.0" }, registry);
//   srv.run();   // blocks on stdin
// ════════════════════════════════════════════════════════════════════════════════
class McpServer {
public:
    McpServer(ServerInfo info, ToolRegistry& registry)
        : info_(std::move(info)), registry_(registry) {}

    // Bloklar — stdin kapanana kadar çalışır
    void run() {
        std::string line;
        while (std::getline(std::cin, line)) {
            if (line.empty()) continue;
            handleLine(line);
        }
    }

private:
    ServerInfo    info_;
    ToolRegistry& registry_;
    bool          initialized_ = false;

    void handleLine(const std::string& line) {
        json id = nullptr;
        try {
            json j = json::parse(line);
            auto req = RpcRequest::parse(j);

            if (!req) {
                send(makeError(nullptr, RpcError::INVALID_REQUEST,
                               "Invalid JSON-RPC request"));
                return;
            }

            id = req->id.has_value() ? *req->id : json(nullptr);

            // notifications (id olmayan) sadece handle, yanıt gönderme
            bool isNotification = !req->id.has_value();
            json result = dispatch(*req);

            if (!isNotification)
                send(makeResult(id, result));

        } catch (const RpcError& e) {
            send(makeError(id, e));
        } catch (const json::exception& e) {
            send(makeError(id, RpcError::PARSE_ERROR, e.what()));
        } catch (const std::exception& e) {
            send(makeError(id, RpcError::INTERNAL_ERROR, e.what()));
        }
    }

    json dispatch(const RpcRequest& req) {
        const auto& m = req.method;

        // ── Lifecycle ────────────────────────────────────────────────────────
        if (m == "initialize")        return handleInitialize(req.params);
        if (m == "notifications/initialized") {
            initialized_ = true;
            return nullptr; // notification
        }
        if (m == "ping")              return json::object();

        // ── Capability discovery ─────────────────────────────────────────────
        if (m == "tools/list")        return registry_.listTools();

        // ── Tool invocation ──────────────────────────────────────────────────
        if (m == "tools/call")        return handleToolCall(req.params);

        throw RpcError{ RpcError::METHOD_NOT_FOUND, "Method not found: " + m };
    }

    json handleInitialize(const json& params) {
        // params.clientInfo, params.protocolVersion — loglama için saklayabiliriz
        (void)params;
        return {
            {"protocolVersion", "2025-03-26"},
            {"capabilities",    { {"tools", json::object()} }},
            {"serverInfo",      { {"name", info_.name}, {"version", info_.version} }}
        };
    }

    json handleToolCall(const json& params) {
        if (!params.contains("name") || !params["name"].is_string())
            throw RpcError{ RpcError::INVALID_PARAMS, "Missing 'name'" };

        std::string name = params["name"];
        json        args = params.value("arguments", json::object());

        // Tool sonucu MCP content array formatında dönmeli
        json toolResult = registry_.callTool(name, args);

        // Eğer tool zaten {content:[...]} döndürdüyse wrap etme
        if (toolResult.contains("content"))
            return toolResult;

        // Aksi halde text content olarak wrap et
        return {
            {"content", json::array({
                { {"type", "text"}, {"text", toolResult.dump(2)} }
            })}
        };
    }

    static void send(const json& j) {
        std::cout << j.dump() << "\n";
        std::cout.flush();
    }
};

} // namespace mcp
