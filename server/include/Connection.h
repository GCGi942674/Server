#ifndef CONNECTION_H
#define CONNECTION_H

#include "protocol/message_codec.h"
#include <functional>
#include <string>

class Connection {
public:
  using MessageCallback = std::function<void(int, const std::string &)>;

public:
  explicit Connection(int fd);
  ~Connection();
  int fd() const;

  void setMessageCallback(MessageCallback cb);

  //处理读事件
  bool handleRead();

  //处理写事件
  bool handleWrite();

  //是否需要监听写事件
  bool wantWrite() const;

  void sendPacket(const std::vector<char>& packet);

private:
  int fd_;
  MessageCallback on_message_;
  MessageCodec::Decoder decoder_;
  std::string wirte_buffer;
};

#endif