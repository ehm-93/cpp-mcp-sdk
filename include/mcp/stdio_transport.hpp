#pragma once

#include "mcp/transport.hpp"

#include <istream>
#include <mutex>
#include <ostream>

namespace mcp {

class StdioTransport final : public Transport {
 public:
  StdioTransport();
  StdioTransport(std::istream& input, std::ostream& output);

  [[nodiscard]] std::optional<std::string> read() override;
  void write(const std::string& message) override;

 private:
  std::istream& input_;
  std::ostream& output_;
  std::mutex write_mutex_;
};

}  // namespace mcp

