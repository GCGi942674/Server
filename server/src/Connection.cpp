#include "Connection.h"
#include <errno.h>
#include <iostream>
#include <sys/socket.h>
#include <unistd.h>

Connection::Connection(int fd) : fd_(fd), state_(ConnState::Connected) {}

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
      this->setState(ConnState::Disconnected);
      return false; //客户端关闭
    } else {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        break;
      }
      if (errno == EINTR) {
        continue;
      }
      this->setState(ConnState::Disconnected);
      return false;
    }
  }
  return true;
}

bool Connection::handleWrite() {
  while (!this->write_buffer_.empty()) {
    ssize_t n = ::send(this->fd_, this->write_buffer_.data(),
                       this->write_buffer_.size(), 0);
    if (n > 0) {
      this->write_buffer_.erase(0, n);
    } else if (n < 0) {
      if (errno == EINTR) {
        continue;
      }
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        return true;
      }
      this->setState(ConnState::Disconnected);
      return false;
    } else {
      this->setState(ConnState::Disconnected);
      return false;
    }
  }
  return true;
}

void Connection::setMessageCallback(MessageCallback cb) {
  this->on_message_ = std::move(cb);
}

void Connection::sendPacket(const std::vector<char> &packet) {
  this->write_buffer_.append(packet.data(), packet.size());
}

void Connection::send(const std::string &data) { this->write_buffer_ += data; }

bool Connection::wantWrite() const { return !this->write_buffer_.empty(); }

Connection::ConnState Connection::state() const { return this->state_; }

void Connection::setState(Connection::ConnState st) { this->state_ = st; }

bool Connection::isConnected() const {
  return this->state_ == Connection::ConnState::Connected;
}

bool Connection::isDisconnecting() const {
  return this->state_ == Connection::ConnState::Disconnecting;
}

bool Connection::isDisconnected() const {
  return this->state_ == Connection::ConnState::Disconnected;
}