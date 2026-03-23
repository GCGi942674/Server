#ifndef ECHO_SERVER_H
#define ECHO_SERVER_H

#include "Acceptor.h"
#include "Connection.h"
#include "EchoHandler.h"
#include "EventLoop.h"
#include "protocol/message_codec.h"
#include "thread_pool.h"
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <sys/epoll.h>
#include <unordered_map>

class EchoServer {
public:
  EchoServer(int port, EchoHandler &handler);
  ~EchoServer();
  void run();
  void onMessage(const std::shared_ptr<Connection> &conn,
                 const std::string &msg);

private:
  void handleNewConnection(int client_fd);
  void handleClientEvent(int client_fd, uint32_t events);
  void removeConnection(int client_fd);
  void updateConnectionEvent(int client_fd, bool want_wrtie);

  EchoHandler &handler_;
  Acceptor acceptor_;
  EventLoop loop_;
  std::unordered_map<int, std::shared_ptr<Connection>> connections_;
  ThreadPool pool_;
};

#endif // ECHO_SERVER_H