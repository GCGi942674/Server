#ifndef ECHO_SERVER_H
#define ECHO_SERVER_H

#include "Connection.h"
#include "EchoHandler.h"
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
  void onMessage(int fd, const std::string &msg);
  void queueInLoop(std::function<void()> task);
  void doPendingTasks();

private:
  void handleAccept();
  void handleClient(int client_fd);
  void removeConnection(int client_fd);
  void updateEpoll(int client_fd, bool want_wrtie);

  EchoHandler &handler_;
  std::unordered_map<int, std::unique_ptr<Connection>> connections_;
  int listen_fd_;
  int epfd_;
  int port_;
  int wakeup_fd_;
  ThreadPool pool_;
  std::mutex mutex_;
  std::queue<std::function<void()>> pending_tasks_;
};

#endif // ECHO_SERVER_H