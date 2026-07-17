#pragma once

#include <optional>
#include <string>

namespace mcp {

class Transport {
 public:
  virtual ~Transport() = default;
  [[nodiscard]] virtual std::optional<std::string> read() = 0;
  virtual void write(const std::string& message) = 0;
};

}  // namespace mcp

