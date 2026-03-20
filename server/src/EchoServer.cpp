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

void EchoServer::onMessage(int fd, const std::string &msg) {

  LOG_INFO("dispatch message to worker, fd=" << fd
                                             << ", msg_size=" << msg.size());

  this->pool_.addTask([this, fd, msg]() -> void {
    LOG_DEBUG("worker handling message, fd=" << fd
                                             << ", msg_size=" << msg.size());

    auto resp = this->handler_.onMessage(msg);
    auto packet = MessageCodec::encode(resp);
    LOG_DEBUG("worker finished message, fd="
              << fd << ", resp_size=" << resp.size()
              << ", packet_size=" << packet.size());

    this->loop_.queueInLoop([this, fd, packet]() -> void {
      auto iter = this->connections_.find(fd);
      if (iter == this->connections_.end()) {
        LOG_WARN("connection not found when sending response, fd=" << fd);
        return;
      }
      if (!iter->second->isConnected()) {
        LOG_WARN(
            "connection already disconnected when sending response, fd=" << fd);
        return;
      }
      iter->second->sendPacket(packet);
      this->updateConnectionEvent(fd, iter->second->wantWrite());
      LOG_DEBUG("response queued back to loop, fd="
                << fd << ", want_write=" << iter->second->wantWrite());
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
  Connection *conn = iter->second.get();

  if (conn->isDisconnected()) {
    LOG_INFO("connection already disconnected before handling event, fd="
             << client_fd);
    this->removeConnection(client_fd);
    return;
  }

  if (events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
    LOG_WARN("connection got close/error event, fd=" << client_fd
                                                     << ", events=" << events);
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
  this->updateConnectionEvent(client_fd, iter->second->wantWrite());
}

void EchoServer::handleNewConnection(int client_fd) {

  auto conn = std::make_unique<Connection>(client_fd);

  Connection *conn_ptr = conn.get();
  this->connections_.emplace(client_fd, std::move(conn));

  conn_ptr->setMessageCallback([this](int client_fd, const std::string &msg) {
    this->onMessage(client_fd, msg);
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