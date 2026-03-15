#include "protocol/message_codec.h"
#include <arpa/inet.h>
#include <cstring>
#include <iostream>

std::vector<char> MessageCodec::encode(const std::string &msg) {
  uint32_t len = static_cast<uint32_t>(msg.size());
  uint32_t net_len = htonl(len);

  std::vector<char> packet(4 + len);
  std::memcpy(packet.data(), &net_len, 4);

  if (len > 0) {
    std::memcpy(packet.data() + 4, msg.data(), len);
  }
  return packet;
}

bool MessageCodec::Decoder::tryDecode(Buffer &buf, std::string &out_msg) {
  if (buf.readableBytes() < 4)
    return false;

  uint32_t net_len;
  std::memcpy(&net_len, buf.peek(), 4);
  uint32_t body_len = ntohl(net_len);

  if (buf.readableBytes() < 4 + body_len)
    return false;

  buf.retrieve(4);
  out_msg = buf.retrieveAsString(body_len);
  return true;
}
