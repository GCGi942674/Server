#include "EchoServer.h"
#include "Logging.h"
#include <signal.h>
#include <sys/eventfd.h>
#include <unistd.h>

static int g_signal_fd = -1;

void handle_sigint(int) {
  uint64_t one = 1;
  ssize_t n = ::write(g_signal_fd, &one, sizeof(one));
  (void)n;
}

int main() {
  Logger::instance().setLevel(LogLevel::INFO);

  LOG_INFO("server starting...");

  g_signal_fd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);

  struct sigaction sa {};
  sa.sa_handler = handle_sigint;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  sigaction(SIGINT, &sa, nullptr);
  sigaction(SIGTERM, &sa, nullptr);

  EchoHandler handler;
  EchoServer server(8080, handler, g_signal_fd);

  server.run();

  if (g_signal_fd != -1) {
    ::close(g_signal_fd);
    g_signal_fd = -1;
  }

  LOG_INFO("server stopped!");

  return 0;
}