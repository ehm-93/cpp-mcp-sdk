#include <mcp/mcp.hpp>

#include <iostream>
#include <sstream>
#include <stop_token>
#include <stdexcept>
#include <string>

namespace {

// A compact request builder keeps each test focused on its MCP payload.
mcp::Json request(std::int64_t id, std::string method, mcp::Json params = mcp::Json::object()) {
  return {{"jsonrpc", "2.0"}, {"id", id}, {"method", std::move(method)}, {"params", std::move(params)}};
}

void expect(bool condition, const char* message) {
  // Unlike assert(), this check remains active in Release builds.
  if (!condition) throw std::runtime_error(message);
}

void test_server() {
  // Register one of each server primitive, then exercise the JSON-RPC boundary
  // through Server::handle without involving real process I/O.
  mcp::Server server({.server_info = {.name = "test", .version = "1"}});
  server.add_tool(
      {.name = "echo", .description = "Echo input", .input_schema = {{"type", "object"}}},
      [](const mcp::Json& args, std::stop_token) {
        auto result = mcp::ToolResult::text(args.value("message", ""));
        result.structured_content = args;
        return result;
      });
  server.add_resource(
      {.uri = "test://readme", .name = "readme", .mime_type = "text/plain"},
      [](std::stop_token) {
        return std::vector<mcp::ResourceContent>{{.uri = "test://readme",
                                                   .mime_type = "text/plain",
                                                   .text = "hello"}};
      });
  server.add_prompt(
      {.name = "greet", .description = "Create a greeting", .arguments = {{.name = "name"}}},
      [](const mcp::Json& args, std::stop_token) {
        return mcp::PromptResult{.messages = {{.role = "user",
                                              .content = mcp::text_content("Hello " + args.value("name", "world"))}}};
      });

  auto response = server.handle(request(1, "initialize",
      {{"protocolVersion", mcp::protocol_version}, {"capabilities", mcp::Json::object()},
       {"clientInfo", {{"name", "test-client"}, {"version", "1"}}}}));
  expect(response && response->at("result").at("protocolVersion") == mcp::protocol_version,
         "initialize did not negotiate the protocol version");
  expect(response->at("result").at("capabilities").contains("tools"),
         "initialize did not advertise tools");

  response = server.handle(request(2, "tools/list"));
  expect(response && response->at("result").at("tools").size() == 1,
         "tools/list returned the wrong size");
  expect(response->at("result").at("tools").at(0).at("name") == "echo",
         "tools/list returned the wrong tool");

  response = server.handle(request(3, "tools/call", {{"name", "echo"},
                                                       {"arguments", {{"message", "hi"}}}}));
  expect(response && response->at("result").at("content").at(0).at("text") == "hi",
         "tools/call returned the wrong text");
  expect(response->at("result").at("structuredContent").at("message") == "hi",
         "tools/call returned the wrong structured content");

  response = server.handle(request(4, "resources/read", {{"uri", "test://readme"}}));
  expect(response && response->at("result").at("contents").at(0).at("text") == "hello",
         "resources/read returned the wrong content");

  response = server.handle(request(5, "prompts/get", {{"name", "greet"},
                                                       {"arguments", {{"name", "C++"}}}}));
  expect(response && response->at("result").at("messages").at(0).at("content").at("text") == "Hello C++",
         "prompts/get returned the wrong message");

  response = server.handle(request(6, "missing/method"));
  expect(response && response->at("error").at("code") == -32601,
         "unknown method did not return method-not-found");
}

void test_stdio_transport() {
  // Injected streams make framing and flushing behavior deterministic.
  std::istringstream input("\n{\"jsonrpc\":\"2.0\"}\n");
  std::ostringstream output;
  mcp::StdioTransport transport(input, output);
  expect(transport.read() == R"({"jsonrpc":"2.0"})", "stdio transport read failed");
  expect(!transport.read(), "stdio transport did not reach EOF");
  transport.write("reply");
  expect(output.str() == "reply\n", "stdio transport write failed");
}

}  // namespace

int main() {
  try {
    test_server();
    test_stdio_transport();
  } catch (const std::exception& error) {
    std::cerr << "test failure: " << error.what() << '\n';
    return 1;
  }
}
