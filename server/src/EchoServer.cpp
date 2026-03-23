#include "EchoServer.h"
#include "logging.h"
#include "utils.h" // 来自 common
#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <sys/eventfd.h>
#include <unistd.h>

EchoServer::EchoServer(int port, EchoHandler &handler)
    : handler_(handler), acceptor_(&loop_, port) {
  LOG_INFO("EchoServer created, port=" << port);
}

EchoServer::~EchoServer() { LOG_INFO("EchoServer destroyed"); }

void EchoServer::onMessage(const std::shared_ptr<Connection> &conn,
                           const std::string &msg) {
  std::weak_ptr<Connection> weak_conn = conn;

  this->pool_.addTask([this, weak_conn, msg]() -> void {
    auto resp = this->handler_.onMessage(msg);
    auto packet = MessageCodec::encode(resp);

    this->loop_.queueInLoop([this, weak_conn, packet]() -> void {
      auto conn = weak_conn.lock();
      if (!conn) {
        LOG_WARN("Connection expired before sending response");
        return;
      }

      int fd = conn->fd();

      auto iter = this->connections_.find(fd);
      if (iter == this->connections_.end()) {
        LOG_WARN("connection not found when sending response, fd=" << fd);
        return;
      }

      if (iter->second != conn) {
        LOG_WARN("fd reused, skip stale connection response, fd=" << fd);
        return;
      }

      if (!conn->isConnected() && !conn->isDisconnecting()) {
        LOG_WARN("connection already full disconnected, fd=" << fd);
        return;
      }

      conn->sendPacket(packet);
      conn->decPendingTasks();
      this->updateConnectionEvent(fd, conn->wantWrite());
      LOG_DEBUG("response queued back to loop, fd=" << fd << ", want_write="
                                                    << conn->wantWrite());

      if (conn->canBeClosed()) {
        this->removeConnection(fd);
      }
    });
  });
}

void EchoServer::run() {
  LOG_INFO("EchoServer running");
  this->acceptor_.setNewConnectionCallback(
      [this](int client_fd) { this->handleNewConnection(client_fd); });
  this->acceptor_.startListen();
  this->loop_.loop();
}

void EchoServer::handleClientEvent(int client_fd, uint32_t events) {
  LOG_DEBUG("handle client event, fd=" << client_fd << ", events=" << events);
  auto iter = this->connections_.find(client_fd);
  if (iter == this->connections_.end()) {
    LOG_WARN("client event but connection not found, fd=" << client_fd);
    return;
  }
  std::shared_ptr<Connection> conn = iter->second;

  if (conn->isDisconnected()) {
    LOG_INFO("connection already disconnected before handling event, fd="
             << client_fd);
    this->removeConnection(client_fd);
    return;
  }

  if (events & (EPOLLERR | EPOLLHUP)) {
    LOG_WARN("epoll error/hup, remove connection, fd="
             << client_fd << ", events=" << events);
    this->removeConnection(client_fd);
    return;
  }

  if (events & EPOLLIN) {
    if (!conn->handleRead()) {
      LOG_INFO("handleRead failed, remove connection, fd=" << client_fd);
      this->removeConnection(client_fd);
      return;
    }
  }

  if (events & EPOLLOUT) {
    if (!conn->handleWrite()) {
      LOG_INFO("handleWrite failed, remove connection, fd=" << client_fd);
      this->removeConnection(client_fd);
      return;
    }
  }

  if (events & EPOLLRDHUP) {
    LOG_INFO("peer rdhup, fd=" << client_fd);
    conn->shutdown();

    if (conn->canBeClosed()) {
      this->removeConnection(client_fd);
      return;
    }
  }

  this->updateConnectionEvent(client_fd, iter->second->wantWrite());
}

void EchoServer::handleNewConnection(int client_fd) {

  auto conn = std::make_shared<Connection>(client_fd);

  this->connections_.emplace(client_fd, conn);

  conn->setMessageCallback(
      [this](const std::shared_ptr<Connection> &conn, const std::string &msg) {
        this->onMessage(conn, msg);
      });

  this->loop_.addFd(client_fd, EPOLLIN | EPOLLRDHUP,
                    [this, client_fd](uint32_t events) {
                      this->handleClientEvent(client_fd, events);
                    });
  LOG_INFO("new connection registered, fd="
           << client_fd << ", total_connections=" << this->connections_.size());
}

void EchoServer::removeConnection(int client_fd) {
  auto iter = this->connections_.find(client_fd);
  if (iter == this->connections_.end()) {
    LOG_WARN("removeConnection: fd not found, fd=" << client_fd);
    return;
  }
  iter->second->setState(Connection::ConnState::Disconnected);
  this->loop_.removeFd(client_fd);
  this->connections_.erase(client_fd);
  LOG_INFO("connection removed, fd=" << client_fd << ", total_connections="
                                     << this->connections_.size());
}

void EchoServer::updateConnectionEvent(int client_fd, bool want_write) {
  uint32_t events = EPOLLIN | EPOLLRDHUP;
  if (want_write) {
    events |= EPOLLOUT;
  }
  this->loop_.updateFd(client_fd, events);
  LOG_DEBUG("connection events updated, fd=" << client_fd
                                             << ", want_write=" << want_write
                                             << ", events=" << events);
}