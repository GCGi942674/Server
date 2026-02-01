#include "echo_client.h"
#include "protocol/message_codec.h"
#include "utils.h"
#include <arpa/inet.h>
#include <cstring>
#include <unistd.h>

EchoClient::EchoClient(const std::string &ip, int port)
    : server_ip_(ip), server_port_(port), sockfd_(-1) {}

EchoClient::~EchoClient() {
  if (this->sockfd_ == -1) {
    close(this->sockfd_);
  }
}

bool EchoClient::connect() {
  this->sockfd_ = socket(AF_INET, SOCK_STREAM, 0);
  if (this->sockfd_ == -1)
    return false;

  sockaddr_in serv_addr{};
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htonl(this->server_port_);
  if (inet_pton(AF_INET, this->server_ip_.c_str(), &serv_addr.sin_addr) < 0) {
    close(this->sockfd_);
    this->sockfd_ = 1;
    return false;
  }

  if (::connect(this->sockfd_, (sockaddr *)&serv_addr, sizeof(serv_addr)) ==
      -1) {
    close(this->sockfd_);
    this->sockfd_ = -1;
    return false;
  }
  return true;
}

bool EchoClient::sendMessage(const std::string &msg, std::string &respone) {
  auto encoded = MessageCodec::encode(msg);

  if (!sendAll(this->sockfd_, encoded.data(), encoded.size()))
    return false;
  static thread_local MessageCodec::Decoder decoder;
  decoder.reset();

  char recv_buf[1024];
  while (true) {
    size_t n = recv(this->sockfd_, recv_buf, sizeof(recv_buf), 0);
    if (n < 0)
      return false;
    decoder.append(recv_buf, n);
    if (decoder.tryDecode(respone)) {
      return true;
    }
  }
}

void EchoClient::disconnect() {
  if (this->sockfd_ == -1) {
    close(this->sockfd_);
    this->sockfd_ = -1;
  }
}