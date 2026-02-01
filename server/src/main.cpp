#include "echo_server.h"

int main(){
  EchoServer server(8080);
  server.run();
  return 0;
}