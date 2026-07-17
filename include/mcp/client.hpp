#pragma once

#include "mcp/transport.hpp"
#include "mcp/types.hpp"

#include <cstdint>
#include <functional>
#include <mutex>
#include <string>

namespace mcp {

// A synchronous JSON-RPC/MCP client built on an arbitrary Transport.
//
// The Client does not own its Transport, so the transport must outlive it.
// Calls to request() block until the matching response arrives.
class Client {
 public:
  // Notifications are one-way messages and therefore have no return value.
  using NotificationHandler = std::function<void(const std::string&, const Json&)>;

  // Servers can also make requests of clients. The returned JSON becomes the
  // JSON-RPC result; throwing RpcError sends a JSON-RPC error instead.
  using RequestHandler = std::function<Json(const std::string&, const Json&)>;

  Client(Transport& transport, Implementation client_info);

  // Performs the MCP initialize handshake, sends notifications/initialized,
  // and returns the server's initialize result.
  [[nodiscard]] Json initialize(Json capabilities = Json::object());

  // Sends a request and waits for its response. RpcError is thrown when the
  // peer returns a JSON-RPC error.
  [[nodiscard]] Json request(const std::string& method, Json params = Json::object());

  // Sends a one-way message; no response is expected.
  void notify(const std::string& method, Json params = Json::object());

  // Asks the peer to stop work associated with an earlier request ID.
  void cancel(std::int64_t request_id, std::string reason = {});

  // Each setter replaces the previously installed handler of that kind.
  void on_notification(NotificationHandler handler);
  void on_request(RequestHandler handler);

 private:
  // Transport messages are newline-delimited JSON when using StdioTransport.
  void send(const Json& message);

  // Reads until expected_id is found. Notifications and server-to-client
  // requests encountered along the way are dispatched to the callbacks above.
  [[nodiscard]] Json receive_response(std::int64_t expected_id);

  // Non-owning reference: see the lifetime note on Client above.
  Transport& transport_;
  Implementation client_info_;
  std::int64_t next_id_{1};
  NotificationHandler notification_handler_;
  RequestHandler request_handler_;

  // A lock covers the complete send/wait cycle, allowing only one outstanding
  // client request at a time and keeping response matching straightforward.
  std::mutex request_mutex_;
};

}  // namespace mcp
