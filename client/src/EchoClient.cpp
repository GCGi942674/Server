#include "EchoClient.h"
#include "protocol/message_codec.h"
#include "utils.h"
#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <unistd.h>

EchoClient::EchoClient(const std::string &host, int port)
    : host_(host), port_(port), sockfd_(-1) {}

EchoClient::~EchoClient() { this->markDisconnected(); }

bool EchoClient::connect() {

  if (this->state_ == ClientState::Connected && this->sockfd_ != -1) {
    return true;
  }

  this->markDisconnected();
  this->state_ = ClientState::Connecting;

  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    this->state_ = ClientState::Disconnected;
    return false;
  }

  sockaddr_in serv_addr{};
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(this->port_);
  if (inet_pton(AF_INET, this->host_.c_str(), &serv_addr.sin_addr) <= 0) {
    close(fd);
    this->state_ = ClientState::Disconnected;
    return false;
  }

  if (::connect(fd, (sockaddr *)&serv_addr, sizeof(serv_addr)) == -1) {
    close(fd);
    this->state_ = ClientState::Disconnected;
    return false;
  }

  this->sockfd_ = fd;
  this->state_ = ClientState::Connected;
  this->reconnect_delay_ms_ = 1000;

  return true;
}

bool EchoClient::sendMessage(const std::string &msg, std::string &response) {

  for (int attempt = 0; attempt < 2; ++attempt) {
    if (!this->ensureConnected()) {
      return false;
    }

    auto encoded = MessageCodec::encode(msg);

    if (!this->writePakcet(encoded)) {
      continue;
    }

    if (!this->readResponse(response)) {
      continue;
    }
    return true;
  }
  return true;
}

void EchoClient::disconnect() { this->markDisconnected(); }

void EchoClient::markDisconnected() {
  if (this->sockfd_ != -1) {
    ::close(this->sockfd_);
    this->sockfd_ = -1;
  }
  this->state_ = ClientState::Disconnected;
}

bool EchoClient::ensureConnected() {
  if (this->state_ == ClientState::Connected && this->sockfd_ != -1) {
    return true;
  }
  return this->connect();
}

bool EchoClient::writePakcet(const std::vector<char> &packet) {
  if (this->sockfd_ == -1) {
    return false;
  }
  if (!sendAll(this->sockfd_, packet.data(), packet.size())) {
    this->markDisconnected();
    return false;
  }
  return true;
}

bool EchoClient::readResponse(std::string &response) {
  char recv_buf[1024];

  while (true) {
    auto result =
        MessageCodec::Decoder::tryDecode(this->inputBuffer_, response);
    if (result == MessageCodec::DecodeResult::Ok) {
      return true;
    }

    if (result == MessageCodec::DecodeResult::Invalid) {
      this->markDisconnected();
      return false;
    }

    ssize_t n = ::recv(this->sockfd_, recv_buf, sizeof(recv_buf), 0);
    if (n == 0) {
      this->markDisconnected();
      return false;
    }

    if (n < 0) {
      this->markDisconnected();
      return false;
    }

    this->inputBuffer_.append(recv_buf, static_cast<size_t>(n));
  }
}
