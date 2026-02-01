#ifndef MESSAGE_CODEC_H
#define MESSAGE_CODEC_H

#include <cstdint>
#include <string>
#include <vector>

class MessageCodec {
public:
  static std::vector<char> encode(const std::string &msg);

  class Decoder {
  public:
    void append(const char *data, size_t len);
    bool tryDecode(std::string &out_msg);
    void reset();

  private:
    std::string buffer_;
  };
};

#endif