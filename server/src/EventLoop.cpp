#include "EventLoop.h"
#include "logging.h"
#include <cerrno>
#include <cstring>
#include <iostream>
#include <sys/eventfd.h>
#include <unistd.h>

EventLoop::EventLoop() : epfd_(-1), wakeup_fd_(-1), quit_(false) {
  this->epfd_ = epoll_create1(0);
  if (this->epfd_ < 0) {
    LOG_ERROR("epoll_create1 failed, errno=" << errno
                                             << ", err=" << strerror(errno));
    std::abort();
  }
  this->wakeup_fd_ = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
  if (this->wakeup_fd_ < 0) {
    LOG_ERROR("eventfd failed, errno=" << errno << ", err=" << strerror(errno));
    std::abort();
  }

  epoll_event ev{};
  ev.data.fd = this->wakeup_fd_;
  ev.events = EPOLLIN;
  if (epoll_ctl(this->epfd_, EPOLL_CTL_ADD, this->wakeup_fd_, &ev) < 0) {
    LOG_ERROR("epoll_ctl add wakeup fd failed, errno=" << errno << ", err="
                                                       << strerror(errno));
    std::abort();
  }
  LOG_INFO("event loop initialized, epfd=" << this->epfd_ << ", wakeup_fd="
                                           << this->wakeup_fd_);
}

EventLoop::~EventLoop() {
  LOG_INFO("event loop destroying, epfd=" << this->epfd_ << ", wakeup_fd="
                                          << this->wakeup_fd_);
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
  LOG_INFO("event loop started");
  epoll_event events[1024];
  while (!this->quit_) {
    int nready = epoll_wait(this->epfd_, events, 1024, -1);
    if (nready < 0) {
      if (errno == EINTR) {

        continue;
      }
      LOG_ERROR("epoll_wait failed, errno=" << errno
                                            << ", err=" << strerror(errno));
      break;
    }

    LOG_DEBUG("epoll_wait returned nready=" << nready);

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
      } else {
        LOG_WARN("callback not found for fd=" << fd);
      }
    }

    this->doPending();
  }
  LOG_INFO("event loop exited");
}

void EventLoop::quit() {
  LOG_INFO("event loop quit requested");
  this->quit_ = true;
  this->queueInLoop([]() {});
}

void EventLoop::addFd(int fd, uint32_t events, EventCallback cb) {
  epoll_event ev{};
  ev.data.fd = fd;
  ev.events = events;
  if (epoll_ctl(this->epfd_, EPOLL_CTL_ADD, fd, &ev) < 0) {
    LOG_ERROR("add fd failed, fd=" << fd << ", errno=" << errno
                                   << ", err=" << strerror(errno));
    return;
  }
  this->callbacks_[fd] = cb;
  LOG_DEBUG("fd added to epoll, fd=" << fd << ", events=" << events);
}

void EventLoop::updateFd(int fd, uint32_t events) {
  epoll_event ev{};
  ev.data.fd = fd;
  ev.events = events;
  if (epoll_ctl(this->epfd_, EPOLL_CTL_MOD, fd, &ev) < 0) {
    LOG_ERROR("update fd failed, fd=" << fd << ", errno=" << errno
                                      << ", err=" << strerror(errno));
    return;
  }
  LOG_DEBUG("fd updated, fd=" << fd << ", events=" << events);
}

void EventLoop::removeFd(int fd) {
  if (epoll_ctl(this->epfd_, EPOLL_CTL_DEL, fd, nullptr) < 0) {
    LOG_WARN("remove fd failed, fd=" << fd << ", errno=" << errno
                                     << ", err=" << strerror(errno));
  } else {
    LOG_INFO("fd removed from epoll, fd=" << fd);
  }
  this->callbacks_.erase(fd);
}

void EventLoop::queueInLoop(Functor task) {
  bool need_wakeup = false;
  size_t pending_size = 0;
  {
    std::lock_guard<std::mutex> lock(this->mutex_);
    need_wakeup = this->pending_tasks_.empty();
    this->pending_tasks_.push(std::move(task));
    pending_size = this->pending_tasks_.size();
  }
  LOG_DEBUG("task queued into loop, pending_size="
            << pending_size << ", need_wakeup=" << need_wakeup);
  if (need_wakeup) {
    uint64_t one = 1;
    ssize_t n = write(this->wakeup_fd_, &one, sizeof(one));
    if (n <= 0 && errno != EAGAIN) {
      LOG_ERROR("wakeup write failed, errno=" << errno
                                              << ", err=" << strerror(errno));
    }
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

  LOG_DEBUG("run pending tasks, count=" << tasks.size());

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
  LOG_DEBUG("wakeup fd drained");
}