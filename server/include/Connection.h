#ifndef CONNECTION_H
#define CONNECTION_H

#include "protocol/message_codec.h"
#include "EchoHandler.h"
#include <string>

class Connection {
public:
  explicit Connection(int fd, EchoHandler& handler);
  ~Connection();
  int fd() const;

  //处理读事件
  bool handleRead();

  //处理写事件
  bool handleWrite();

  //是否需要监听写事件
  bool wantWrite() const;

private:
  EchoHandler& handler_;
  int fd_;
  MessageCodec::Decoder decoder_;
  std::string wirte_buffer;
};

#endif