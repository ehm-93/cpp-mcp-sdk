#pragma once

#include "mcp/transport.hpp"
#include "mcp/types.hpp"

#include <atomic>
#include <map>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <unordered_map>

namespace mcp {

// Identity and protocol-level information returned during initialization.
struct ServerOptions {
  Implementation server_info;
  std::string instructions;
  std::string protocol_version = mcp::protocol_version;
};

// Registry and dispatcher for the server side of MCP.
// Definitions and callbacks are copied into the server when registered.
class Server {
 public:
  explicit Server(ServerOptions options);

  Server& add_tool(Tool tool, ToolHandler handler);
  Server& add_resource(Resource resource, ResourceHandler handler);
  Server& add_prompt(Prompt prompt, PromptHandler handler);

  // Processes one decoded JSON-RPC message. Notifications return nullopt;
  // requests return a complete response object. This entry point is thread-safe.
  [[nodiscard]] std::optional<Json> handle(const Json& message);

  // Reads framed JSON-RPC messages until EOF. Requests may run concurrently so
  // a cancellation notification can reach a handler that is still running.
  void serve(Transport& transport);

  // Helpers for applications that mutate a registry visible to their clients.
  [[nodiscard]] static Json tools_changed_notification();
  [[nodiscard]] static Json resources_changed_notification();
  [[nodiscard]] static Json prompts_changed_notification();

 private:
  // Keeping definitions beside handlers makes lookup atomic under one mutex.
  struct ToolEntry { Tool definition; ToolHandler handler; };
  struct ResourceEntry { Resource definition; ResourceHandler handler; };
  struct PromptEntry { Prompt definition; PromptHandler handler; };

  [[nodiscard]] Json dispatch(const std::string& method, const Json& params,
                              const Json& id, std::stop_token stop_token);
  [[nodiscard]] Json capabilities() const;
  void cancel(const Json& params);
  [[nodiscard]] static Json success(const Json& id, Json result);
  [[nodiscard]] static Json failure(const Json& id, ErrorCode code,
                                    const std::string& message, const Json& data = nullptr);
  [[nodiscard]] static std::string id_key(const Json& id);

  ServerOptions options_;

  // std::map gives deterministic list ordering, which is friendly to caches and
  // makes tests and generated prompts reproducible.
  std::map<std::string, ToolEntry, std::less<>> tools_;
  std::map<std::string, ResourceEntry, std::less<>> resources_;
  std::map<std::string, PromptEntry, std::less<>> prompts_;
  mutable std::mutex registry_mutex_;

  // Active request IDs map to stop sources. notifications/cancelled signals the
  // matching source, and handlers observe it through their std::stop_token.
  std::mutex requests_mutex_;
  std::unordered_map<std::string, std::stop_source> active_requests_;

  // ping is legal before initialization; other operational methods are not.
  std::atomic<bool> initialize_seen_{false};
};

}  // namespace mcp
