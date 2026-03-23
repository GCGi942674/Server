#ifndef CONNECTION_H
#define CONNECTION_H

#include "Buffer.h"
#include "protocol/message_codec.h"
#include <atomic>
#include <functional>
#include <memory>
#include <string>

class Connection : public std::enable_shared_from_this<Connection> {
public:
  using MessageCallback = std::function<void(
      const std::shared_ptr<Connection> &conn, const std::string &)>;

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

  void shutdown();

  bool shouldCloseAfterWrite() const;

  void incPendingTasks();

  void decPendingTasks();

  bool hasPendingTasks() const;

  bool canBeClosed() const;

private:
  int fd_;
  ConnState state_;
  MessageCallback on_message_;

  Buffer inputBuffer_;
  Buffer outputBuffer_;

  std::atomic<int> pending_tasks_{0};
};

#endif