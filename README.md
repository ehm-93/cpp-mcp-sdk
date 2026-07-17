# mcp-cpp-sdk

A small, modern C++20 SDK for building and consuming
[Model Context Protocol](https://modelcontextprotocol.io/) servers. It targets the
stable MCP protocol revision `2025-11-25` and has no runtime dependencies beyond
the C++ standard library; JSON encoding uses `nlohmann_json`.

## Features

- JSON-RPC 2.0 validation, responses, standard errors, ping, and cancellation
- MCP initialization and capability negotiation
- Typed registration APIs for tools, resources, and prompts
- Text, image, audio, embedded/linked resource, and structured result building blocks
- Concurrent stdio server with `std::stop_token` cancellation
- Synchronous client with notification and server-to-client request callbacks
- CMake install/export support through the `mcp::sdk` target

## Quick start

```cpp
#include <mcp/mcp.hpp>

int main() {
  mcp::Server server({.server_info = {.name = "demo", .version = "1.0.0"}});

  server.add_tool(
      {.name = "hello",
       .description = "Say hello",
       .input_schema = {{"type", "object"},
                        {"properties", {{"name", {{"type", "string"}}}}}}},
      [](const mcp::Json& args, std::stop_token) {
        return mcp::ToolResult::text("Hello, " + args.value("name", "world") + "!");
      });

  mcp::StdioTransport transport;
  server.serve(transport);
}
```

The complete calculator server is in
[`examples/calculator_server.cpp`](examples/calculator_server.cpp).

## Build

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

If `nlohmann_json` is not already installed, CMake fetches version 3.11.3. Set
`MCP_CPP_FETCH_DEPENDENCIES=OFF` for fully offline/configured dependency builds.
Consumers can use an installed package with:

```cmake
find_package(mcp-cpp-sdk CONFIG REQUIRED)
target_link_libraries(my_server PRIVATE mcp::sdk)
```

## Server API

`mcp::Server::add_tool`, `add_resource`, and `add_prompt` copy the callbacks into
the registry and reject duplicate names/URIs. Registry listings are deterministic.
Handlers may throw; tool exceptions become MCP tool errors (`isError: true`), while
resource and prompt exceptions become JSON-RPC errors. Every handler receives a
`std::stop_token` that is signalled by `notifications/cancelled`.

`Server::handle` is a transport-neutral, thread-safe entry point for already parsed
messages. `Server::serve` runs newline-delimited stdio-style transports and dispatches
requests concurrently.

## Client API

Construct `mcp::Client` with any `mcp::Transport`, call `initialize()`, then use
`request()` and `notify()`. While waiting for a response it dispatches inbound
notifications and server-to-client requests to callbacks installed with
`on_notification` and `on_request`.

## Scope

This release implements the stable stdio protocol core and the principal server
features. Streamable HTTP, OAuth, resource templates/subscriptions, completions,
sampling, elicitation, and experimental tasks are intentionally transport or
extension layers for future releases.

## Security

MCP tool calls can perform arbitrary actions. Applications embedding this SDK are
responsible for authorization, user consent, input/schema validation, secret
handling, sandboxing, and output size limits. Tool annotations must be treated as
untrusted unless the server itself is trusted.

## License

MIT-0
