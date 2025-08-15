#pragma once
#include <string>

struct HttpOpts {
  std::string host = "127.0.0.1";
  int         port = 8787;
  std::string bearer; // if not empty, require Authorization: Bearer <token>
  bool        enable_ws = true;
};

class HttpServer {
public:
  explicit HttpServer(const HttpOpts& opts);
  ~HttpServer();
  bool start();  // background thread
  void stop();   // join
  bool isRunning() const;
  // add a method to push WS event JSON (no-op if ws disabled)
  void pushEvent(const std::string& jsonLine);
private:
  struct Impl; Impl* impl_;
};
