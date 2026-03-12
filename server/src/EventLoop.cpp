#include "EventLoop.h"
#include <cerrno>
#include <cstring>
#include <iostream>
#include <sys/eventfd.h>
#include <unistd.h>

EventLoop::EventLoop() : epfd_(-1), wakeup_fd_(-1), quit_(false) {
  this->epfd_ = epoll_create1(0);
  if (this->epfd_ < 0) {
    std::cerr << "epoll_create1 error !" << std::endl;
    std::abort();
  }
  this->wakeup_fd_ = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
  if (this->wakeup_fd_ < 0) {
    std::cerr << "eventfd error !" << std::endl;
    std::abort();
  }

  epoll_event ev{};
  ev.data.fd = this->wakeup_fd_;
  ev.events = EPOLLIN;
  if (epoll_ctl(this->epfd_, EPOLL_CTL_ADD, this->wakeup_fd_, &ev) < 0) {
    std::cerr << "epoll_ctl add Wakeup_fd_ error :" << strerror(errno)
              << std::endl;
    std::abort();
  }
}

EventLoop::~EventLoop() {
  if (this->epfd_ != -1) {
    close(this->epfd_);
    this->epfd_ = -1;
  }
  if (this->wakeup_fd_ != -1) {
    close(this->wakeup_fd_);
    this->wakeup_fd_ = -1;
  }
}

void EventLoop::loop() {
  epoll_event events[1024];
  while (!this->quit_) {
    int nready = epoll_wait(this->epfd_, events, 1024, -1);
    if (nready < 0) {
      if (errno == EINTR) {
        continue;
      }
      std::cerr << "epoll_wait error !" << std::endl;
      break;
    }

    for (int i = 0; i < nready; ++i) {
      int fd = events[i].data.fd;
      uint32_t event = events[i].events;

      if (fd == this->wakeup_fd_) {
        this->handleWakeUp();
        continue;
      }

      auto it = this->callbacks_.find(fd);
      if (it != this->callbacks_.end()) {
        it->second(event);
      }
    }

    this->doPending();
  }
}

void EventLoop::quit() {
  this->quit_ = true;
  this->queueInLoop([]() {});
}

void EventLoop::addFd(int fd, uint32_t events, EventCallback cb) {
  epoll_event ev{};
  ev.data.fd = fd;
  ev.events = events;
  if (epoll_ctl(this->epfd_, EPOLL_CTL_ADD, fd, &ev) < 0) {
    std::cerr << "add fd error :" << strerror(errno) << std::endl;
    return;
  }
  this->callbacks_[fd] = cb;
}

void EventLoop::updateFd(int fd, uint32_t events) {
  epoll_event ev{};
  ev.data.fd = fd;
  ev.events = events;
  if (epoll_ctl(this->epfd_, EPOLL_CTL_MOD, fd, &ev) < 0) {
    std::cerr << "update fd error :" << strerror(errno) << std::endl;
  }
}

void EventLoop::removeFd(int fd) {
  epoll_ctl(this->epfd_, EPOLL_CTL_DEL, fd, nullptr);
  this->callbacks_.erase(fd);
}

void EventLoop::queueInLoop(Functor task) {
  bool need_wakeup = false;
  {
    std::lock_guard<std::mutex> lock(this->mutex_);
    need_wakeup = this->pending_tasks_.empty();
    this->pending_tasks_.push(std::move(task));
  }
  if (need_wakeup) {
    uint64_t one = 1;
    ssize_t n = write(this->wakeup_fd_, &one, sizeof(one));
    (void)n;
  }
}

void EventLoop::doPending() {
  std::queue<Functor> tasks;
  {
    std::lock_guard<std::mutex> lock(this->mutex_);
    if (this->pending_tasks_.empty()) {
      return;
    }
    this->pending_tasks_.swap(tasks);
  }
  while (!tasks.empty()) {
    tasks.front()();
    tasks.pop();
  }
}

void EventLoop::handleWakeUp() {
  uint64_t one;
  while (true) {
    ssize_t n = read(this->wakeup_fd_, &one, sizeof(one));
    if (n > 0) {
      continue;
    }
    if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
      break;
    }
    if (n < 0 && (errno == EINTR)) {
      continue;
    }
    break;
  }
}