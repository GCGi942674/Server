#include "protocol/message_codec.h"
#include <arpa/inet.h>
#include <cstring>

std::vector<char> MessageCodec::encode(const std::string &msg) {
  uint32_t len = static_cast<uint32_t>(msg.size());
  uint32_t net_len = ntohl(len);

  std::vector<char> result;
  result.resize(4 + len);
  std::memcpy(result.data() + 4, &net_len, 4);

  if (len > 0) {
    std::memcpy(result.data() + 4, msg.data(), len);
  }
  return result;
}

void MessageCodec::Decoder::append(const char *data, size_t len) {
  this->buffer_.append(data, len);
}

bool MessageCodec::Decoder::tryDecode(std::string &out_msg) {
  if (this->buffer_.size() < 4)
    return false;

  uint32_t net_len;
  std::memcpy(&net_len, this->buffer_.data(), 4);
  uint32_t body_len = ntohl(net_len);

  if (this->buffer_.size() < 4 + body_len)
    return false;

  out_msg.assign(this->buffer_.data() + 4, body_len);
  this->buffer_.erase(0, 4 + body_len);
  return true;
}

void MessageCodec::Decoder::reset(){
    this->buffer_.clear();
}