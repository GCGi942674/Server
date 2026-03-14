#include "Connection.h"
#include <errno.h>
#include <iostream>
#include <sys/socket.h>
#include <unistd.h>

Connection::Connection(int fd) : fd_(fd) {}

Connection::~Connection() {
  if (this->fd_ != -1) {
    close(this->fd_);
    this->fd_ = -1;
  }
}

int Connection::fd() const { return fd_; }

bool Connection::handleRead() {
  char buffer[4096];

  while (true) {
    ssize_t n = recv(this->fd_, buffer, sizeof(buffer), 0);

    if (n > 0) {
      this->decoder_.append(buffer, n);
      std::string msg;
      while (this->decoder_.tryDecode(msg)) {
        if (this->on_message_) {
          this->on_message_(this->fd_, msg);
        }
      }
    } else if (n == 0) {
      return false; //客户端关闭
    } else {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        break;
      }
      return false;
    }
  }
  return true;
}

bool Connection::handleWrite() {
  while (!this->wirte_buffer.empty()) {
    ssize_t n = ::send(this->fd_, this->wirte_buffer.data(),
                       this->wirte_buffer.size(), 0);
    if (n > 0) {
      this->wirte_buffer.erase(0, n);
    } else if (n < 0) {
      if (errno == EINTR) {
        continue;
      }
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        return true;
      }
      return false;
    } else {
      return false;
    }
  }
  return true;
}

void Connection::setMessageCallback(MessageCallback cb) {
  this->on_message_ = std::move(cb);
}

void Connection::sendPacket(const std::vector<char> &packet) {
  this->wirte_buffer.append(packet.data(), packet.size());
}

void Connection::send(const std::string &data) { this->wirte_buffer += data; }

bool Connection::wantWrite() const { return !this->wirte_buffer.empty(); }