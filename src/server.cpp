#include "mcp/server.hpp"

#include <algorithm>
#include <chrono>
#include <future>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

namespace mcp {
namespace {

bool valid_id(const Json& id) {
  return id.is_string() || id.is_number_integer();
}

const Json& object_params(const Json& message) {
  static const Json empty = Json::object();
  if (!message.contains("params")) return empty;
  const auto& params = message.at("params");
  if (!params.is_object()) throw RpcError(ErrorCode::invalid_params, "params must be an object");
  return params;
}

}  // namespace

Server::Server(ServerOptions options) : options_(std::move(options)) {
  if (options_.server_info.name.empty()) throw std::invalid_argument("server name must not be empty");
  if (options_.server_info.version.empty()) throw std::invalid_argument("server version must not be empty");
}

Server& Server::add_tool(Tool tool, ToolHandler handler) {
  if (tool.name.empty() || tool.name.size() > 128) {
    throw std::invalid_argument("tool name must contain between 1 and 128 characters");
  }
  if (!tool.input_schema.is_object()) throw std::invalid_argument("tool input schema must be an object");
  if (!handler) throw std::invalid_argument("tool handler must be callable");
  const auto name = tool.name;
  const std::scoped_lock lock(registry_mutex_);
  if (!tools_.emplace(name, ToolEntry{std::move(tool), std::move(handler)}).second) {
    throw std::invalid_argument("duplicate tool name");
  }
  return *this;
}

Server& Server::add_resource(Resource resource, ResourceHandler handler) {
  if (resource.uri.empty()) throw std::invalid_argument("resource URI must not be empty");
  if (!handler) throw std::invalid_argument("resource handler must be callable");
  const auto uri = resource.uri;
  const std::scoped_lock lock(registry_mutex_);
  if (!resources_.emplace(uri, ResourceEntry{std::move(resource), std::move(handler)}).second) {
    throw std::invalid_argument("duplicate resource URI");
  }
  return *this;
}

Server& Server::add_prompt(Prompt prompt, PromptHandler handler) {
  if (prompt.name.empty()) throw std::invalid_argument("prompt name must not be empty");
  if (!handler) throw std::invalid_argument("prompt handler must be callable");
  const auto name = prompt.name;
  const std::scoped_lock lock(registry_mutex_);
  if (!prompts_.emplace(name, PromptEntry{std::move(prompt), std::move(handler)}).second) {
    throw std::invalid_argument("duplicate prompt name");
  }
  return *this;
}

Json Server::capabilities() const {
  Json result = Json::object();
  const std::scoped_lock lock(registry_mutex_);
  if (!tools_.empty()) result["tools"] = {{"listChanged", true}};
  if (!resources_.empty()) result["resources"] = {{"listChanged", true}, {"subscribe", false}};
  if (!prompts_.empty()) result["prompts"] = {{"listChanged", true}};
  return result;
}

std::string Server::id_key(const Json& id) {
  return id.is_string() ? "s:" + id.get<std::string>() : "n:" + id.dump();
}

Json Server::success(const Json& id, Json result) {
  return {{"jsonrpc", "2.0"}, {"id", id}, {"result", std::move(result)}};
}

Json Server::failure(const Json& id, ErrorCode code, const std::string& message, const Json& data) {
  Json error = {{"code", static_cast<int>(code)}, {"message", message}};
  if (!data.is_null()) error["data"] = data;
  return {{"jsonrpc", "2.0"}, {"id", id}, {"error", std::move(error)}};
}

void Server::cancel(const Json& params) {
  if (!params.is_object() || !params.contains("requestId") || !valid_id(params.at("requestId"))) return;
  const auto key = id_key(params.at("requestId"));
  const std::scoped_lock lock(requests_mutex_);
  if (const auto found = active_requests_.find(key); found != active_requests_.end()) {
    found->second.request_stop();
  }
}

Json Server::dispatch(const std::string& method, const Json& params, const Json&,
                      std::stop_token stop_token) {
  if (method == "initialize") {
    if (!params.contains("protocolVersion") || !params.at("protocolVersion").is_string() ||
        !params.contains("clientInfo") || !params.at("clientInfo").is_object() ||
        !params.contains("capabilities") || !params.at("capabilities").is_object()) {
      throw RpcError(ErrorCode::invalid_params, "invalid initialize parameters");
    }
    const auto requested = params.at("protocolVersion").get<std::string>();
    const auto negotiated = requested == options_.protocol_version ? requested : options_.protocol_version;
    initialize_seen_.store(true);
    Json result = {{"protocolVersion", negotiated},
                   {"capabilities", capabilities()},
                   {"serverInfo", options_.server_info}};
    if (!options_.instructions.empty()) result["instructions"] = options_.instructions;
    return result;
  }

  if (method == "ping") return Json::object();

  if (!initialize_seen_.load()) {
    throw RpcError(ErrorCode::invalid_request, "server has not been initialized");
  }

  if (method == "tools/list") {
    Json tools = Json::array();
    const std::scoped_lock lock(registry_mutex_);
    for (const auto& [name, entry] : tools_) {
      static_cast<void>(name);
      tools.push_back(entry.definition);
    }
    return {{"tools", std::move(tools)}};
  }

  if (method == "tools/call") {
    if (!params.contains("name") || !params.at("name").is_string()) {
      throw RpcError(ErrorCode::invalid_params, "tool name is required");
    }
    const auto name = params.at("name").get<std::string>();
    ToolHandler handler;
    {
      const std::scoped_lock lock(registry_mutex_);
      const auto found = tools_.find(name);
      if (found == tools_.end()) throw RpcError(ErrorCode::invalid_params, "unknown tool: " + name);
      handler = found->second.handler;
    }
    const Json arguments = params.value("arguments", Json::object());
    if (!arguments.is_object()) throw RpcError(ErrorCode::invalid_params, "tool arguments must be an object");
    try {
      return Json(handler(arguments, stop_token));
    } catch (const RpcError& error) {
      return Json(ToolResult::error(error.what()));
    } catch (const std::exception& error) {
      return Json(ToolResult::error(error.what()));
    }
  }

  if (method == "resources/list") {
    Json resources = Json::array();
    const std::scoped_lock lock(registry_mutex_);
    for (const auto& [uri, entry] : resources_) {
      static_cast<void>(uri);
      resources.push_back(entry.definition);
    }
    return {{"resources", std::move(resources)}};
  }

  if (method == "resources/read") {
    if (!params.contains("uri") || !params.at("uri").is_string()) {
      throw RpcError(ErrorCode::invalid_params, "resource URI is required");
    }
    const auto uri = params.at("uri").get<std::string>();
    ResourceHandler handler;
    {
      const std::scoped_lock lock(registry_mutex_);
      const auto found = resources_.find(uri);
      if (found == resources_.end()) throw RpcError(ErrorCode::resource_not_found, "resource not found: " + uri);
      handler = found->second.handler;
    }
    return {{"contents", handler(stop_token)}};
  }

  if (method == "prompts/list") {
    Json prompts = Json::array();
    const std::scoped_lock lock(registry_mutex_);
    for (const auto& [name, entry] : prompts_) {
      static_cast<void>(name);
      prompts.push_back(entry.definition);
    }
    return {{"prompts", std::move(prompts)}};
  }

  if (method == "prompts/get") {
    if (!params.contains("name") || !params.at("name").is_string()) {
      throw RpcError(ErrorCode::invalid_params, "prompt name is required");
    }
    const auto name = params.at("name").get<std::string>();
    PromptHandler handler;
    {
      const std::scoped_lock lock(registry_mutex_);
      const auto found = prompts_.find(name);
      if (found == prompts_.end()) throw RpcError(ErrorCode::invalid_params, "unknown prompt: " + name);
      handler = found->second.handler;
    }
    const Json arguments = params.value("arguments", Json::object());
    if (!arguments.is_object()) throw RpcError(ErrorCode::invalid_params, "prompt arguments must be an object");
    return Json(handler(arguments, stop_token));
  }

  throw RpcError(ErrorCode::method_not_found, "method not found: " + method);
}

std::optional<Json> Server::handle(const Json& message) {
  if (!message.is_object() || message.value("jsonrpc", "") != "2.0" ||
      !message.contains("method") || !message.at("method").is_string()) {
    return failure(nullptr, ErrorCode::invalid_request, "invalid JSON-RPC request");
  }

  const auto method = message.at("method").get<std::string>();
  const bool is_request = message.contains("id");
  if (is_request && !valid_id(message.at("id"))) {
    return failure(nullptr, ErrorCode::invalid_request, "request id must be a string or integer");
  }

  try {
    const auto& params = object_params(message);
    if (!is_request) {
      if (method == "notifications/cancelled") cancel(params);
      return std::nullopt;
    }

    const Json id = message.at("id");
    std::stop_source source;
    const auto key = id_key(id);
    {
      const std::scoped_lock lock(requests_mutex_);
      if (!active_requests_.emplace(key, source).second) {
        return failure(id, ErrorCode::invalid_request, "request id is already active");
      }
    }

    struct EraseRequest {
      Server& server;
      std::string key;
      ~EraseRequest() {
        const std::scoped_lock lock(server.requests_mutex_);
        server.active_requests_.erase(key);
      }
    } erase{*this, key};

    return success(id, dispatch(method, params, id, source.get_token()));
  } catch (const RpcError& error) {
    return is_request ? std::optional<Json>(failure(message.at("id"), error.code(), error.what(), error.data()))
                      : std::nullopt;
  } catch (const std::exception& error) {
    return is_request ? std::optional<Json>(failure(message.at("id"), ErrorCode::internal_error, error.what()))
                      : std::nullopt;
  }
}

void Server::serve(Transport& transport) {
  std::vector<std::future<void>> pending;
  std::mutex transport_mutex;
  while (const auto wire = transport.read()) {
    Json message;
    try {
      message = Json::parse(*wire);
    } catch (const Json::parse_error&) {
      const std::scoped_lock lock(transport_mutex);
      transport.write(failure(nullptr, ErrorCode::parse_error, "parse error").dump());
      continue;
    }

    // Notifications are cheap and cancellation must be observed immediately.
    if (message.is_object() && !message.contains("id")) {
      static_cast<void>(handle(message));
    } else {
      pending.erase(std::remove_if(pending.begin(), pending.end(), [](std::future<void>& task) {
                      return task.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
                    }), pending.end());
      pending.push_back(std::async(std::launch::async,
                                  [this, &transport, &transport_mutex, message = std::move(message)] {
        if (const auto response = handle(message)) {
          const std::scoped_lock lock(transport_mutex);
          transport.write(response->dump());
        }
      }));
    }
  }
  for (auto& task : pending) task.wait();
}

Json Server::tools_changed_notification() {
  return {{"jsonrpc", "2.0"}, {"method", "notifications/tools/list_changed"}};
}

Json Server::resources_changed_notification() {
  return {{"jsonrpc", "2.0"}, {"method", "notifications/resources/list_changed"}};
}

Json Server::prompts_changed_notification() {
  return {{"jsonrpc", "2.0"}, {"method", "notifications/prompts/list_changed"}};
}

}  // namespace mcp
