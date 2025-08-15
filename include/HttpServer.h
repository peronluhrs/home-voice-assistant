#pragma once
#include <string>

struct HttpOpts {
  std::string host = "127.0.0.1";
  int         port = 8787;
  std::string bearer;
  bool        enable_ws = true;
};

class HttpServer {
public:
  explicit HttpServer(const HttpOpts&, void*, void*, void*, void*) {}
  ~HttpServer() {}

  bool start()            { return true; }   // Pretend OK so main() proceeds
  void stop()             {}
  bool isRunning() const  { return true; }
  void pushEvent(const std::string&) {}      // no-op
};
