#ifndef ECHO_SERVER_H
#define ECHO_SERVER_H

#include "Acceptor.h"
#include "Connection.h"
#include "EchoHandler.h"
#include "EventLoop.h"
#include "protocol/message_codec.h"
#include "Thread_pool.h"
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <sys/epoll.h>
#include <unordered_map>

class EchoServer {
public:
  EchoServer(int port, EchoHandler &handler, int signal_fd);
  ~EchoServer();
  void run();
  void beginShutdown();
  void tryFinishShutdown();
  void onMessage(const std::shared_ptr<Connection> &conn,
                 const std::string &msg);

private:
  void handleNewConnection(int client_fd);
  void handleClientEvent(int client_fd, uint32_t events);
  void removeConnection(int client_fd);
  void updateConnectionEvent(int client_fd, bool want_wrtie);

private:
  uint64_t idle_timeout_ms_{60000};
  uint64_t shutdown_timeout_ms_{30000};
  EventLoop::TimerId shutdown_timer_{0};
  EventLoop::TimerId idle_check_timer_{0};

  EchoHandler &handler_;
  Acceptor acceptor_;
  EventLoop loop_;
  std::unordered_map<int, std::shared_ptr<Connection>> connections_;
  ThreadPool pool_;
  std::atomic<bool> stopping_{false};
  int signal_fd{-1};
};

#endif // ECHO_SERVER_H