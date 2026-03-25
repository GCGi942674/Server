#include "EchoServer.h"
#include "logging.h"
#include <signal.h>
#include <unistd.h>

EchoServer *g_server = nullptr;

void handle_sigint(int) {
  if (g_server) {
    LOG_INFO("SIGINT receieved, stopping server...");
    g_server->stop();
  }
}

int main() {
  Logger::instance().setLevel(LogLevel::INFO);

  signal(SIGINT, handle_sigint);

  LOG_INFO("server starting...");

  EchoHandler handler;
  EchoServer server(8080, handler);

  g_server = &server;

  server.run();

  LOG_INFO("server stopped!");

  return 0;
}