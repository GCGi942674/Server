#include "EchoServer.h"
#include "utils.h" // 来自 common
#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <sys/eventfd.h>
#include <unistd.h>

EchoServer::EchoServer(int port, EchoHandler &handler)
    : acceptor_(&loop_, port), handler_(handler) {}

EchoServer::~EchoServer() {}

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
  this->acceptor_.setNewConnectionCallback(
      [this](int client_fd) { this->handleNewConnection(client_fd); });
  this->acceptor_.startListen();
  this->loop_.loop();
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

void EchoServer::handleNewConnection(int client_fd) {

  auto conn = std::make_unique<Connection>(client_fd);
  
  Connection *conn_ptr = conn.get();
  this->connections_.emplace(client_fd, std::move(conn));

  conn_ptr->setMessageCallback([this](int client_fd, const std::string &msg) {
    this->onMessage(client_fd, msg);
  });

  this->loop_.addFd(client_fd, EPOLLIN | EPOLLHUP,
                    [this, client_fd](uint32_t events) {
                      this->handleClientEvent(client_fd, events);
                    });
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