#include "EchoServer.h"
#include "Logging.h"
#include "Scope_guard.h"
#include "Utils.h" // 来自 common
#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <sys/eventfd.h>
#include <unistd.h>

EchoServer::EchoServer(int port, EchoHandler &handler, int signal_fd)
    : handler_(handler), acceptor_(&loop_, port), signal_fd(signal_fd) {
  LOG_INFO("EchoServer created, port=" << port);
}

EchoServer::~EchoServer() { LOG_INFO("EchoServer destroyed"); }

void EchoServer::onMessage(const std::shared_ptr<Connection> &conn,
                           const std::string &msg) {
  std::weak_ptr<Connection> weak_conn = conn;

  bool ok = this->pool_.addTask([this, weak_conn, msg]() -> void {
    bool is_heartbeat = (msg == "__ping__");
    uint64_t begin_us = getSteadyClockUs();

    if (is_heartbeat) {
      LOG_DEBUG("heartbeat pong queued");
    } else {
      LOG_DEBUG("normal response queued");
    }

    auto resp = this->handler_.onMessage(msg);
    auto packet = MessageCodec::encode(resp);

    this->metrics_.onResponseSent(is_heartbeat);

    uint64_t end_us = getSteadyClockUs();
    this->metrics_.onWorkerTaskCompleted(end_us - begin_us);

    this->loop_.queueInLoop([this, weak_conn, packet]() -> void {
      auto conn = weak_conn.lock();
      if (!conn) {
        LOG_WARN("Connection expired before sending response");
        return;
      }

      auto guard = finally([&] { conn->decPendingTasks(); });

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
      this->updateConnectionEvent(fd, conn->wantWrite());
      LOG_DEBUG("response queued back to loop, fd=" << fd << ", want_write="
                                                    << conn->wantWrite());

      if (conn->canBeClosed()) {
        this->removeConnection(fd, ServerMetrics::CloseReason::PeerClosed);
      }
    });
  });

  if (!ok) {
    conn->decPendingTasks();
    this->metrics_.onWorkerTaskRejected();
    LOG_WARN("thread pool shutting down, reject new task");
  } else {
    this->metrics_.onWorkerTaskSubmitted();
  }
}

