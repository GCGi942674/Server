#ifndef ECHO_CLIENT_H
#define ECHO_CLIENT_H

#include "Buffer.h"
#include <string>

enum class ClientState { Disconnected, Connecting, Connected, Reconnecting };

class EchoClient {
public:
  EchoClient(const std::string &ip, int port);
  ~EchoClient();

  bool connect();
  void disconnect();
  bool sendMessage(const std::string &msg, std::string &response);

private:
  void markDisconnected();
  bool ensureConnected();
  bool writePakcet(const std::vector<char> & packet);
  bool readResponse(std::string& response);

private:
  std::string host_;
  int port_;
  int sockfd_{-1};

  ClientState state_{ClientState::Disconnected};

  int reconnect_delay_ms_{1000};
  int max_reconnect_delay_ms_{8000};
  int connect_timeout_ms_{3000};

  Buffer inputBuffer_;
};

#endif // ECHO_CLIENT_H