#include "EchoServer.h"

int main(){
  EchoHandler handler;
  EchoServer server(8080, handler);
  server.run();
  return 0;
}