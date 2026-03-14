#ifndef CONNECTION_H
#define CONNECTION_H

#include "protocol/message_codec.h"
#include <functional>
#include <string>

class Connection {
public:
  using MessageCallback = std::function<void(int, const std::string &)>;

  enum class ConnState { Connected, Disconnecting, Disconnected };

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

  void send(const std::string &data);

  void sendPacket(const std::vector<char> &packet);

  ConnState state() const;

  void setState(ConnState st);

  bool isConnected() const;

  bool isDisconnecting() const;

  bool isDisconnected() const;

private:
  int fd_;
  ConnState state_;
  MessageCallback on_message_;
  MessageCodec::Decoder decoder_;
  std::string write_buffer_;
};

#endif