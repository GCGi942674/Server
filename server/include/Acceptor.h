#ifndef ACCEPTOR_H_
#define ACCEPTOR_H_

#include "EventLoop.h"
#include <functional>

class Acceptor {
public:
  using ServerCallback = std::function<void(int)>;

public:
  Acceptor(EventLoop *loop, int port);
  ~Acceptor();

  bool startListen();
  void handleAccept();
  void stopListen();

  void setNewConnectionCallback(ServerCallback callback);

private:
  int listen_fd_;
  int port_;
  EventLoop *loop_;
  ServerCallback callback_;
};

#endif