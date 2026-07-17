#pragma once

#include <nlohmann/json.hpp>

#include <cstdint>
#include <exception>
#include <functional>
#include <optional>
#include <stop_token>
#include <string>
#include <utility>
#include <vector>

namespace mcp {

// Keeping JSON in the public API lets callers use MCP extension fields without
// waiting for a new strongly typed SDK release.
using Json = nlohmann::json;
inline constexpr auto protocol_version = "2025-11-25";

// Standard JSON-RPC errors plus the resource error used by this MCP revision.
enum class ErrorCode : int {
  parse_error = -32700,
  invalid_request = -32600,
  method_not_found = -32601,
  invalid_params = -32602,
  internal_error = -32603,
  resource_not_found = -32002,
};

// Represents a protocol-level error code, message, and optional data payload.
// Callbacks may throw this when they need to control the JSON-RPC response.
class RpcError final : public std::exception {
 public:
  RpcError(ErrorCode code, std::string message, Json data = nullptr)
      : code_(code), message_(std::move(message)), data_(std::move(data)) {}

  [[nodiscard]] ErrorCode code() const noexcept { return code_; }
  [[nodiscard]] const Json& data() const noexcept { return data_; }
  [[nodiscard]] const char* what() const noexcept override { return message_.c_str(); }

 private:
  ErrorCode code_;
  std::string message_;
  Json data_;
};

// Software identity exchanged during initialization.
struct Implementation {
  std::string name;
  std::string version;
  std::optional<std::string> title;
  std::optional<std::string> description;
};

inline void to_json(Json& value, const Implementation& implementation) {
  value = {{"name", implementation.name}, {"version", implementation.version}};
  if (implementation.title) value["title"] = *implementation.title;
  if (implementation.description) value["description"] = *implementation.description;
}

// Tool failures are normally returned as data with isError=true rather than as
// protocol errors, allowing the model to inspect and correct its arguments.
struct ToolResult {
  std::vector<Json> content;
  std::optional<Json> structured_content;
  bool is_error{false};
  Json meta = Json::object();

  [[nodiscard]] static ToolResult text(std::string value) {
    ToolResult result;
    result.content.push_back({{"type", "text"}, {"text", std::move(value)}});
    return result;
  }

  [[nodiscard]] static ToolResult error(std::string value) {
    auto result = text(std::move(value));
    result.is_error = true;
    return result;
  }
};

inline void to_json(Json& value, const ToolResult& result) {
  value = {{"content", result.content}, {"isError", result.is_error}};
  if (result.structured_content) value["structuredContent"] = *result.structured_content;
  if (!result.meta.empty()) value["_meta"] = result.meta;
}

using ToolHandler = std::function<ToolResult(const Json&, std::stop_token)>;

// Metadata advertised by tools/list. Schemas remain JSON objects so users may
// choose their own JSON Schema validation library.
struct Tool {
  std::string name;
  std::string description;
  Json input_schema = {{"type", "object"}, {"additionalProperties", false}};
  std::optional<std::string> title;
  std::optional<Json> output_schema;
  std::optional<Json> annotations;
  std::optional<Json> icons;
  std::optional<Json> execution;
};

inline void to_json(Json& value, const Tool& tool) {
  value = {{"name", tool.name}, {"description", tool.description}, {"inputSchema", tool.input_schema}};
  if (tool.title) value["title"] = *tool.title;
  if (tool.output_schema) value["outputSchema"] = *tool.output_schema;
  if (tool.annotations) value["annotations"] = *tool.annotations;
  if (tool.icons) value["icons"] = *tool.icons;
  if (tool.execution) value["execution"] = *tool.execution;
}

struct ResourceContent {
  std::string uri;
  std::optional<std::string> mime_type;
  std::optional<std::string> text;
  // Binary resource bodies are base64-encoded on the wire.
  std::optional<std::string> blob;
  Json meta = Json::object();
};

inline void to_json(Json& value, const ResourceContent& content) {
  value = {{"uri", content.uri}};
  if (content.mime_type) value["mimeType"] = *content.mime_type;
  if (content.text) value["text"] = *content.text;
  if (content.blob) value["blob"] = *content.blob;
  if (!content.meta.empty()) value["_meta"] = content.meta;
}

using ResourceHandler = std::function<std::vector<ResourceContent>(std::stop_token)>;

// Metadata advertised by resources/list. Content is loaded lazily by the
// associated handler when resources/read is called.
struct Resource {
  std::string uri;
  std::string name;
  std::optional<std::string> title;
  std::optional<std::string> description;
  std::optional<std::string> mime_type;
  std::optional<std::int64_t> size;
  std::optional<Json> annotations;
  std::optional<Json> icons;
};

inline void to_json(Json& value, const Resource& resource) {
  value = {{"uri", resource.uri}, {"name", resource.name}};
  if (resource.title) value["title"] = *resource.title;
  if (resource.description) value["description"] = *resource.description;
  if (resource.mime_type) value["mimeType"] = *resource.mime_type;
  if (resource.size) value["size"] = *resource.size;
  if (resource.annotations) value["annotations"] = *resource.annotations;
  if (resource.icons) value["icons"] = *resource.icons;
}

// Argument declarations guide clients; handlers still validate actual input.
struct PromptArgument {
  std::string name;
  std::optional<std::string> title;
  std::optional<std::string> description;
  bool required{false};
};

inline void to_json(Json& value, const PromptArgument& argument) {
  value = {{"name", argument.name}, {"required", argument.required}};
  if (argument.title) value["title"] = *argument.title;
  if (argument.description) value["description"] = *argument.description;
}

// Prompt content stays as JSON because MCP supports multiple content variants.
struct PromptMessage {
  std::string role;
  Json content;
};

inline void to_json(Json& value, const PromptMessage& message) {
  value = {{"role", message.role}, {"content", message.content}};
}

struct PromptResult {
  std::optional<std::string> description;
  std::vector<PromptMessage> messages;
};

inline void to_json(Json& value, const PromptResult& result) {
  value = {{"messages", result.messages}};
  if (result.description) value["description"] = *result.description;
}

using PromptHandler = std::function<PromptResult(const Json&, std::stop_token)>;

// Metadata advertised by prompts/list.
struct Prompt {
  std::string name;
  std::optional<std::string> title;
  std::optional<std::string> description;
  std::vector<PromptArgument> arguments;
  std::optional<Json> icons;
};

inline void to_json(Json& value, const Prompt& prompt) {
  value = {{"name", prompt.name}, {"arguments", prompt.arguments}};
  if (prompt.title) value["title"] = *prompt.title;
  if (prompt.description) value["description"] = *prompt.description;
  if (prompt.icons) value["icons"] = *prompt.icons;
}

// Builders for common MCP content blocks.
[[nodiscard]] inline Json text_content(std::string text) {
  return {{"type", "text"}, {"text", std::move(text)}};
}

[[nodiscard]] inline Json image_content(std::string data, std::string mime_type) {
  return {{"type", "image"}, {"data", std::move(data)}, {"mimeType", std::move(mime_type)}};
}

[[nodiscard]] inline Json audio_content(std::string data, std::string mime_type) {
  return {{"type", "audio"}, {"data", std::move(data)}, {"mimeType", std::move(mime_type)}};
}

}  // namespace mcp
