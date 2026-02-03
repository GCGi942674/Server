#include "echo_server.h"
#include "utils.h" // 来自 common
#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <unistd.h>

EchoServer::EchoServer(int port) : listen_fd_(-1), epfd_(-1), port_(port) {}

EchoServer::~EchoServer() {
  if (this->listen_fd_ != -1) {
    close(this->listen_fd_);
  }

  if (this->epfd_ != -1) {
    close(this->epfd_);
  }
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

  epoll_event events[1024];
  while (true) {
    int nready = epoll_wait(this->epfd_, events, 1024, -1);
    for (int i = 0; i < nready; ++i) {
      int fd = events[i].data.fd;

      if (this->listen_fd_ == fd) {
        this->handleeAccept();
      } else {
        this->handleClient(fd);
      }
    }
  }
}

void EchoServer::handleeAccept() {
  int client_fd = accept(this->listen_fd_, nullptr, nullptr);
  if (client_fd > 0) {
    setNonBlocking(client_fd);
    epoll_event cev{};
    cev.events = EPOLLIN;
    cev.data.fd = client_fd;
    epoll_ctl(this->epfd_, EPOLL_CTL_ADD, client_fd, &cev);
  }
}

void EchoServer::handleClient(int client_fd) {
  char buffer[1024];
  while (true) {
    ssize_t n = recv(client_fd, buffer, sizeof(buffer), 0);
    if (n > 0) {
      this->decoders_[client_fd].append(buffer, n);

      std::string msg;
      while (this->decoders_[client_fd].tryDecode(msg)) {
        auto resp = MessageCodec::encode(msg);
        if (!sendAll(client_fd, resp.data(), resp.size())) {
          epoll_ctl(this->epfd_, EPOLL_CTL_DEL, client_fd, nullptr);
          close(client_fd);
          this->decoders_.erase(client_fd);
          return;
        }
      }

    } else if (n == 0) {
      epoll_ctl(this->epfd_, EPOLL_CTL_DEL, client_fd, nullptr);
      close(client_fd);
      this->decoders_.erase(client_fd);
      return;
    } else {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        break;
      } else {
        epoll_ctl(this->epfd_, EPOLL_CTL_DEL, client_fd, nullptr);
        close(client_fd);
        this->decoders_.erase(client_fd);
      }
      return;
    }
  }
}