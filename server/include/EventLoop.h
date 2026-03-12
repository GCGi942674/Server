#ifndef EVENT_LOOP_H_
#define EVENT_LOOP_H_

#include <functional>
#include <mutex>
#include <queue>
#include <sys/epoll.h>
#include <unordered_map>

class EventLoop {
public:
  using Functor = std::function<void()>;
  using EventCallback = std::function<void(uint32_t)>;

public:
  EventLoop();
  ~EventLoop();

  void loop();
  void quit();

  void addFd(int fd, uint32_t events, EventCallback cb);
  void updateFd(int fd, uint32_t events);
  void removeFd(int fd);

  void queueInLoop(Functor task);

private:
  void doPending();
  void handleWakeUp();

private:
  int epfd_;
  int wakeup_fd_;
  bool quit_;

  std::mutex mutex_;
  std::queue<Functor> pending_tasks_;
  std::unordered_map<int, EventCallback> callbacks_;
};

#endif