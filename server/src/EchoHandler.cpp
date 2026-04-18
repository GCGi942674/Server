#include "EchoHandler.h"
#include "Logging.h"

std::string EchoHandler::onMessage(const std::string &msg) {
  if (msg == "__ping__") {
    LOG_DEBUG("heartbeat ping received");
    return "__pong__";
  }
  return msg;
}