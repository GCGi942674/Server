#ifndef ECHO_CLIENT_H
#define ECHO_CLIENT_H

#include "Buffer.h"
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <string>
#include <thread>

enum class ClientState { Disconnected, Connecting, Connected, Reconnecting };

class EchoClient {
public:
  EchoClient(const std::string &ip, int port);
  ~EchoClient();

  bool connect();
  void disconnect();
  void resetConnection();
  bool sendMessage(const std::string &msg, std::string &response);

  bool sendPing();

  void startHeartbeat();
  void stopHeartbeat();

private:
  void markDisconnected();
  bool ensureConnected();
  bool writePacket(const std::vector<char> &packet);
  bool readResponse(std::string &response);

  void heartbeatLoop();
  bool sendPingUnlocked();

private:
  std::string host_;
  int port_;
  int sockfd_{-1};

  ClientState state_{ClientState::Disconnected};

  int reconnect_delay_ms_{1000};
  int max_reconnect_delay_ms_{8000};
  int connect_timeout_ms_{3000};

  Buffer inputBuffer_;

  std::mutex io_mutex_;
  std::condition_variable cv_;
  std::mutex cv_mutex_;
  // std::mutex state_mutex;

  std::atomic<bool> running_{false};
  std::atomic<bool> heartbeat_enabled_{false};
  std::thread heartbeat_thread_;

  int heartbeat_interval_ms_{20000};
  int heartbeat_fail_count_{0};
  int heartbeat_fail_threshold_{2};
};

#endif // ECHO_CLIENT_H