#include "mcp/stdio_transport.hpp"

#include <iostream>
#include <string>

namespace mcp {

StdioTransport::StdioTransport() : StdioTransport(std::cin, std::cout) {}

StdioTransport::StdioTransport(std::istream& input, std::ostream& output)
    : input_(input), output_(output) {}

std::optional<std::string> StdioTransport::read() {
  std::string line;
  while (std::getline(input_, line)) {
    // Empty lines carry no protocol message and are safe to ignore.
    if (!line.empty()) return line;
  }
  return std::nullopt;
}

void StdioTransport::write(const std::string& message) {
  const std::scoped_lock lock(write_mutex_);
  // Flushing is important for subprocess transports: the peer may be blocked
  // waiting for this response before it writes anything else.
  output_ << message << '\n' << std::flush;
}

}  // namespace mcp
