#pragma once

#include "mcp/transport.hpp"

#include <istream>
#include <mutex>
#include <ostream>

namespace mcp {

// Newline-delimited transport used by local MCP subprocesses.
// Protocol traffic belongs on stdout; applications should log to stderr.
class StdioTransport final : public Transport {
 public:
  // Connects to the process-wide std::cin and std::cout streams.
  StdioTransport();

  // Stream injection is useful for tests or embedding in another I/O layer.
  // The streams are borrowed and must outlive this object.
  StdioTransport(std::istream& input, std::ostream& output);

  [[nodiscard]] std::optional<std::string> read() override;
  void write(const std::string& message) override;

 private:
  std::istream& input_;
  std::ostream& output_;

  // A complete JSON message must not be interleaved with another writer.
  std::mutex write_mutex_;
};

}  // namespace mcp
