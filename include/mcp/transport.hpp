#pragma once

#include <optional>
#include <string>

namespace mcp {

// Minimal message-oriented I/O boundary shared by Client and Server.
// Implementations provide framing: StdioTransport uses one JSON message per line.
class Transport {
 public:
  virtual ~Transport() = default;

  // Returns the next complete wire message, or nullopt when the channel closes.
  [[nodiscard]] virtual std::optional<std::string> read() = 0;

  // Writes one complete wire message. Server::serve may call this from workers,
  // although it serializes those calls before they reach the transport.
  virtual void write(const std::string& message) = 0;
};

}  // namespace mcp