void EchoServer::run() {
  LOG_INFO("EchoServer running");

  this->loop_.addFd(signal_fd, EPOLLIN, [this](uint32_t) {
    uint64_t val = 0;
    while (true) {
      ssize_t n = ::read(this->signal_fd, &val, sizeof(val));
      if (n > 0) {
        continue;
      }
      if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
        break;
      }
      if (n < 0 && errno == EINTR) {
        continue;
      }
      break;
    }
    LOG_INFO("signal received, begin, graceful shutdown");
    this->beginShutdown();
  });

  this->acceptor_.setNewConnectionCallback(
      [this](int client_fd) { this->handleNewConnection(client_fd); });

  if (!this->acceptor_.startListen()) {
    LOG_ERROR("acceptor startListen failed, quit server");
    this->loop_.quit();
    return;
  }

  this->idle_check_timer_ = this->loop_.runEvery(5000, [this]() {
    auto snap = this->metrics_.snapshot();

    double qps = 0.0;
    double in_bytes_per_sec = 0.0;
    double out_bytes_per_sec = 0.0;
    double avg_latency_us = 0.0;

    uint64_t delta_msg = 0;
    uint64_t delta_in = 0;
    uint64_t delta_out = 0;
    uint64_t delta_done = 0;
    uint64_t delta_latency = 0;
    uint64_t delta_accept = 0;
    uint64_t delta_close = 0;

    constexpr double interval_sec = kMetricIntervalSec;

    if (this->has_last_snapshot_) {
      delta_msg = snap.messages_received -
                  this->last_metrics_snapshot_.messages_received;
      delta_in =
          snap.bytes_received - this->last_metrics_snapshot_.bytes_received;
      delta_out = snap.bytes_sent - this->last_metrics_snapshot_.bytes_sent;
      delta_done = snap.worker_tasks_completed -
                   this->last_metrics_snapshot_.worker_tasks_completed;
      delta_latency = snap.business_latency_us_total -
                      this->last_metrics_snapshot_.business_latency_us_total;
      delta_accept = snap.total_connections_accepted -
                     this->last_metrics_snapshot_.total_connections_accepted;
      delta_close = snap.total_connections_closed -
                    this->last_metrics_snapshot_.total_connections_closed;

      qps = static_cast<double>(delta_msg) / interval_sec;
      in_bytes_per_sec = static_cast<double>(delta_in) / interval_sec;
      out_bytes_per_sec = static_cast<double>(delta_out) / interval_sec;

      avg_latency_us = (delta_done > 0) ? (static_cast<double>(delta_latency) /
                                           static_cast<double>(delta_done))
                                        : 0.0;
    }

    LOG_INFO("[METRIC_TOTAL] ..."
             << " conn_cur=" << snap.current_connections
             << " conn_total=" << snap.total_connections_accepted
             << " conn_closed=" << snap.total_connections_closed << " msg_recv="
             << snap.messages_received << " resp_sent=" << snap.responses_sent
             << " bytes_in=" << snap.bytes_received << " bytes_out="
             << snap.bytes_sent << " task_done=" << snap.worker_tasks_completed
             << " latency_max_us=" << snap.business_latency_us_max);

    LOG_INFO("[METRIC_WIN] ..."
             << " accept=" << delta_accept << " close=" << delta_close
             << " qps=" << qps << " in_Bps=" << in_bytes_per_sec << " out_Bps="
             << out_bytes_per_sec << " avg_us=" << avg_latency_us);

    this->last_metrics_snapshot_ = snap;
    this->has_last_snapshot_ = true;

    uint64_t now = getSteadyClockMs();

    std::vector<int> to_close;

    for (auto &[fd, conn] : this->connections_) {
      if (conn->isDisconnected()) {
        to_close.push_back(fd);
        continue;
      }

      if (now - conn->lastActiveMs() > this->idle_timeout_ms_) {
        LOG_INFO("connection idle timeout, fd=" << fd);
        conn->shutdown();
        this->updateConnectionEvent(fd, conn->wantWrite());

        if (conn->canBeClosed()) {
          to_close.push_back(fd);
        }
      }
    }

    for (int fd : to_close) {
      this->removeConnection(fd, ServerMetrics::CloseReason::IdleTimeout);
    }

    LOG_INFO(
        "timer heartbeat, current connections=" << this->connections_.size());
  });

  this->loop_.loop();
}

void EchoServer::beginShutdown() {
  if (this->stopping_.exchange(true)) {
    return;
  }

  LOG_INFO("begin graceful shutdown");

  this->acceptor_.stopListen();
  this->pool_.shutdown();

  for (auto &[fd, conn] : this->connections_) {
    conn->shutdown();
    this->updateConnectionEvent(fd, conn->wantWrite());
  }

  this->shutdown_timer_ =
      this->loop_.runAfter(this->shutdown_timeout_ms_, [this]() {
        LOG_WARN("graceful shutdown timeout, force closing all connections");

        std::vector<int> fds;
        for (auto &[fd, conn] : this->connections_) {
          fds.push_back(fd);
        }

        for (int fd : fds) {
          this->removeConnection(fd, ServerMetrics::CloseReason::ForceShutdown);
        }
      });

  this->tryFinishShutdown();
}

