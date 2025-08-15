#pragma once

#include <string>
#include <map>
#include <functional>
#include <vector>

namespace httplib {

// Common typedefs
using Headers = std::multimap<std::string, std::string>;
using Params  = std::multimap<std::string, std::string>;

// Minimal request/response/stream types

struct Request {
  std::string method;
  std::string path;
  std::string body;
  Headers headers;
  Params params;

  // Non-standard helper some code expects in real impls:
  bool is_upgrade_request() const { return false; }
};

struct Response {
  int status = 200;
  std::string body;
  Headers headers;

  void set_content(const std::string& s, const char* /*content_type*/) {
    body = s;
  }
  void set_header(const std::string& key, const std::string& val) {
    headers.emplace(key, val);
  }

  // For code that tries to set an "upgrade" callback:
  template <typename F>
  void set_upgrade_handler(F /*fn*/) {
    // no-op in shim
  }
};

class Stream {
public:
  size_t read(char* /*buf*/, size_t /*n*/) { return 0; }
  bool send(const std::string& /*data*/) { return true; } // for code that tries to stream WS
};

// Handler signatures
using Handler = std::function<void(const Request&, Response&)>;

template <typename... Args>
using Logger = std::function<void(const Request&, const Response&)>;

template <typename... Args>
using ExceptionHandler = std::function<void(const Request&, Response&, std::exception_ptr)>;

template <typename... Args>
using PreRoutingHandler = std::function<bool(const Request&, Response&)>;

// A very small Server shim that accepts handler registrations but does nothing at runtime
class Server {
public:
  Server() = default;

  // HTTP verbs
  void Get   (const std::string& /*pattern*/, Handler /*h*/) {}
  void Post  (const std::string& /*pattern*/, Handler /*h*/) {}
  void Put   (const std::string& /*pattern*/, Handler /*h*/) {}
  void Delete(const std::string& /*pattern*/, Handler /*h*/) {}
  void Options(const std::string& /*pattern*/, Handler /*h*/) {}

  // Misc server configuration hooks (all no-ops here)
  template <typename F> void set_logger(F /*fn*/) {}
  template <typename F> void set_exception_handler(F /*fn*/) {}
  template <typename F> void set_pre_routing_handler(F /*fn*/) {}
  void set_default_header(const std::string& /*k*/, const std::string& /*v*/) {}
  void set_mount_point(const std::string& /*dir*/, const std::string& /*path*/ = "/") {}
  void remove_mount_point(const std::string& /*path*/) {}
  void set_read_timeout(time_t /*sec*/, time_t /*usec*/ = 0) {}
  void set_write_timeout(time_t /*sec*/, time_t /*usec*/ = 0) {}
  void set_idle_interval(time_t /*sec*/, time_t /*usec*/ = 0) {}
  void set_keep_alive_max_count(size_t /*n*/) {}
  void set_payload_max_length(size_t /*n*/) {}
  void set_tcp_nodelay(bool /*on*/) {}
  template <typename F> void set_socket_options(F /*fn*/) {}

  // Start/stop
  bool listen(const char* /*host*/, int /*port*/) { return true; }
  bool listen_after_bind() { return true; }
  void stop() {}

  // Health
  bool is_valid() const { return true; }

  // Fake “broadcast” used by some WS demo code
  void send_to_all_websocket_connections(const std::string& /*msg*/) {}

private:
  // No actual storage in shim
};

} // namespace httplib
