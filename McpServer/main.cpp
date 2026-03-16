#include "McpServer/McpServer.h"
#include "McpServer/ToolRegistry.h"
#include "McpServer/Tools/ListReadersTool.h"
#include "McpServer/Tools/ConnectTool.h"
#include "McpServer/Tools/DisconnectTool.h"
#include "McpServer/Tools/SendApduTool.h"
#include "McpServer/Tools/TransactionTool.h"

int main() {
    // stdout'u unbuffered yap — Claude Code her satırı anında okusun
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    mcp::ToolRegistry registry;

    mcp::tools::registerListReaders(registry);
    mcp::tools::registerConnect(registry);
    mcp::tools::registerDisconnect(registry);
    mcp::tools::registerSendApdu(registry);
    mcp::tools::registerTransaction(registry);

    mcp::McpServer server({ "pcsc-mcp", "1.0.0" }, registry);
    server.run();

    return 0;
}
