#ifndef MESSAGE_CODEC_H
#define MESSAGE_CODEC_H

#include "Buffer.h"
#include <cstdint>
#include <string>
#include <vector>

class MessageCodec {
public:
  static std::vector<char> encode(const std::string &msg);

  class Decoder {
  public:
    static bool tryDecode(Buffer &buf, std::string &out_msg);
  };
};

#endif