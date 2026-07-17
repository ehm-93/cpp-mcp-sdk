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

struct ServerOptions {
  Implementation server_info;
  std::string instructions;
  std::string protocol_version = mcp::protocol_version;
};

class Server {
 public:
  explicit Server(ServerOptions options);

  Server& add_tool(Tool tool, ToolHandler handler);
  Server& add_resource(Resource resource, ResourceHandler handler);
  Server& add_prompt(Prompt prompt, PromptHandler handler);

  // Processes one decoded JSON-RPC message. Thread-safe.
  [[nodiscard]] std::optional<Json> handle(const Json& message);

  // Serves newline-delimited JSON-RPC until EOF. Requests may run concurrently.
  void serve(Transport& transport);

  [[nodiscard]] static Json tools_changed_notification();
  [[nodiscard]] static Json resources_changed_notification();
  [[nodiscard]] static Json prompts_changed_notification();

 private:
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
  std::map<std::string, ToolEntry, std::less<>> tools_;
  std::map<std::string, ResourceEntry, std::less<>> resources_;
  std::map<std::string, PromptEntry, std::less<>> prompts_;
  mutable std::mutex registry_mutex_;
  std::mutex requests_mutex_;
  std::unordered_map<std::string, std::stop_source> active_requests_;
  std::atomic<bool> initialize_seen_{false};
};

}  // namespace mcp

