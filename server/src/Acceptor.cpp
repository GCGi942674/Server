#include "Acceptor.h"
#include "logging.h"
#include "utils.h"
#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <sys/eventfd.h>
#include <unistd.h>

Acceptor::Acceptor(EventLoop *loop, int port)
    : loop_(loop), port_(port), listen_fd_(-1) {}

Acceptor::~Acceptor() {
  if (listen_fd_ != -1) {
    LOG_INFO("close listen_fd=" << this->listen_fd_);
    close(this->listen_fd_);
    this->listen_fd_ = -1;
  }
}

void Acceptor::startListen() {
  this->listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);

  if (this->listen_fd_ < 0) {
    LOG_ERROR("socket failed, error = " << errno
                                        << ", err = " << strerror(errno));
    return;
  }

  int opt = 1;
  setsockopt(this->listen_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  setNonBlocking(listen_fd_);

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(this->port_);
  addr.sin_addr.s_addr = INADDR_ANY;

  if (bind(this->listen_fd_, (sockaddr *)&addr, sizeof(addr)) < 0) {
    LOG_ERROR("bind failed, fd=" << this->listen_fd_ << ", port=" << this->port_
                                 << ", errno=" << errno
                                 << ", err=" << strerror(errno));
    return;
  }

  if (listen(this->listen_fd_, 128) < 0) {
    LOG_ERROR("listen failed, fd=" << this->listen_fd_ << ", errno=" << errno
                                   << ", err=" << strerror(errno));
    return;
  }

  LOG_INFO("listen started, fd=" << this->listen_fd_
                                 << ", port=" << this->port_);

  this->loop_->addFd(this->listen_fd_, EPOLLIN,
                     [this](uint32_t event) { this->handleAccept(); });
}

void Acceptor::handleAccept() {
  while (true) {
    int client_fd = accept(this->listen_fd_, nullptr, nullptr);
    if (client_fd >= 0) {
      setNonBlocking(client_fd);
      LOG_INFO("accept new connection, client_fd=" << client_fd);
      if (this->callback_) {
        this->callback_(client_fd);
      } else {
        LOG_WARN("new connection callback not set, client_fd=" << client_fd);
        close(client_fd);
      }
    } else {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        break;
      }
      if (errno == EINTR) {
        continue;
      }
      LOG_ERROR("accept failed, errno=" << errno
                                        << ", err=" << strerror(errno));
      break;
    }
  }
}

void Acceptor::setNewConnectionCallback(ServerCallback callback) {
  this->callback_ = std::move(callback);
}