void EchoServer::tryFinishShutdown() {
  if (this->stopping_ == true && this->connections_.empty()) {
    if (this->shutdown_timer_ != 0) {
      this->loop_.cancelTimer(this->shutdown_timer_);
      this->shutdown_timer_ = 0;
    }

    LOG_INFO("all connections drained, stopping thread pool and quitting loop");
    this->pool_.stop();
    this->loop_.quit();
  }
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
    this->removeConnection(client_fd, ServerMetrics::CloseReason::PeerClosed);
    return;
  }

  if (events & (EPOLLERR | EPOLLHUP)) {
    LOG_WARN("epoll error/hup, remove connection, fd="
             << client_fd << ", events=" << events);
    this->removeConnection(client_fd, ServerMetrics::CloseReason::EpollError);
    return;
  }

  if (events & EPOLLIN) {
    auto rr = conn->handleRead();
    this->metrics_.onBytesReceived(rr.bytes_received);

    for (uint64_t i = 0; i < rr.messages_decoded; ++i) {
      this->metrics_.onMessageDecodedOk();
    }

    for (uint64_t i = 0; i < rr.heartbeat_messages; ++i) {
      this->metrics_.onMessageReceived(true);
    }

    uint64_t normal_msgs = rr.messages_decoded - rr.heartbeat_messages;
    for (uint64_t i = 0; i < normal_msgs; ++i) {
      this->metrics_.onMessageReceived(false);
    }

    if (rr.peer_close) {
      this->removeConnection(client_fd, ServerMetrics::CloseReason::PeerClosed);
      LOG_INFO("peer closed, remove connection, fd=" << client_fd);
      return;
    }

    if (rr.decode_error) {
      this->removeConnection(client_fd, ServerMetrics::CloseReason::ReadError);
      LOG_INFO("handleRead failed, remove connection, fd=" << client_fd);
      return;
    }

    if (!rr.ok) {
      this->removeConnection(client_fd, ServerMetrics::CloseReason::ReadError);
      LOG_INFO("handleRead failed, remove connection, fd=" << client_fd);
      return;
    }
  }

  if (events & EPOLLOUT) {
    auto wr = conn->handleWrite();
    this->metrics_.onBytesSent(wr.bytes_sent);
    if (!wr.ok) {
      LOG_INFO("handleWrite failed, remove connection, fd=" << client_fd);
      this->removeConnection(client_fd, ServerMetrics::CloseReason::WriteError);
      return;
    }

    if (wr.close) {
      this->removeConnection(client_fd, ServerMetrics::CloseReason::PeerClosed);
      return;
    }
  }

  if (events & EPOLLRDHUP) {
    LOG_INFO("peer rdhup, fd=" << client_fd);
    conn->shutdown();

    if (conn->canBeClosed()) {
      this->removeConnection(client_fd, ServerMetrics::CloseReason::PeerClosed);
      return;
    }
  }

  this->updateConnectionEvent(client_fd, iter->second->wantWrite());
}

void EchoServer::handleNewConnection(int client_fd) {
  if (this->stopping_) {
    LOG_INFO("server stopping, reject new connection, fd = " << client_fd);
    close(client_fd);
    return;
  }

  auto conn = std::make_shared<Connection>(client_fd);

  this->connections_.emplace(client_fd, conn);

  this->metrics_.onConnectionAccepted();

  conn->setMessageCallback(
      [this](const std::shared_ptr<Connection> &conn, const std::string &msg) {
        this->onMessage(conn, msg);
      });

  this->loop_.addFd(client_fd, EPOLLIN | EPOLLRDHUP,
                    [this, client_fd](uint32_t events) {
                      this->handleClientEvent(client_fd, events);
                    });
  LOG_DEBUG("new connection registered, fd=" << client_fd
                                             << ", total_connections="
                                             << this->connections_.size());
}

void EchoServer::removeConnection(int client_fd,
                                  ServerMetrics::CloseReason reason) {
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

  this->tryFinishShutdown();

  this->metrics_.onConnectionClosed(reason);
}

void EchoServer::updateConnectionEvent(int client_fd, bool want_write) {

  auto iter = this->connections_.find(client_fd);
  if (iter == this->connections_.end()) {
    return;
  }

  auto conn = iter->second;
  uint32_t events = EPOLLRDHUP;
  if (!this->stopping_ && conn->isConnected()) {
    events |= EPOLLIN;
  }

  if (want_write) {
    events |= EPOLLOUT;
  }

  this->loop_.updateFd(client_fd, events);
  LOG_DEBUG("connection events updated, fd=" << client_fd
                                             << ", want_write=" << want_write
                                             << ", events=" << events);
}