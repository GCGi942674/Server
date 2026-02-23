#ifndef ECHO_SERVER_H
#define ECHO_SERVER_H

#include "Connection.h"
#include "protocol/message_codec.h"
#include <memory>
#include <sys/epoll.h>
#include <unordered_map>

class EchoServer {
public:
  EchoServer(int port);
  ~EchoServer();
  void run();

private:
  void handleAccept();
  void handleClient(int client_fd);
  void removeConnection(int client_fd);
  void updateEpoll(int client_fd, bool want_wrtie);

  std::unordered_map<int, std::unique_ptr<Connection>> connections_;
  int listen_fd_;
  int epfd_;
  int port_;
};

#endif // ECHO_SERVER_H