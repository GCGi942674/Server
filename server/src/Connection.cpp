#include "Connection.h"
#include <errno.h>
#include <sys/socket.h>
#include <unistd.h>

Connection::Connection(int fd) : fd_(fd) {}

Connection::~Connection() { close(this->fd_); }

int Connection::fd() const { return fd_; }

bool Connection::handleRead() {
  char buffer[1024];

  while (true) {
    ssize_t n = recv(this->fd_, buffer, sizeof(buffer), 0);

    if (n > 0) {
      this->decoder_.append(buffer, n);
      std::string message;
      while (this->decoder_.tryDecode(message)) {
        auto resp = MessageCodec::encode(message);
        this->wirte_buffer.append(resp.data(), resp.size());
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
    ssize_t n = send(this->fd_, this->wirte_buffer.data(),
                     this->wirte_buffer.size(), 0);
    if (n > 0) {
      this->wirte_buffer.erase(0, n);
    } else {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        return true;
      }
      return false;
    }
  }
  return true;
}

bool Connection::wantWrite() const { return !this->wirte_buffer.empty(); }