#include <mcp/mcp.hpp>

#include <stop_token>
#include <string>

int main() {
  // Designated initializers make MCP metadata readable at the call site.
  mcp::Server server({.server_info = {.name = "calculator", .version = "1.0.0"},
                      .instructions = "Use add to add two numbers."});

  server.add_tool(
      {.name = "add",
       .description = "Add two numbers",
       .input_schema = {{"type", "object"},
                        {"properties", {{"a", {{"type", "number"}}},
                                        {"b", {{"type", "number"}}}}},
                        {"required", {"a", "b"}},
                        {"additionalProperties", false}}},
      // Handlers are ordinary callables. Keep the stop token parameter even
      // when a short operation has no useful cancellation point.
      [](const mcp::Json& arguments, std::stop_token) {
        if (!arguments.contains("a") || !arguments.at("a").is_number() ||
            !arguments.contains("b") || !arguments.at("b").is_number()) {
          return mcp::ToolResult::error("a and b must be numbers");
        }
        const auto sum = arguments.at("a").get<double>() + arguments.at("b").get<double>();
        // Structured content is machine-readable; text remains useful to older
        // clients and can be shown directly to a user or model.
        auto result = mcp::ToolResult::text(std::to_string(sum));
        result.structured_content = {{"sum", sum}};
        return result;
      });

  // serve() owns the read loop and returns when stdin reaches EOF.
  mcp::StdioTransport transport;
  server.serve(transport);
}
