#pragma once

#include "mcp/transport.hpp"
#include "mcp/types.hpp"

#include <cstdint>
#include <functional>
#include <mutex>
#include <string>

namespace mcp {

class Client {
 public:
  using NotificationHandler = std::function<void(const std::string&, const Json&)>;
  using RequestHandler = std::function<Json(const std::string&, const Json&)>;

  Client(Transport& transport, Implementation client_info);

  [[nodiscard]] Json initialize(Json capabilities = Json::object());
  [[nodiscard]] Json request(const std::string& method, Json params = Json::object());
  void notify(const std::string& method, Json params = Json::object());
  void cancel(std::int64_t request_id, std::string reason = {});

  void on_notification(NotificationHandler handler);
  void on_request(RequestHandler handler);

 private:
  void send(const Json& message);
  [[nodiscard]] Json receive_response(std::int64_t expected_id);

  Transport& transport_;
  Implementation client_info_;
  std::int64_t next_id_{1};
  NotificationHandler notification_handler_;
  RequestHandler request_handler_;
  std::mutex request_mutex_;
};

}  // namespace mcp

