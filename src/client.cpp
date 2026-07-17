#include "mcp/client.hpp"

#include <stdexcept>
#include <utility>

namespace mcp {

Client::Client(Transport& transport, Implementation client_info)
    : transport_(transport), client_info_(std::move(client_info)) {}

void Client::send(const Json& message) { transport_.write(message.dump()); }

Json Client::initialize(Json capabilities) {
  // initialize is an ordinary request followed by the one-way ready signal.
  Json result = request("initialize", {{"protocolVersion", protocol_version},
                                        {"capabilities", std::move(capabilities)},
                                        {"clientInfo", client_info_}});
  notify("notifications/initialized");
  return result;
}

Json Client::request(const std::string& method, Json params) {
  // Holding this lock while waiting deliberately limits the synchronous client
  // to one in-flight request. That avoids needing a response-routing thread.
  const std::scoped_lock lock(request_mutex_);
  const auto id = next_id_++;
  send({{"jsonrpc", "2.0"}, {"id", id}, {"method", method}, {"params", std::move(params)}});
  return receive_response(id);
}

void Client::notify(const std::string& method, Json params) {
  send({{"jsonrpc", "2.0"}, {"method", method}, {"params", std::move(params)}});
}

void Client::cancel(std::int64_t request_id, std::string reason) {
  Json params = {{"requestId", request_id}};
  if (!reason.empty()) params["reason"] = std::move(reason);
  notify("notifications/cancelled", std::move(params));
}

void Client::on_notification(NotificationHandler handler) {
  notification_handler_ = std::move(handler);
}

void Client::on_request(RequestHandler handler) { request_handler_ = std::move(handler); }

Json Client::receive_response(std::int64_t expected_id) {
  while (const auto wire = transport_.read()) {
    Json message;
    try {
      message = Json::parse(*wire);
    } catch (const Json::parse_error&) {
      continue;
    }
    if (!message.is_object() || message.value("jsonrpc", "") != "2.0") continue;

    // A response has an ID plus either result or error.
    if (message.contains("id") && (message.contains("result") || message.contains("error"))) {
      if (message.at("id") != expected_id) continue;
      if (message.contains("error")) {
        const auto& error = message.at("error");
        const auto code = static_cast<ErrorCode>(error.value("code", static_cast<int>(ErrorCode::internal_error)));
        throw RpcError(code, error.value("message", "JSON-RPC error"), error.value("data", Json(nullptr)));
      }
      return message.value("result", Json::object());
    }

    // Messages with a method are inbound notifications or requests that must be
    // handled while the client is waiting for its own response.
    if (!message.contains("method") || !message.at("method").is_string()) continue;
    const auto method = message.at("method").get<std::string>();
    const auto params = message.value("params", Json::object());
    if (!message.contains("id")) {
      if (notification_handler_) notification_handler_(method, params);
      continue;
    }

    // An inbound method with an ID is a server-to-client request.
    const auto id = message.at("id");
    try {
      if (!request_handler_) throw RpcError(ErrorCode::method_not_found, "client method not supported");
      send({{"jsonrpc", "2.0"}, {"id", id}, {"result", request_handler_(method, params)}});
    } catch (const RpcError& error) {
      Json body = {{"code", static_cast<int>(error.code())}, {"message", error.what()}};
      if (!error.data().is_null()) body["data"] = error.data();
      send({{"jsonrpc", "2.0"}, {"id", id}, {"error", std::move(body)}});
    } catch (const std::exception& error) {
      send({{"jsonrpc", "2.0"}, {"id", id},
            {"error", {{"code", static_cast<int>(ErrorCode::internal_error)}, {"message", error.what()}}}});
    }
  }
  throw std::runtime_error("transport closed while waiting for a response");
}

}  // namespace mcp
