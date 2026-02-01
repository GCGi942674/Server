#ifndef ECHO_SERVER_H
#define ECHO_SERVER_H

#include <sys/epoll.h>
#include <unordered_map>
#include "protocol/message_codec.h"

class EchoServer {
public:
  EchoServer(int port);
  ~EchoServer();
  void run();

private:
  void handleeAccept();
  void handleClient(int client_fd);

  std::unordered_map<int, MessageCodec::Decoder> decoders_;
  int listen_fd_;
  int epfd_;
  int port_;
};

#endif // ECHO_SERVER_H