#include "EchoHandler.h"
#include "logging.h"

std::string EchoHandler::onMessage(const std::string &msg) {
  if (msg == "__ping__") {
    LOG_INFO("heartbeat ping received");
    return "__pong__";
  }
  return msg;
}