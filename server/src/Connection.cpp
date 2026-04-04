#include "Connection.h"
#include "logging.h"
#include <cstring>
#include <errno.h>
#include <iostream>
#include <sys/socket.h>
#include <unistd.h>

Connection::Connection(int fd) : fd_(fd), state_(ConnState::Connected) {
  this->refreshActivity();
  LOG_INFO("connection created, fd=" << this->fd_);
}

Connection::~Connection() {
  LOG_INFO("connection destroying, fd=" << this->fd_);
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
      this->refreshActivity();
      LOG_DEBUG("recv success, fd=" << this->fd_ << ", bytes=" << n);
      this->inputBuffer_.append(buffer, static_cast<size_t>(n));

    } else if (n == 0) {
      LOG_INFO("peer closed connection, fd=" << this->fd_);
      this->shutdown();
      break;
    } else {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        break;
      }
      if (errno == EINTR) {
        continue;
      }
      LOG_ERROR("recv failed, fd=" << this->fd_ << ", errno=" << errno
                                   << ", err=" << strerror(errno));
      this->setState(ConnState::Disconnected);
      return false;
    }
  }
  std::string msg;
  while (true) {
    auto result = MessageCodec::Decoder::tryDecode(this->inputBuffer_, msg);

    if (result == MessageCodec::DecodeResult::Ok) {
      LOG_INFO("message decoded, fd=" << this->fd_
                                      << ", msg_size=" << msg.size());
      if (this->on_message_) {
        this->incPendingTasks();
        this->on_message_(this->shared_from_this(), msg);
      } else {
        LOG_WARN("message callback not set, fd=" << this->fd_);
      }
      continue;
    }

    if (result == MessageCodec::DecodeResult::NeedMoreData) {
      LOG_DEBUG("decode need more data, fd=" << this->fd_);
      break;
    }

    LOG_WARN("invalid packet, fd=" << this->fd_);
    this->setState(ConnState::Disconnected);
    return false;
  }

  return true;
}

bool Connection::handleWrite() {
  while (this->outputBuffer_.readableBytes() > 0) {

    ssize_t n = ::send(this->fd_, this->outputBuffer_.peek(),
                       this->outputBuffer_.readableBytes(), 0);
    if (n > 0) {
      this->refreshActivity();
      LOG_DEBUG("send success, fd=" << this->fd_ << ", bytes=" << n);
      this->outputBuffer_.retrieve(static_cast<size_t>(n));
    } else if (n < 0) {
      if (errno == EINTR) {
        continue;
      }
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        LOG_DEBUG("send would block, fd=" << this->fd_);
        return true;
      }
      LOG_ERROR("send failed, fd=" << this->fd_ << ", errno=" << errno
                                   << ", err=" << strerror(errno));
      this->setState(ConnState::Disconnected);
      return false;
    } else {
      LOG_WARN("send returned 0, fd=" << this->fd_);
      this->setState(ConnState::Disconnected);
      return false;
    }
  }

  if (this->canBeClosed()) {
    LOG_INFO("write buffer drained, closing disconnecting connection, fd="
             << this->fd_);
    this->setState(ConnState::Disconnected);
    return false;
  }

  LOG_DEBUG("write buffer drained, fd=" << this->fd_);
  return true;
}

void Connection::setMessageCallback(MessageCallback cb) {
  this->on_message_ = std::move(cb);
}

void Connection::sendPacket(const std::vector<char> &packet) {
  this->outputBuffer_.append(packet.data(), packet.size());
  LOG_DEBUG("packet queued to output buffer, fd="
            << this->fd_ << ", packet_size=" << packet.size()
            << ", pending_write=" << this->outputBuffer_.readableBytes());
}

void Connection::send(const std::string &data) {
  this->outputBuffer_.append(data);
  LOG_DEBUG("raw data queued to output buffer, fd="
            << this->fd_ << ", data_size=" << data.size());
}

bool Connection::wantWrite() const {
  return this->outputBuffer_.readableBytes() > 0;
}

Connection::ConnState Connection::state() const { return this->state_; }

void Connection::setState(Connection::ConnState st) {
  if (this->state_ != st) {
    LOG_INFO("connection state change, fd=" << this->fd_ << ", from="
                                            << static_cast<int>(this->state_)
                                            << ", to=" << static_cast<int>(st));
  }
  this->state_ = st;
}

void Connection::shutdown() {
  if (this->state_ == ConnState::Connected) {
    LOG_INFO("connection enter disconnecting, fd=" << this->fd_);
    this->state_ = ConnState::Disconnecting;
  }
}

bool Connection::shouldCloseAfterWrite() const {
  return this->state_ == ConnState::Disconnecting &&
         this->outputBuffer_.readableBytes() == 0;
}

bool Connection::isConnected() const {
  return this->state_ == Connection::ConnState::Connected;
}

bool Connection::isDisconnecting() const {
  return this->state_ == Connection::ConnState::Disconnecting;
}

bool Connection::isDisconnected() const {
  return this->state_ == Connection::ConnState::Disconnected;
}

void Connection::incPendingTasks() { ++this->pending_tasks_; }

void Connection::decPendingTasks() { --this->pending_tasks_; }

bool Connection::hasPendingTasks() const {
  return this->pending_tasks_.load() > 0;
}

bool Connection::canBeClosed() const {
  return this->state_ == ConnState::Disconnecting &&
         this->outputBuffer_.readableBytes() == 0 && !this->hasPendingTasks();
}

void Connection::refreshActivity() {
  using namespace std::chrono;
  uint64_t now =
      duration_cast<milliseconds>(steady_clock::now().time_since_epoch())
          .count();
  this->last_active_ms_.store(now);
}

uint64_t Connection::lastActiveMs() const {
  return this->last_active_ms_.load();
}
