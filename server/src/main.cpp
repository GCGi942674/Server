#include "EchoServer.h"
#include "logging.h"

int main() {
  Logger::instance().setLevel(LogLevel::INFO);

  LOG_INFO("server starting...");

  EchoHandler handler;
  EchoServer server(8080, handler);
  server.run();

  LOG_INFO("server stopped!");

  return 0;
}