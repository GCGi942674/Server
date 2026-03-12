#include "EchoServer.h"
#include "utils.h" // 来自 common
#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <sys/eventfd.h>
#include <unistd.h>

EchoServer::EchoServer(int port, EchoHandler &handler)
    : listen_fd_(-1), port_(port), handler_(handler) {}

EchoServer::~EchoServer() {
  if (this->listen_fd_ != -1) {
    close(this->listen_fd_);
    this->listen_fd_ = -1;
  }
}

void EchoServer::onMessage(int fd, const std::string &msg) {
  this->pool_.addTask([this, fd, msg]() -> void {
    auto resp = this->handler_.onMessage(msg);
    auto packet = MessageCodec::encode(resp);
    this->loop_.queueInLoop([this, fd, packet]() -> void {
      auto iter = this->connections_.find(fd);
      if (iter == this->connections_.end()) {
        return;
      }
      iter->second->sendPacket(packet);
      this->updateConnectionEvent(fd, iter->second->wantWrite());
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

  this->loop_.addFd(this->listen_fd_, EPOLLIN,
                    [this](uint32_t event) { this->handleAccept(); });

  this->loop_.loop();
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

      this->loop_.addFd(client_fd, EPOLLIN | EPOLLRDHUP,
                        [this, client_fd](uint32_t events) {
                          this->handleClientEvent(client_fd, events);
                        });
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

void EchoServer::handleClientEvent(int client_fd, uint32_t events) {
  auto iter = this->connections_.find(client_fd);
  if (iter == this->connections_.end()) {
    return;
  }
  Connection *conn = iter->second.get();

  if (events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
    this->removeConnection(client_fd);
    return;
  }

  if (events & EPOLLIN) {
    if (!conn->handleRead()) {
      this->removeConnection(client_fd);
      return;
    }
  }

  if (events & EPOLLOUT) {
    if (!conn->handleWrite()) {
      this->removeConnection(client_fd);
      return;
    }
  }
  this->updateConnectionEvent(client_fd, iter->second->wantWrite());
}

void EchoServer::removeConnection(int client_fd) {
  this->loop_.removeFd(client_fd);
  this->connections_.erase(client_fd);
}

void EchoServer::updateConnectionEvent(int client_fd, bool want_write) {
  uint32_t events = EPOLLIN | EPOLLRDHUP;
  if (want_write) {
    events |= EPOLLOUT;
  }
  this->loop_.updateFd(client_fd, events);
}