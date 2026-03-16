#pragma once
#include "JsonRpc.h"
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace mcp {

// ── Tool descriptor (MCP tools/list formatı) ─────────────────────────────────
struct ToolDescriptor {
    std::string name;
    std::string description;
    json        inputSchema;  // JSON Schema object
};

using ToolHandler = std::function<json(const json& params)>;

// ── Registry ─────────────────────────────────────────────────────────────────
class ToolRegistry {
public:
    void registerTool(ToolDescriptor desc, ToolHandler handler) {
        handlers_[desc.name] = std::move(handler);
        descriptors_.push_back(std::move(desc));
    }

    // tools/list yanıtı
    json listTools() const {
        json tools = json::array();
        for (const auto& d : descriptors_) {
            tools.push_back({
                {"name",        d.name},
                {"description", d.description},
                {"inputSchema", d.inputSchema}
            });
        }
        return { {"tools", tools} };
    }

    // tools/call dispatch — bulunamazsa RpcError fırlatır
    json callTool(const std::string& name, const json& params) const {
        auto it = handlers_.find(name);
        if (it == handlers_.end()) {
            throw RpcError{
                RpcError::METHOD_NOT_FOUND,
                "Unknown tool: " + name
            };
        }
        return it->second(params);
    }

    bool has(const std::string& name) const {
        return handlers_.count(name) > 0;
    }

private:
    std::unordered_map<std::string, ToolHandler> handlers_;
    std::vector<ToolDescriptor>                  descriptors_;
};

} // namespace mcp
