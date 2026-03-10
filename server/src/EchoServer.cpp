#include "EchoServer.h"
#include "utils.h" // 来自 common
#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <sys/eventfd.h>
#include <unistd.h>

EchoServer::EchoServer(int port, EchoHandler &handler)
    : listen_fd_(-1), epfd_(-1), port_(port), handler_(handler),
      wakeup_fd_(-1) {}

EchoServer::~EchoServer() {
  if (this->listen_fd_ != -1) {
    close(this->listen_fd_);
  }

  if (this->epfd_ != -1) {
    close(this->epfd_);
  }

  if (this->wakeup_fd_ != -1) {
    close(this->wakeup_fd_);
  }
}

void EchoServer::onMessage(int fd, const std::string &msg) {
  this->pool_.addTask([this, fd, msg]() -> void {
    auto resp = this->handler_.onMessage(msg);
    auto packet = MessageCodec::encode(resp);
    this->queueInLoop([this, fd, packet]() -> void {
      auto iter = this->connections_.find(fd);
      if (iter == this->connections_.end()) {
        return;
      }
      iter->second->sendPacket(packet);
      this->updateEpoll(fd, iter->second->wantWrite());
    });
  });
}

void EchoServer::run() {
  this->listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);
  setNonBlocking(listen_fd_);

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(this->port_);
  addr.sin_addr.s_addr = INADDR_ANY;

  bind(this->listen_fd_, (sockaddr *)&addr, sizeof(addr));
  listen(this->listen_fd_, 128);

  this->epfd_ = epoll_create1(0);
  epoll_event ev{};
  ev.events = EPOLLIN;
  ev.data.fd = this->listen_fd_;
  epoll_ctl(this->epfd_, EPOLL_CTL_ADD, this->listen_fd_, &ev);

  this->wakeup_fd_ = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
  epoll_event wev{};
  wev.events = EPOLLIN;
  wev.data.fd = this->wakeup_fd_;
  epoll_ctl(this->epfd_, EPOLL_CTL_ADD, this->wakeup_fd_, &wev);

  epoll_event events[1024];
  while (true) {
    int nready = epoll_wait(this->epfd_, events, 1024, -1);

    this->doPendingTasks();

    for (int i = 0; i < nready; ++i) {
      int fd = events[i].data.fd;

      if (this->listen_fd_ == fd) {
        this->handleAccept();
        continue;
      }

      if (this->wakeup_fd_ == fd) {
        this->doPendingTasks();
        continue;
      }

      auto iter = this->connections_.find(fd);
      if (iter == this->connections_.end()) {
        continue;
      }
      Connection *conn = iter->second.get();

      uint32_t ev = events[i].events;

      if (ev & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
        this->removeConnection(fd);
        continue;
      }

      if (events[i].events & EPOLLIN) {
        if (!conn->handleRead()) {
          this->removeConnection(fd);
          continue;
        }
      }

      if (events[i].events & EPOLLOUT) {
        if (!conn->handleWrite()) {
          this->removeConnection(fd);
          continue;
        }
      }

      this->updateEpoll(fd, conn->wantWrite());
    }
  }
}

void EchoServer::handleAccept() {
  while (true) {
    int client_fd = accept(this->listen_fd_, nullptr, nullptr);
    if (client_fd >= 0) {
      setNonBlocking(client_fd);
      this->connections_.emplace(client_fd,
                                 std::make_unique<Connection>(client_fd));
      auto iter = this->connections_.find(client_fd);
      if (iter != this->connections_.end()) {
        iter->second->setMessageCallback(
            [this](int fd, const std::string &msg) -> void {
              this->onMessage(fd, msg);
            });
      }

      epoll_event cev{};
      cev.events = EPOLLIN | EPOLLRDHUP;
      cev.data.fd = client_fd;
      epoll_ctl(this->epfd_, EPOLL_CTL_ADD, client_fd, &cev);
    } else {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        break;
      } else {
        std::cerr << strerror(errno);
        break;
      }
    }
  }
}

void EchoServer::removeConnection(int client_fd) {
  epoll_ctl(this->epfd_, EPOLL_CTL_DEL, client_fd, nullptr);
  this->connections_.erase(client_fd);
}

void EchoServer::queueInLoop(std::function<void()> task) {
  bool wake_up = false;
  {
    this->mutex_.lock();
    if (this->pending_tasks_.empty()) {
      wake_up = true;
    }
    this->pending_tasks_.push(task);
    this->mutex_.unlock();
  }
  if (wake_up) {
    uint64_t one = 1;
    ssize_t n = write(this->wakeup_fd_, &one, sizeof(one));
    (void)n;
  }
}

void EchoServer::doPendingTasks() {
  std::queue<std::function<void()>> tasks;
  {
    std::lock_guard<std::mutex> lock(this->mutex_);
    if (this->pending_tasks_.empty()) {
      return;
    }
    tasks.swap(this->pending_tasks_);
  }
  while (!tasks.empty()) {
    tasks.front()();
    tasks.pop();
  }
}

void EchoServer::updateEpoll(int client_fd, bool want_write) {
  struct epoll_event ev;
  ev.data.fd = client_fd;
  ev.events = EPOLLIN | EPOLLRDHUP;
  if (want_write) {
    ev.events |= EPOLLOUT;
  }
  epoll_ctl(this->epfd_, EPOLL_CTL_MOD, client_fd, &ev);
}